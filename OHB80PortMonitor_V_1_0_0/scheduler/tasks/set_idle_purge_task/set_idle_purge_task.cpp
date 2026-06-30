#include "set_idle_purge_task.h"

#include "app/shareddata.h"
#include "idlepurgeconfig.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaObject>
#include <QtGlobal>
#include <QVariantMap>

namespace {
QString bytesToHexWithCrc(const QByteArray &bytes, const QByteArray &crc)
{
    QStringList hexList;
    for (int i = 0; i < bytes.size(); ++i) {
        hexList << QString("%1")
                       .arg(static_cast<quint8>(bytes[i]), 2, 16, QLatin1Char('0'))
                       .toUpper();
    }

    if (crc.size() >= 2) {
        hexList << QString("%1")
                       .arg(static_cast<quint8>(crc[0]), 2, 16, QLatin1Char('0'))
                       .toUpper();
        hexList << QString("%1")
                       .arg(static_cast<quint8>(crc[1]), 2, 16, QLatin1Char('0'))
                       .toUpper();
    }

    return hexList.isEmpty() ? QStringLiteral("None")
                             : hexList.join(QStringLiteral(" "));
}
} // namespace

SetIdlePurgeTask::SetIdlePurgeTask(IdlePurgeProperty property,
                                   quint16 value,
                                   QObject *parent)
    : SchedulerTask(parent)
    , m_property(property)
    , m_value(value)
    , m_deviceLogger("scheduler/set_idle_purge_task/detail")
{
    qDebug() << "[Scheduler][SetIdlePurgeTask] create task"
             << propertyToString(property) << "=" << value;
}

SetIdlePurgeTask::~SetIdlePurgeTask()
{
    qDebug() << "[Scheduler][SetIdlePurgeTask] destroy task";
}

