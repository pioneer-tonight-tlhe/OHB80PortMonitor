#include "send_command_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "app/applogger.h"
#include "loggermanager.h"

#include <QDebug>
#include <QDateTime>

SendCommandTask::SendCommandTask(QObject *parent)
    : SchedulerTask(parent)
{
    qDebug() << "=============================SendCommandTask 调度任务开始=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================SendCommandTask 调度任务开始=============================");
}

SendCommandTask::~SendCommandTask()
{
    qDebug() << "=============================SendCommandTask 调度任务结束=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================SendCommandTask 调度任务结束=============================");
}

// ============================================================
// 配置接口
// ============================================================

void SendCommandTask::setSendToDevices(const QVector<QString> &qrcodes,
                                        const QString &commandName,
                                        const QVector<quint16> &params)
{
    m_targetQrcodes = qrcodes;
    m_commandName   = commandName;
    m_params        = params;
    m_sendToAll     = false;
}

void SendCommandTask::setSendToAll(const QString &commandName,
                                    const QVector<quint16> &params)
{
    m_commandName = commandName;
    m_params      = params;
    m_sendToAll   = true;
}

// ============================================================
// start()
// ============================================================

void SendCommandTask::start()
{
    setState(Running);
    m_stopped            = false;
    m_totalCount         = 0;
    m_completedCount.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_resultSuccessCount = 0;
    m_resultFailedIds.clear();

    if (m_commandName.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, "SendCommandTask: 未配置指令名");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN, "[Scheduler][SendCommandTask] 未配置指令名");
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, "SendCommandTask: CommandPool 未初始化");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN, "[Scheduler][SendCommandTask] CommandPool 未初始化");
        return;
    }

    if (!pool->contains(m_commandName)) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, QString("SendCommandTask: 指令模板 '%1' 不存在").arg(m_commandName));
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN, 
            QString("[Scheduler][SendCommandTask] 指令模板 '%1' 不存在").arg(m_commandName).toStdString());
        return;
    }

    // 确定目标 master ID 列表（master ID == QRCode）
    const QStringList targetIds = m_sendToAll
        ? mgr.masterIds()
        : QStringList(m_targetQrcodes.begin(), m_targetQrcodes.end());

    if (targetIds.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, "SendCommandTask: 没有找到目标设备");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN, "[Scheduler][SendCommandTask] 没有找到目标设备");
        return;
    }

    qDebug() << "[Scheduler][SendCommandTask] 目标设备数:" << targetIds.size();
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SendCommandTask] 目标设备数=%1").arg(targetIds.size()).toStdString());

    // 构建参数覆盖的 registerValue（大端序）
    const QByteArray overrideRegisterValue = buildRegisterValue(m_params);

    // 向每个目标设备发送指令并建立信号连接
    for (const QString &id : qAsConst(targetIds)) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][SendCommandTask] Master 不存在:" << id;
            m_resultFailedIds.append(id);
            continue;
        }

        // 检查设备是否已连接
        if (!master->isConnected()) {
            qWarning() << "[Scheduler][SendCommandTask] 设备未连接:" << id;
            m_resultFailedIds.append(id);
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            qWarning() << "[Scheduler][SendCommandTask] Sender 为空:" << id;
            m_resultFailedIds.append(id);
            continue;
        }

        // 从池中克隆指令副本（uuid 已生成，运行时状态已重置）
        ModbusCommand cmd = pool->clone(m_commandName);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SendCommandTask] 克隆指令失败:" << id;
            m_resultFailedIds.append(id);
            continue;
        }

        // 业务指令模块标记
        cmd.module = CommandModule::BusinessCommandIssuer;

        // 写指令（功能码 06/16）用参数覆盖 registerValue
        if (!overrideRegisterValue.isEmpty()) {
            cmd.request.registerValue = overrideRegisterValue;
            cmd.request.byteCount     = static_cast<quint8>(overrideRegisterValue.size());

            // FC 0x06 rawBytes = [SlaveAddr(1) + Function(1) + StartAddr(2) + RegisterValue(2)]
            // 需同步更新 rawBytes 中的 RegisterValue 部分，否则 buildRequestFrame 仍使用模板值
            if (cmd.request.functionCode == 0x06
                && cmd.request.rawBytes.size() >= 6
                && overrideRegisterValue.size() >= 2) {
                cmd.request.rawBytes[4] = overrideRegisterValue[0];
                cmd.request.rawBytes[5] = overrideRegisterValue[1];
            }
            // FC 0x06 响应为请求的镜像回显，需同步更新 response 以通过校验
            if (cmd.request.functionCode == 0x06
                && overrideRegisterValue.size() >= 2) {
                cmd.response.registerValue = overrideRegisterValue;
                if (cmd.response.rawBytes.size() >= 6) {
                    cmd.response.rawBytes[4] = overrideRegisterValue[0];
                    cmd.response.rawBytes[5] = overrideRegisterValue[1];
                }
            }
        }

        // 直接连接 commandFinished 信号（信号已携带 masterId）
        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SendCommandTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        // 记录 uuid 以便在回调中匹配
        m_pendingMap[cmd.uuid] = id;
        m_totalCount++;
        qDebug() << "[Scheduler][SendCommandTask] 向设备" << id << "发送指令" << m_commandName;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, 
            QString("[Scheduler][SendCommandTask] 向设备 %1 发送指令 %2").arg(id).arg(m_commandName).toStdString());

        // 提交指令（线程安全）
        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        disconnectAll();
        setState(Failed);
        emit allFinished(false, 0, m_resultFailedIds.count(), m_resultFailedIds);
        emit finished(false, QString("SendCommandTask: 所有设备无法接收指令（失败数=%1）")
                                .arg(m_resultFailedIds.count()));
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN, 
            QString("[Scheduler][SendCommandTask] 所有设备无法接收指令（失败数=%1）").arg(m_resultFailedIds.count()).toStdString());
    }
}

