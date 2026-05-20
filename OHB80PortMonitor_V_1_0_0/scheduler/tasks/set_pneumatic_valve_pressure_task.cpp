#include "set_pneumatic_valve_pressure_task.h"
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

#include <QDebug>
#include <QDateTime>
#include <QTimer>
#include <cmath>

// ============================================================
// 构造 / 析构
// ============================================================

SetPneumaticValvePressureTask::SetPneumaticValvePressureTask(const QVector<QString> &qrcodes,
                                                             double pressureBar,
                                                             QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_pressureBar(pressureBar)
{
    qDebug() << "[Scheduler][SetPneumaticValvePressureTask] 创建任务："
             << "qrcodes=" << qrcodes
             << "pressure=" << pressureBar << "bar";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetPneumaticValvePressureTask] 创建任务：设备数=%1 压力=%2bar")
            .arg(qrcodes.size()).arg(pressureBar).toStdString());
}

SetPneumaticValvePressureTask::~SetPneumaticValvePressureTask()
{
    qDebug() << "[Scheduler][SetPneumaticValvePressureTask] 任务销毁";
}

// ============================================================
// start()
// ============================================================

void SetPneumaticValvePressureTask::start()
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

    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_pressureBar);
        emit finished(false, "SetPneumaticValvePressureTask: qrcode 列表为空");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetPneumaticValvePressureTask] qrcode 列表为空");
        return;
    }

    const QString cmdId = QStringLiteral("WritePneumaticValvePressure");

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_pressureBar);
        emit finished(false, "SetPneumaticValvePressureTask: CommandPool 未初始化");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetPneumaticValvePressureTask] CommandPool 未初始化");
        return;
    }

    if (!pool->contains(cmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_pressureBar);
        emit finished(false, QString("SetPneumaticValvePressureTask: 指令 '%1' 不存在").arg(cmdId));
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetPneumaticValvePressureTask] 指令 '%1' 不存在").arg(cmdId).toStdString());
        return;
    }

    // 压力值 × 10000 转换为寄存器值（quint16）
    const double scaled    = m_pressureBar * kRegisterScale;
    const quint32 raw      = static_cast<quint32>(std::round(scaled));
    const quint16 regVal   = static_cast<quint16>(qBound<quint32>(0, raw, 0xFFFF));
    const QByteArray regBytes = buildRegisterValue(regVal);

    qDebug() << "[Scheduler][SetPneumaticValvePressureTask] 压力转寄存器值："
             << m_pressureBar << "bar →" << regVal;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetPneumaticValvePressureTask] 压力 %1 bar 转寄存器值 %2 (0x%3)")
            .arg(m_pressureBar).arg(regVal)
            .arg(QString::number(regVal, 16).toUpper()).toStdString());

    // 写入运行日志：任务启动
    auto* opTaskStart = SharedData::getOperationDispatchTask();
    if (opTaskStart) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetPneumaticValvePressure task started: %1 bar for %2 devices")
                             .arg(m_pressureBar).arg(m_qrcodes.size()), 0);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][SetPneumaticValvePressureTask] Master 不存在:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        if (!master->isConnected()) {
            qWarning() << "[Scheduler][SetPneumaticValvePressureTask] 设备未连接，跳过:" << id;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][SetPneumaticValvePressureTask] 设备 %1 未连接，跳过下发").arg(id).toStdString());
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            qWarning() << "[Scheduler][SetPneumaticValvePressureTask] Sender 为空:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommand cmd = pool->clone(cmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetPneumaticValvePressureTask] 克隆指令失败:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = regBytes;
        cmd.request.byteCount     = static_cast<quint8>(regBytes.size());

        // FC 0x06 rawBytes 同步更新寄存器值部分
        if (cmd.request.functionCode == 0x06
            && cmd.request.rawBytes.size() >= 6
            && regBytes.size() >= 2) {
            cmd.request.rawBytes[4] = regBytes[0];
            cmd.request.rawBytes[5] = regBytes[1];
        }
        // FC 0x06 响应为请求镜像回显，同步更新 response
        cmd.response.registerValue = regBytes;
        if (cmd.response.rawBytes.size() >= 6 && regBytes.size() >= 2) {
            cmd.response.rawBytes[4] = regBytes[0];
            cmd.response.rawBytes[5] = regBytes[1];
        }

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetPneumaticValvePressureTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        m_pendingMap[cmd.uuid] = id;
        m_totalCount++;

        qDebug() << "[Scheduler][SetPneumaticValvePressureTask] 向设备" << id
                 << "发送" << cmdId << "寄存器值=" << regVal;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetPneumaticValvePressureTask] 向设备 %1 发送 %2 寄存器值=%3")
                .arg(id).arg(cmdId).arg(regVal).toStdString());

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        forceFinish();
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &SetPneumaticValvePressureTask::onTimeout);
    }
    m_timeoutTimer->start(5000);
}