void SetIdlePurgeTask::start()
{
    disconnectAll();

    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_completedCount.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_successCount = 0;
    m_failedQrCodes.clear();
    m_allFinishedEmitted = false;

    const QString propertyName = propertyToString(m_property);
    const QString propertyValue = getValueText(m_value);
    const QString cmdId = getCommandIdForProperty(m_property);

    ModbusTcpMasterManager &masterManager = ModbusTcpMasterManager::instance();
    CommandPool *commandPool = masterManager.commandPool();
    if (!commandPool) {
        setState(Failed);
        emit allFinished(false, 0, {}, propertyName, m_value);
        emit finished(false, QStringLiteral("SetIdlePurgeTask: CommandPool not initialized"));
        return;
    }

    if (!commandPool->contains(cmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, propertyName, m_value);
        emit finished(false, QString("SetIdlePurgeTask: command '%1' not found").arg(cmdId));
        return;
    }

    const QStringList masterIds = masterManager.masterIds();
    if (masterIds.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, propertyName, m_value);
        emit finished(false, QStringLiteral("SetIdlePurgeTask: no target devices found"));
        return;
    }

    const QByteArray regValue = buildRegisterValue(m_value);

    if (auto *opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetIdlePurge task started: %1 = %2")
                             .arg(propertyName)
                             .arg(propertyValue),
                         0);
    }

    for (const QString &id : masterIds) {
        ModbusTcpMaster *master = masterManager.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] master does not exist:" << id;
            writeDeviceSkipLog(id, cmdId, QStringLiteral("Master not found"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto *opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        if (!master->isConnected()) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] device disconnected, skip:" << id;
            writeDeviceSkipLog(id, cmdId, QStringLiteral("Device disconnected"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto *opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] sender is null:" << id;
            writeDeviceSkipLog(id, cmdId, QStringLiteral("Sender is null"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto *opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommand cmd = commandPool->clone(cmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetIdlePurgeTask] clone command failed:" << id;
            writeDeviceSkipLog(id, cmdId, QStringLiteral("Clone command failed"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto *opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = regValue;
        cmd.request.byteCount = static_cast<quint8>(regValue.size());

        if (cmd.request.functionCode == 0x06
            && cmd.request.rawBytes.size() >= 6
            && regValue.size() >= 2) {
            cmd.request.rawBytes[4] = regValue[0];
            cmd.request.rawBytes[5] = regValue[1];
        }

        cmd.response.registerValue = regValue;
        if (cmd.response.rawBytes.size() >= 6 && regValue.size() >= 2) {
            cmd.response.rawBytes[4] = regValue[0];
            cmd.response.rawBytes[5] = regValue[1];
        }

        auto finishedConn = connect(sender, &ModbusCommandSender::commandFinished,
                                    this, &SetIdlePurgeTask::onCommandFinished,
                                    Qt::QueuedConnection);
        m_connections.append(finishedConn);

        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &SetIdlePurgeTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        qDebug() << "[Scheduler][SetIdlePurgeTask] send to device" << id
                 << cmdId << "realValue=" << propertyValue << "writeValue=" << m_value;

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        forceFinish();
    }
}

void SetIdlePurgeTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetIdlePurgeTask: cancelled"));
}

void SetIdlePurgeTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) {
        return;
    }
    if (!m_pendingMap.contains(cmd.uuid)) {
        return;
    }

    m_pendingMap.remove(cmd.uuid);

    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");
    int execStatus = 3;
    if (cmd.received) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    } else if (cmd.sendCount > 1) {
        execStatus = 2;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);
    QString description;
    if (execStatus != 0) {
        description = cmd.errorMessage;
    } else {
        const QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (!parsedData.isEmpty()) {
            QStringList parts;
            for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                parts << QString("%1=%2").arg(it.key(), it.value().toString());
            }
            description = parts.join(QStringLiteral(", "));
        }
    }
    if (description.isEmpty()) {
        description = QStringLiteral("OK");
    }

    if (LogDB::CommunicateLogDBCon *db = LogDB::DatabaseManager::instance().communicateLogCon()) {
        const QString respTimeStr = cmd.responseMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString();

        QByteArray requestWithCrc = cmd.request.rawBytes;
        if (!cmd.request.crc.isEmpty() && cmd.request.crc.size() >= 2) {
            requestWithCrc.append(cmd.request.crc);
        }

        QByteArray responseWithCrc = cmd.response.rawBytes;
        if (!cmd.response.crc.isEmpty() && cmd.response.crc.size() >= 2) {
            responseWithCrc.append(cmd.response.crc);
        }

        db->insertRecord(sentTimeStr,
                         respTimeStr,
                         cmd.id,
                         masterId,
                         execStatus,
                         retryCount,
                         requestWithCrc,
                         responseWithCrc,
                         description);
    }

    const bool success = cmd.received
                      && !cmd.timedOut
                      && !cmd.checksumError
                      && !cmd.deviceBusy;

    writeDeviceCommandLog(masterId, cmd, success);

    if (success) {
        ++m_successCount;
        qDebug() << "[Scheduler][SetIdlePurgeTask] device success:" << masterId;
    } else {
        if (!m_failedQrCodes.contains(masterId)) {
            m_failedQrCodes.append(masterId);
        }
        qWarning() << "[Scheduler][SetIdlePurgeTask] device failed:" << masterId
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;

        if (auto *opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, masterId);
        }
    }

    checkAllFinished();
}

void SetIdlePurgeTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) {
        return;
    }

    forceFinish();
}

void SetIdlePurgeTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) {
        return;
    }
    if (!m_pendingMap.contains(cmd.uuid)) {
        return;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;

    qDebug() << "[Scheduler][SetIdlePurgeTask] command timeout retry:" << masterId
             << retryCount << "/" << maxRetry;

    emit deviceRetrying(masterId, retryCount, maxRetry);
}