// ============================================================
// stop()
// ============================================================

void SendCommandTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "SendCommandTask: 任务被取消");
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, "[Scheduler][SendCommandTask] 任务被取消");
}

// ============================================================
// onCommandFinished() — 单设备指令完成回调
// ============================================================

void SendCommandTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) return;

    // 仅处理本任务发出的指令（通过 uuid 匹配）
    if (!m_pendingMap.contains(cmd.uuid)) return;
    m_pendingMap.remove(cmd.uuid);

    // 写入通讯日志
    {
        const QString sentTimeStr = cmd.sentMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-");
        int execStatus = 3;
        if (cmd.received)          execStatus = 0;
        else if (cmd.timedOut)     execStatus = 1;
        else if (cmd.sendCount > 1) execStatus = 2;
        const int retryCount = qMax(0, cmd.sendCount - 1);
        QString description;
        if (execStatus != 0) {
            description = cmd.errorMessage;
        } else {
            QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
            if (!parsedData.isEmpty()) {
                QStringList parts;
                for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it)
                    parts << QString("%1=%2").arg(it.key(), it.value().toString());
                description = parts.join(", ");
            }
        }
        if (description.isEmpty()) {
            description = QStringLiteral("OK");
        }
        if (LogDB::CommunicateLogDBCon *db = LogDB::DatabaseManager::instance().communicateLogCon()) {
            const QString respTimeStr = cmd.responseMs > 0
                ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                : QString();
            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, masterId,
                             execStatus, retryCount,
                             cmd.request.rawBytes, cmd.response.rawBytes, description);
        }
    }

    // 判断是否成功：received 为 true 且无错误
    const bool success = cmd.received
                      && !cmd.timedOut
                      && !cmd.checksumError
                      && !cmd.deviceBusy;

    if (success) {
        ++m_resultSuccessCount;
        emit dataResult(masterId, cmd);
        qDebug() << "[Scheduler][SendCommandTask] 设备" << masterId
                 << "指令" << cmd.id << "成功 响应字节="
                 << cmd.response.registerValue.toHex(' ');
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SendCommandTask] 设备 %1 指令 %2 成功 响应字节=%3").arg(masterId).arg(cmd.id).arg(QString::fromLatin1(cmd.response.registerValue.toHex(' '))).toStdString());
    } else {
        m_resultFailedIds.append(masterId);
        qWarning() << "[Scheduler][SendCommandTask] 设备" << masterId
                   << "指令" << cmd.id << "失败:" << cmd.errorMessage
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SendCommandTask] 设备 %1 指令 %2 失败: %3 timedOut=%4 checksumError=%5 deviceBusy=%6")
                .arg(masterId).arg(cmd.id).arg(cmd.errorMessage).arg(cmd.timedOut).arg(cmd.checksumError).arg(cmd.deviceBusy).toStdString());
    }

    checkAllFinished();
}

void SendCommandTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;

    disconnectAll();
    const bool allSuccess = m_resultFailedIds.isEmpty();
    setState(allSuccess ? Finished : Failed);

    emit allFinished(allSuccess, m_resultSuccessCount,
                     m_resultFailedIds.count(), m_resultFailedIds);

    emit finished(allSuccess,
                  allSuccess
                      ? QString("指令 '%1' 已对 %2 台设备执行完毕")
                            .arg(m_commandName).arg(done)
                      : QString("指令 '%1'：%2 台成功，%3 台失败")
                            .arg(m_commandName)
                            .arg(m_resultSuccessCount)
                            .arg(m_resultFailedIds.count()));
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), 
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][SendCommandTask] 指令 '%1' 执行完成: %2 台成功，%3 台失败")
            .arg(m_commandName).arg(m_resultSuccessCount).arg(m_resultFailedIds.count()).toStdString());
}

// ============================================================
// 内部辅助
// ============================================================

QByteArray SendCommandTask::buildRegisterValue(const QVector<quint16> &params) const
{
    QByteArray bytes;
    bytes.reserve(params.size() * 2);
    // 每个 quint16 按大端序拼接（符合 Modbus 标准）
    for (quint16 v : params) {
        bytes.append(static_cast<char>((v >> 8) & 0xFF));
        bytes.append(static_cast<char>(v & 0xFF));
    }
    return bytes;
}

void SendCommandTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}