// ============================================================
// stop()
// ============================================================

void SetPneumaticValvePressureTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "SetPneumaticValvePressureTask: 任务被取消");
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[Scheduler][SetPneumaticValvePressureTask] 任务被取消");
}

// ============================================================
// onCommandFinished()
// ============================================================

void SetPneumaticValvePressureTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
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
            qDebug() << "[Scheduler][SetPneumaticValvePressureTask] 插入通讯日志: sendTime=" << sentTimeStr
                     << "respTime=" << respTimeStr << "commandId=" << cmd.id << "masterId=" << masterId
                     << "execStatus=" << execStatus << "retryCount=" << retryCount
                     << "sendFrame=" << cmd.request.rawBytes.toHex() << "respFrame=" << cmd.response.rawBytes.toHex()
                     << "description=" << description;
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
        qDebug() << "[Scheduler][SetPneumaticValvePressureTask] 设备" << masterId << "设置成功";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetPneumaticValvePressureTask] 设备 %1 设置成功").arg(masterId).toStdString());
    } else {
        m_failedQrCodes.append(masterId);
        qWarning() << "[Scheduler][SetPneumaticValvePressureTask] 设备" << masterId
                   << "设置失败 timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetPneumaticValvePressureTask] 设备 %1 设置失败: timedOut=%2 checksumError=%3 deviceBusy=%4")
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

void SetPneumaticValvePressureTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;
    forceFinish();
}

void SetPneumaticValvePressureTask::onTimeout()
{
    const int remaining = m_pendingMap.size();
    qWarning() << "[Scheduler][SetPneumaticValvePressureTask] 5秒超时，剩余" << remaining << "台设备未响应，标记为失败";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][SetPneumaticValvePressureTask] 5秒超时，剩余 %1 台设备未响应")
            .arg(remaining).toStdString());
    forceFinish();
}

void SetPneumaticValvePressureTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) m_timeoutTimer->stop();
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
            const QString desc = QString("SetPneumaticValvePressure %1 bar task completed: %2 devices succeeded")
                  .arg(m_pressureBar).arg(m_successCount);
            opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
        } else {
            const QString desc = QString("SetPneumaticValvePressure %1 bar task finished: %2 succeeded, %3 failed")
                  .arg(m_pressureBar).arg(m_successCount).arg(m_failedQrCodes.count());
            opTaskEnd->log(OperationDispatchTask::MsgType::Error, desc, 0);
        }
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes, m_pressureBar);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetPneumaticValvePressureTask: 压力 %1 bar 设置完成（%2 台）")
                            .arg(m_pressureBar).arg(m_successCount)
                      : QString("SetPneumaticValvePressureTask: 压力 %1 bar 设置完成，%2 台成功，%3 台失败")
                            .arg(m_pressureBar).arg(m_successCount).arg(m_failedQrCodes.count()));

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][SetPneumaticValvePressureTask] 压力 %1 bar 设置完成: %2 台成功，%3 台失败")
            .arg(m_pressureBar).arg(m_successCount).arg(m_failedQrCodes.count()).toStdString());
}

void SetPneumaticValvePressureTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("SetPneumaticValvePressure %1 bar task failed: device %2")
        .arg(m_pressureBar).arg(qrcode);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetPneumaticValvePressureTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetPneumaticValvePressureTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}