void SetIdlePurgeTask::forceFinish()
{
    if (m_allFinishedEmitted) {
        return;
    }
    m_allFinishedEmitted = true;

    disconnectAll();

    if (auto *opTaskPending = SharedData::getOperationDispatchTask()) {
        for (const QString &qrCode : m_pendingMap.values()) {
            if (!m_failedQrCodes.contains(qrCode)) {
                m_failedQrCodes.append(qrCode);
            }
            logFailedDevice(opTaskPending, qrCode);
        }
    } else {
        for (const QString &qrCode : m_pendingMap.values()) {
            if (!m_failedQrCodes.contains(qrCode)) {
                m_failedQrCodes.append(qrCode);
            }
        }
    }
    m_pendingMap.clear();

    const QString propertyName = propertyToString(m_property);
    const QString propertyValue = getValueText(m_value);
    const bool deviceWriteSuccess = m_failedQrCodes.isEmpty();
    const bool hasDeviceFailure = !deviceWriteSuccess;
    QString persistErrorMessage;
    bool persistSuccess = persistConfig(&persistErrorMessage);

    if (!persistSuccess) {
        qWarning() << "[Scheduler][SetIdlePurgeTask] config persistence failed:"
                   << propertyName << propertyValue << persistErrorMessage;
        deviceDetailLogger().warn(
            QString("[SetIdlePurgeTask] config persistence failed\n"
                    "property: %1\n"
                    "value: %2\n"
                    "reason: %3")
                .arg(propertyName)
                .arg(propertyValue)
                .arg(persistErrorMessage)
                .toStdString());
    }

    const bool finalSuccess = deviceWriteSuccess && persistSuccess;
    setState(finalSuccess ? Finished : Failed);

    if (auto *opTaskEnd = SharedData::getOperationDispatchTask()) {
        QString desc;
        OperationDispatchTask::MsgType msgType = OperationDispatchTask::MsgType::Message;

        if (finalSuccess) {
            desc = QString("SetIdlePurge %1=%2 task completed: %3 devices succeeded")
                       .arg(propertyName)
                       .arg(propertyValue)
                       .arg(m_successCount);
        } else if (hasDeviceFailure && persistSuccess) {
            desc = QString("SetIdlePurge %1=%2 task finished: %3 succeeded, %4 failed")
                       .arg(propertyName)
                       .arg(propertyValue)
                       .arg(m_successCount)
                       .arg(m_failedQrCodes.count());
            msgType = OperationDispatchTask::MsgType::Error;
        } else if (hasDeviceFailure) {
            desc = QString("SetIdlePurge %1=%2 task finished: %3 succeeded, %4 failed, config persistence failed (%5)")
                       .arg(propertyName)
                       .arg(propertyValue)
                       .arg(m_successCount)
                       .arg(m_failedQrCodes.count())
                       .arg(persistErrorMessage);
            msgType = OperationDispatchTask::MsgType::Error;
        } else {
            desc = QString("SetIdlePurge %1=%2 task finished: %3 devices succeeded, but config persistence failed (%4)")
                       .arg(propertyName)
                       .arg(propertyValue)
                       .arg(m_successCount)
                       .arg(persistErrorMessage);
            msgType = OperationDispatchTask::MsgType::Error;
        }

        opTaskEnd->log(msgType, desc, 0);
    }

    QString finishedMessage;
    if (finalSuccess) {
        finishedMessage = QString("SetIdlePurgeTask: %1 completed (%2 devices)")
                              .arg(propertyName)
                              .arg(m_successCount);
    } else if (hasDeviceFailure && persistSuccess) {
        finishedMessage = QString("SetIdlePurgeTask: %1 completed, %2 succeeded, %3 failed")
                              .arg(propertyName)
                              .arg(m_successCount)
                              .arg(m_failedQrCodes.count());
    } else if (hasDeviceFailure) {
        finishedMessage = QString("SetIdlePurgeTask: %1 completed, %2 succeeded, %3 failed, config persistence failed (%4)")
                              .arg(propertyName)
                              .arg(m_successCount)
                              .arg(m_failedQrCodes.count())
                              .arg(persistErrorMessage);
    } else {
        finishedMessage = QString("SetIdlePurgeTask: %1 device write succeeded, but config persistence failed (%2)")
                              .arg(propertyName)
                              .arg(persistErrorMessage);
    }

    emit allFinished(finalSuccess, m_successCount, m_failedQrCodes, propertyName, m_value);
    emit finished(finalSuccess, finishedMessage);
}

bool SetIdlePurgeTask::persistConfig(QString *errorMessage)
{
    IdlePurgeConfig &config = IdlePurgeConfig::getInstance();
    bool success = false;

    switch (m_property) {
    case IdlePurgeProperty::Enable:
        success = config.setEnabled(m_value != 0);
        break;
    case IdlePurgeProperty::PurgeTime:
        success = config.setPurgeDurationSeconds(static_cast<int>(m_value));
        break;
    case IdlePurgeProperty::PurgeInterval:
        success = config.setPurgeIntervalSeconds(static_cast<int>(m_value));
        break;
    }

    if (errorMessage) {
        if (success) {
            errorMessage->clear();
        } else {
            *errorMessage = QString("write %1 to %2 failed")
                                .arg(propertyToString(m_property))
                                .arg(config.getConfigPath());
        }
    }

    return success;
}

