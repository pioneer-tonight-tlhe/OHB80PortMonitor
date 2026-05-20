#include "set_idle_purge_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "loggermanager.h"

#include <QTimer>
#include <QDebug>
#include <QDateTime>

// ============================================================
// 构造 / 析构
// ============================================================

SetIdlePurgeTask::SetIdlePurgeTask(IdlePurgeProperty property,
                                   quint16 value,
                                   QObject *parent)
    : SchedulerTask(parent)
    , m_property(property)
    , m_value(value)
{
    qDebug() << "[Scheduler][SetIdlePurgeTask] 创建任务："
             << propertyToString(property) << "=" << value;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetIdlePurgeTask] 创建任务：%1 = %2")
            .arg(propertyToString(property)).arg(value).toStdString());
}

SetIdlePurgeTask::~SetIdlePurgeTask()
{
    qDebug() << "[Scheduler][SetIdlePurgeTask] 任务销毁";
}

// ============================================================
// start()
// ============================================================

void SetIdlePurgeTask::start()
{
    setState(Running);
    m_stopped       = false;
    m_totalCount    = 0;
    m_completedCount.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_successCount  = 0;
    m_failedQrCodes.clear();
    m_allFinishedEmitted = false;

    const QString cmdId = commandIdForProperty(m_property);

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool) {
        setState(Failed);
        emit allFinished(false, 0, {}, propertyToString(m_property), m_value);
        emit finished(false, "SetIdlePurgeTask: CommandPool 未初始化");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetIdlePurgeTask] CommandPool 未初始化");
        return;
    }

    if (!pool->contains(cmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, propertyToString(m_property), m_value);
        emit finished(false, QString("SetIdlePurgeTask: 指令 '%1' 不存在").arg(cmdId));
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetIdlePurgeTask] 指令 '%1' 不存在").arg(cmdId).toStdString());
        return;
    }

    const QStringList masterIds = mgr.masterIds();
    if (masterIds.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, propertyToString(m_property), m_value);
        emit finished(false, "SetIdlePurgeTask: 没有找到目标设备");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetIdlePurgeTask] 没有找到目标设备");
        return;
    }

    const QByteArray regValue = buildRegisterValue(m_value);

    // 写入运行日志：任务启动
    auto* opTaskStart = SharedData::getOperationDispatchTask();
    if (opTaskStart) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetIdlePurge task started: %1 = %2")
                             .arg(propertyToString(m_property)).arg(m_value), 0);
    }

    for (const QString &id : masterIds) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] Master 不存在:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        // 未连接的设备直接跳过，不入队等待
        if (!master->isConnected()) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] 设备未连接，跳过:" << id;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][SetIdlePurgeTask] 设备 %1 未连接，跳过下发").arg(id).toStdString());
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] Sender 为空:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommand cmd = pool->clone(cmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] 克隆指令失败:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = regValue;
        cmd.request.byteCount     = static_cast<quint8>(regValue.size());

        // FC 0x06 rawBytes = [SlaveAddr(1) + Function(1) + StartAddr(2) + RegisterValue(2)]
        // 需同步更新 rawBytes 中的 RegisterValue 部分，否则 buildRequestFrame 仍使用模板值
        if (cmd.request.functionCode == 0x06
            && cmd.request.rawBytes.size() >= 6
            && regValue.size() >= 2) {
            cmd.request.rawBytes[4] = regValue[0];
            cmd.request.rawBytes[5] = regValue[1];
        }
        // FC 0x06 响应为请求的镜像回显，需同步更新 response.registerValue 以通过校验
        cmd.response.registerValue = regValue;
        // 同步更新 response.rawBytes 中的 RegisterValue 部分
        if (cmd.response.rawBytes.size() >= 6 && regValue.size() >= 2) {
            cmd.response.rawBytes[4] = regValue[0];
            cmd.response.rawBytes[5] = regValue[1];
        }

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetIdlePurgeTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        m_pendingMap[cmd.uuid] = id;
        m_totalCount++;

        qDebug() << "[Scheduler][SetIdlePurgeTask] 向设备" << id
                 << "发送" << cmdId << "值=" << m_value;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetIdlePurgeTask] 向设备 %1 发送 %2 值=%3")
                .arg(id).arg(cmdId).arg(m_value).toStdString());

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        forceFinish();
        return;
    }

    // 启动 5 秒整体超时，超时后未响应的设备全部标记为失败
    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &SetIdlePurgeTask::onTimeout);
    }
    m_timeoutTimer->start(5000);
}

// ============================================================
// stop()
// ============================================================

void SetIdlePurgeTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "SetIdlePurgeTask: 任务被取消");
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[Scheduler][SetIdlePurgeTask] 任务被取消");
}

// ============================================================
// onCommandFinished()
// ============================================================

void SetIdlePurgeTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) return;
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

    const bool success = cmd.received
                      && !cmd.timedOut
                      && !cmd.checksumError
                      && !cmd.deviceBusy;

    if (success) {
        ++m_successCount;
        qDebug() << "[Scheduler][SetIdlePurgeTask] 设备" << masterId << "设置成功";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetIdlePurgeTask] 设备 %1 设置成功").arg(masterId).toStdString());
    } else {
        m_failedQrCodes.append(masterId);
        qWarning() << "[Scheduler][SetIdlePurgeTask] 设备" << masterId
                   << "设置失败 timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetIdlePurgeTask] 设备 %1 设置失败: timedOut=%2 checksumError=%3 deviceBusy=%4")
                .arg(masterId).arg(cmd.timedOut).arg(cmd.checksumError).arg(cmd.deviceBusy).toStdString());
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, masterId);
        }
    }

    checkAllFinished();
}

// ============================================================
// 内部辅助
// ============================================================

void SetIdlePurgeTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;

    forceFinish();
}

void SetIdlePurgeTask::onTimeout()
{
    const int remaining = m_pendingMap.size();
    qWarning() << "[Scheduler][SetIdlePurgeTask] 5秒超时，剩余" << remaining << "台设备未响应，标记为失败";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][SetIdlePurgeTask] 5秒超时，剩余 %1 台设备未响应")
            .arg(remaining).toStdString());

    forceFinish();
}

void SetIdlePurgeTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    disconnectAll();

    // 超时仍未响应的设备：补登失败日志
    auto* opTaskPending = SharedData::getOperationDispatchTask();
    for (const QString &qrCode : m_pendingMap.values()) {
        m_failedQrCodes.append(qrCode);
        if (opTaskPending) {
            logFailedDevice(opTaskPending, qrCode);
        }
    }
    m_pendingMap.clear();

    const bool allSuccess = m_failedQrCodes.isEmpty();
    setState(allSuccess ? Finished : Failed);

    // 写入运行日志：任务汇总
    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        if (allSuccess) {
            const QString desc = QString("SetIdlePurge %1 task completed: %2 devices succeeded")
                  .arg(propertyToString(m_property)).arg(m_successCount);
            opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
        } else {
            const QString desc = QString("SetIdlePurge %1 task finished: %2 succeeded, %3 failed")
                  .arg(propertyToString(m_property)).arg(m_successCount).arg(m_failedQrCodes.count());
            opTaskEnd->log(OperationDispatchTask::MsgType::Error, desc, 0);
        }
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes,
                     propertyToString(m_property), m_value);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetIdlePurgeTask: %1 设置完毕（共 %2 台）")
                            .arg(propertyToString(m_property)).arg(m_successCount)
                      : QString("SetIdlePurgeTask: %1 设置完毕，%2 台成功，%3 台失败")
                            .arg(propertyToString(m_property))
                            .arg(m_successCount)
                            .arg(m_failedQrCodes.count()));

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][SetIdlePurgeTask] %1 设置完成: %2 台成功，%3 台失败")
            .arg(propertyToString(m_property)).arg(m_successCount)
            .arg(m_failedQrCodes.count()).toStdString());
}

void SetIdlePurgeTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("SetIdlePurge %1 task failed: device %2")
        .arg(propertyToString(m_property), qrcode);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QString SetIdlePurgeTask::commandIdForProperty(IdlePurgeProperty p) const
{
    switch (p) {
    case IdlePurgeProperty::Enable:       return QStringLiteral("WriteIdlePurgeEnable");
    case IdlePurgeProperty::PurgeTime:    return QStringLiteral("WriteIdlePurgeTime");
    case IdlePurgeProperty::PurgeInterval:return QStringLiteral("WriteIdlePurgeInterval");
    }
    return QString();
}

QByteArray SetIdlePurgeTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetIdlePurgeTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}

// ============================================================
// 静态方法
// ============================================================

QString SetIdlePurgeTask::propertyToString(IdlePurgeProperty p)
{
    switch (p) {
    case IdlePurgeProperty::Enable:        return QStringLiteral("Idle Purge Enable");
    case IdlePurgeProperty::PurgeTime:     return QStringLiteral("Purge Duration");
    case IdlePurgeProperty::PurgeInterval: return QStringLiteral("Purge Interval");
    }
    return QStringLiteral("Unknown");
}