void SetIdlePurgeTask::logFailedDevice(OperationDispatchTask *opTask, const QString &qrCode)
{
    const QString desc = QString("[QRCode:%1]: SetIdlePurge %2=%3 task failed")
                             .arg(qrCode)
                             .arg(propertyToString(m_property))
                             .arg(getValueText(m_value));
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

void SetIdlePurgeTask::writeDeviceSkipLog(const QString &qrCode,
                                          const QString &commandId,
                                          const QString &reason)
{
    deviceDetailLogger().info(
        QString("[SetIdlePurgeTask][QRCode:%1] skip send\ncommand: %2\nreason: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SetIdlePurgeTask::writeDeviceCommandLog(const QString &qrCode,
                                             const ModbusCommand &cmd,
                                             bool success)
{
    const std::string msg = QString("[QRCode:%1] %2")
                                .arg(qrCode)
                                .arg(buildCommandFrameLogString(cmd))
                                .toStdString();
    if (success) {
        deviceDetailLogger().info(msg);
    } else {
        deviceDetailLogger().warn(msg);
    }
}

QString SetIdlePurgeTask::buildCommandFrameLogString(const ModbusCommand &cmd) const
{
    QString responseFrame;
    if (cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy) {
        responseFrame = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
    } else {
        QStringList failureReasons;
        if (cmd.timedOut) {
            failureReasons << QStringLiteral("timeout");
        }
        if (cmd.checksumError) {
            failureReasons << QStringLiteral("checksum error");
        }
        if (cmd.deviceBusy) {
            failureReasons << QStringLiteral("device busy");
        }
        if (!cmd.errorMessage.isEmpty()) {
            failureReasons << cmd.errorMessage;
        }

        const QString failureText = failureReasons.isEmpty()
            ? QStringLiteral("failed")
            : failureReasons.join(QStringLiteral(", "));
        if (!cmd.received) {
            responseFrame = failureText;
        } else {
            const QString frameText = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
            responseFrame = frameText == QStringLiteral("None")
                ? failureText
                : QString("%1, %2").arg(failureText, frameText);
        }
    }

    return QString("[SetIdlePurgeTask] command finished\n"
                   "command: %1\n"
                   "request: %2\n"
                   "response: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SetIdlePurgeTask::getSubFunctionName() const
{
    switch (m_property) {
    case IdlePurgeProperty::Enable:
        return QStringLiteral("set_idle_purge_enable");
    case IdlePurgeProperty::PurgeTime:
        return QStringLiteral("set_purge_duration");
    case IdlePurgeProperty::PurgeInterval:
        return QStringLiteral("set_purge_interval");
    }
    return QStringLiteral("unknown");
}

QString SetIdlePurgeTask::getDeviceLogPath() const
{
    return QStringLiteral("scheduler/set_idle_purge_task/%1").arg(getSubFunctionName());
}

QString SetIdlePurgeTask::buildSafeLogPathSegment(const QString &value)
{
    QString result = value.trimmed();
    if (result.isEmpty()) {
        return QStringLiteral("unknown");
    }

    const QString invalidChars = QStringLiteral("\\/:*?\"<>|");
    for (int i = 0; i < result.size(); ++i) {
        if (invalidChars.contains(result.at(i)) || result.at(i).unicode() < 0x20) {
            result[i] = QLatin1Char('_');
        }
    }

    if (result == QStringLiteral(".") || result == QStringLiteral("..")) {
        return QStringLiteral("unknown");
    }
    return result;
}

ILogger &SetIdlePurgeTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        m_deviceLogger.set_log_file(getDeviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return m_deviceLogger;
}

QString SetIdlePurgeTask::getCommandIdForProperty(IdlePurgeProperty property) const
{
    switch (property) {
    case IdlePurgeProperty::Enable:
        return QStringLiteral("WriteIdlePurgeEnable");
    case IdlePurgeProperty::PurgeTime:
        return QStringLiteral("WriteIdlePurgeTime");
    case IdlePurgeProperty::PurgeInterval:
        return QStringLiteral("WriteIdlePurgeInterval");
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
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

QString SetIdlePurgeTask::getValueText(quint16 value) const
{
    switch (m_property) {
    case IdlePurgeProperty::Enable:
        return (value == 1) ? QStringLiteral("enable")
                            : QStringLiteral("disable");
    case IdlePurgeProperty::PurgeTime:
    case IdlePurgeProperty::PurgeInterval:
        return QString("%1 s").arg(value);
    }
    return QString::number(value);
}

QString SetIdlePurgeTask::propertyToString(IdlePurgeProperty property)
{
    switch (property) {
    case IdlePurgeProperty::Enable:
        return QStringLiteral("Idle Purge Enable");
    case IdlePurgeProperty::PurgeTime:
        return QStringLiteral("Purge Duration");
    case IdlePurgeProperty::PurgeInterval:
        return QStringLiteral("Purge Interval");
    }
    return QStringLiteral("Unknown");
}
