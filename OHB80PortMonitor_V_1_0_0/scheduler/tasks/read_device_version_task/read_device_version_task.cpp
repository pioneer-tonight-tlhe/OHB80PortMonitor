#include "read_device_version_task.h"

#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "logdatabases/databasemanager.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaType>
#include <QSet>
#include <QVariantMap>

namespace {
constexpr const char* kReadVersionCommandId = "ReadVersion";
constexpr const char* kReadUIScreenVersionCommandId = "ReadUIScreenVersion";
constexpr int kCommandCountPerDevice = 2;
constexpr int kTotalTimeoutMs = 8000;

QString bytesToHexWithCrc(const QByteArray& bytes, const QByteArray& crc)
{
    QStringList hexList;
    for (int index = 0; index < bytes.size(); ++index) {
        hexList << QString("%1")
                       .arg(static_cast<quint8>(bytes.at(index)),
                            2,
                            16,
                            QLatin1Char('0'))
                       .toUpper();
    }

    if (crc.size() >= 2) {
        hexList << QString("%1")
                       .arg(static_cast<quint8>(crc.at(0)),
                            2,
                            16,
                            QLatin1Char('0'))
                       .toUpper();
        hexList << QString("%1")
                       .arg(static_cast<quint8>(crc.at(1)),
                            2,
                            16,
                            QLatin1Char('0'))
                       .toUpper();
    }

    return hexList.isEmpty() ? QStringLiteral("无") : hexList.join(QStringLiteral(" "));
}

QString commandResultText(const ModbusCommand& cmd)
{
    if (cmd.timedOut) {
        return QStringLiteral("timeout");
    }
    if (cmd.checksumError) {
        return QStringLiteral("checksum error");
    }
    if (cmd.deviceBusy) {
        return QStringLiteral("device busy");
    }
    if (!cmd.errorMessage.isEmpty()) {
        return cmd.errorMessage;
    }
    if (!cmd.received) {
        return QStringLiteral("not received");
    }
    return QStringLiteral("parse failed");
}
} // namespace

ReadDeviceVersionTask::ReadDeviceVersionTask(const QVector<QString>& qrCodes,
                                             QObject* parent)
    : SchedulerTask(parent)
    , m_qrCodes(qrCodes)
    , m_deviceLogger("scheduler/read_device_version_task/detail")
{
    static const bool s_metaTypesRegistered = []() {
        qRegisterMetaType<ReadDeviceVersionTask::DeviceVersionInfo>(
            "ReadDeviceVersionTask::DeviceVersionInfo");
        qRegisterMetaType<QList<ReadDeviceVersionTask::DeviceVersionInfo>>(
            "QList<ReadDeviceVersionTask::DeviceVersionInfo>");
        return true;
    }();
    Q_UNUSED(s_metaTypesRegistered)

    qDebug() << "[Scheduler][ReadDeviceVersionTask] create task qrcodes=" << qrCodes;
}

ReadDeviceVersionTask::~ReadDeviceVersionTask()
{
    qDebug() << "[Scheduler][ReadDeviceVersionTask] destroy task";
}

void ReadDeviceVersionTask::start()
{
    disconnectAll();

    setState(Running);
    m_stopped = false;
    m_pendingMap.clear();
    m_resultMap.clear();
    m_deviceFinishedCount.clear();
    m_deviceSuccessCount.clear();
    m_deviceCompletedMap.clear();
    m_connections.clear();
    m_totalDeviceCount = 0;
    m_completedDeviceCount = 0;
    m_successCount = 0;
    m_failedQrCodes.clear();
    m_allFinishedEmitted = false;

    if (m_qrCodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {});
        emit finished(false, QStringLiteral("ReadDeviceVersionTask: qrCode list is empty"));
        return;
    }

    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    CommandPool* pool = manager.commandPool();
    if (!pool
        || !pool->contains(QString::fromLatin1(kReadVersionCommandId))
        || !pool->contains(QString::fromLatin1(kReadUIScreenVersionCommandId))) {
        setState(Failed);
        emit allFinished(false, 0, {});
        emit finished(false, QStringLiteral("ReadDeviceVersionTask: required version command is missing"));
        return;
    }

    m_totalDeviceCount = m_qrCodes.size();
    for (const QString& qrCode : m_qrCodes) {
        initializeDeviceResult(qrCode);
    }

    for (const QString& qrCode : m_qrCodes) {
        ModbusTcpMaster* master = manager.getMaster(qrCode);
        if (!master) {
            markDeviceSkipped(qrCode, QStringLiteral("Master 不存在"));
            continue;
        }
        if (!master->isConnected()) {
            markDeviceSkipped(qrCode, QStringLiteral("设备未连接"));
            continue;
        }

        ModbusCommandSender* sender = master->sender();
        if (!sender) {
            markDeviceSkipped(qrCode, QStringLiteral("Sender 为空"));
            continue;
        }

        ModbusCommand readFirmwareVersionCommand = pool->clone(QString::fromLatin1(kReadVersionCommandId));
        ModbusCommand readUiScreenVersionCommand = pool->clone(QString::fromLatin1(kReadUIScreenVersionCommandId));
        if (!readFirmwareVersionCommand.isValid() || !readUiScreenVersionCommand.isValid()) {
            markDeviceSkipped(qrCode, QStringLiteral("克隆版本查询指令失败"));
            continue;
        }

        readFirmwareVersionCommand.module = CommandModule::BusinessCommandIssuer;
        readUiScreenVersionCommand.module = CommandModule::BusinessCommandIssuer;

        auto finishedConnection = connect(sender,
                                          &ModbusCommandSender::commandFinished,
                                          this,
                                          &ReadDeviceVersionTask::onCommandFinished,
                                          Qt::QueuedConnection);
        m_connections.append(finishedConnection);

        auto retryConnection = connect(sender,
                                       &ModbusCommandSender::commandTimeoutRetry,
                                       this,
                                       &ReadDeviceVersionTask::onCommandTimeoutRetry,
                                       Qt::QueuedConnection);
        m_connections.append(retryConnection);

        m_pendingMap.insert(readFirmwareVersionCommand.uuid,
                            PendingCommand{qrCode, VersionField::FirmwareVersion});
        m_pendingMap.insert(readUiScreenVersionCommand.uuid,
                            PendingCommand{qrCode, VersionField::UiScreenVersion});

        qDebug() << "[Scheduler][ReadDeviceVersionTask] send to device"
                 << qrCode
                 << kReadVersionCommandId
                 << kReadUIScreenVersionCommandId;

        QMetaObject::invokeMethod(sender,
                                  [sender,
                                   readFirmwareVersionCommand,
                                   readUiScreenVersionCommand]() {
                                      sender->submit(readFirmwareVersionCommand);
                                      sender->submit(readUiScreenVersionCommand);
                                  },
                                  Qt::QueuedConnection);
    }

    if (m_allFinishedEmitted) {
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer,
                &QTimer::timeout,
                this,
                &ReadDeviceVersionTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);

    checkAllFinished();
}

void ReadDeviceVersionTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("ReadDeviceVersionTask: cancelled"));
}

void ReadDeviceVersionTask::onCommandFinished(ModbusCommand cmd, const QString& masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) {
        return;
    }
    if (!m_pendingMap.contains(cmd.uuid)) {
        return;
    }

    const PendingCommand pending = m_pendingMap.take(cmd.uuid);
    const bool commandOk = cmd.received
                        && !cmd.timedOut
                        && !cmd.checksumError
                        && !cmd.deviceBusy;

    QString description = commandResultText(cmd);
    bool success = false;
    if (commandOk) {
        const QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        const QString version = (pending.field == VersionField::FirmwareVersion)
                                    ? parsedData.value(QStringLiteral("firmwareVersion")).toString()
                                    : parsedData.value(QStringLiteral("uiScreenVersion")).toString();
        if (!version.trimmed().isEmpty()) {
            updateDeviceVersionResult(pending.qrCode, pending.field, version);
            description = (pending.field == VersionField::FirmwareVersion)
                              ? QStringLiteral("firmwareVersion=%1").arg(version)
                              : QStringLiteral("uiScreenVersion=%1").arg(version);
            success = true;
        } else {
            cmd.errorMessage = (pending.field == VersionField::FirmwareVersion)
                                   ? QStringLiteral("firmware version parse failed")
                                   : QStringLiteral("ui screen version parse failed");
            description = cmd.errorMessage;
        }
    }

    writeCommunicateLog(pending.qrCode, cmd, description);
    writeDeviceCommandLog(pending.qrCode, cmd, success);
    markCommandFinished(pending.qrCode, success);
}

void ReadDeviceVersionTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString& masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) {
        return;
    }
    if (!m_pendingMap.contains(cmd.uuid)) {
        return;
    }

    const PendingCommand pending = m_pendingMap.value(cmd.uuid);
    emit deviceRetrying(pending.qrCode,
                        cmd.id,
                        qMax(0, cmd.sendCount - 1),
                        cmd.maxRetryCount);
}

void ReadDeviceVersionTask::onTimeout()
{
    if (m_stopped) {
        return;
    }

    qWarning() << "[Scheduler][ReadDeviceVersionTask] task timeout, pending command count="
               << m_pendingMap.size();
    forceFinish();
}

void ReadDeviceVersionTask::disconnectAll()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

void ReadDeviceVersionTask::initializeDeviceResult(const QString& qrCode)
{
    DeviceVersionInfo info;
    info.qrCode = qrCode;
    m_resultMap.insert(qrCode, info);
    m_deviceFinishedCount.insert(qrCode, 0);
    m_deviceSuccessCount.insert(qrCode, 0);
    m_deviceCompletedMap.insert(qrCode, false);
}

void ReadDeviceVersionTask::markDeviceSkipped(const QString& qrCode, const QString& reason)
{
    writeDeviceSkipLog(qrCode, QStringLiteral("ReadVersion/ReadUIScreenVersion"), reason);
    m_deviceFinishedCount[qrCode] = kCommandCountPerDevice;
    finalizeDeviceIfNeeded(qrCode);
}

void ReadDeviceVersionTask::markCommandFinished(const QString& qrCode, bool success)
{
    m_deviceFinishedCount[qrCode] = m_deviceFinishedCount.value(qrCode, 0) + 1;
    if (success) {
        m_deviceSuccessCount[qrCode] = m_deviceSuccessCount.value(qrCode, 0) + 1;
    }
    finalizeDeviceIfNeeded(qrCode);
}

void ReadDeviceVersionTask::finalizeDeviceIfNeeded(const QString& qrCode)
{
    if (m_deviceCompletedMap.value(qrCode, false)) {
        return;
    }
    if (m_deviceFinishedCount.value(qrCode, 0) < kCommandCountPerDevice) {
        return;
    }

    m_deviceCompletedMap[qrCode] = true;
    ++m_completedDeviceCount;

    if (m_resultMap.value(qrCode).allOk()) {
        ++m_successCount;
    } else if (!m_failedQrCodes.contains(qrCode)) {
        m_failedQrCodes.append(qrCode);
    }

    checkAllFinished();
}

void ReadDeviceVersionTask::checkAllFinished()
{
    if (m_completedDeviceCount < m_totalDeviceCount) {
        return;
    }
    forceFinish();
}

void ReadDeviceVersionTask::forceFinish()
{
    if (m_allFinishedEmitted) {
        return;
    }

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    disconnectAll();

    QSet<QString> pendingQrCodes;
    for (auto it = m_pendingMap.constBegin(); it != m_pendingMap.constEnd(); ++it) {
        pendingQrCodes.insert(it.value().qrCode);
    }
    for (const QString& qrCode : pendingQrCodes) {
        if (!m_deviceCompletedMap.value(qrCode, false)) {
            m_deviceFinishedCount[qrCode] = kCommandCountPerDevice;
            m_deviceCompletedMap[qrCode] = true;
            ++m_completedDeviceCount;
            if (!m_failedQrCodes.contains(qrCode)) {
                m_failedQrCodes.append(qrCode);
            }
        }
    }
    m_pendingMap.clear();

    for (const QString& qrCode : m_qrCodes) {
        if (!m_deviceCompletedMap.value(qrCode, false)) {
            m_deviceFinishedCount[qrCode] = kCommandCountPerDevice;
            m_deviceCompletedMap[qrCode] = true;
            ++m_completedDeviceCount;
            if (!m_failedQrCodes.contains(qrCode)) {
                m_failedQrCodes.append(qrCode);
            }
        }
    }

    m_allFinishedEmitted = true;

    QList<DeviceVersionInfo> results;
    results.reserve(m_qrCodes.size());
    for (const QString& qrCode : m_qrCodes) {
        results.append(m_resultMap.value(qrCode));
    }

    const bool allSuccess = (m_successCount == m_totalDeviceCount);
    setState(allSuccess ? Finished : Failed);

    emit allFinished(allSuccess, m_successCount, results);
    emit finished(allSuccess,
                  allSuccess
                      ? QStringLiteral("ReadDeviceVersionTask: %1/%2 devices succeeded")
                            .arg(m_successCount)
                            .arg(m_totalDeviceCount)
                      : QStringLiteral("ReadDeviceVersionTask: %1/%2 devices succeeded")
                            .arg(m_successCount)
                            .arg(m_totalDeviceCount));
}

void ReadDeviceVersionTask::updateDeviceVersionResult(const QString& qrCode,
                                                      VersionField field,
                                                      const QString& version)
{
    DeviceVersionInfo info = m_resultMap.value(qrCode);
    info.qrCode = qrCode;

    ModbusTcpMaster* master = ModbusTcpMasterManager::instance().getMaster(qrCode);
    if (field == VersionField::FirmwareVersion) {
        info.firmwareVersion = version;
        info.firmwareVersionOk = true;
        if (master) {
            master->setFirmwareVersion(version);
        }
    } else {
        info.uiScreenVersion = version;
        info.uiScreenVersionOk = true;
        if (master) {
            master->setUiScreenVersion(version);
        }
    }

    m_resultMap.insert(qrCode, info);
}

void ReadDeviceVersionTask::writeCommunicateLog(const QString& qrCode,
                                                const ModbusCommand& cmd,
                                                const QString& description) const
{
    LogDB::CommunicateLogDBCon* databaseConnection =
        LogDB::DatabaseManager::instance().communicateLogCon();
    if (!databaseConnection) {
        return;
    }

    const QString sendTime = (cmd.sentMs > 0)
                                 ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs)
                                       .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                 : QStringLiteral("-");
    const QString responseTime = (cmd.responseMs > 0)
                                     ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs)
                                           .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                                     : QString();

    int execStatus = 3;
    if (cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    } else if (cmd.sendCount > 1) {
        execStatus = 2;
    }

    QByteArray requestWithCrc = cmd.request.rawBytes;
    if (cmd.request.crc.size() >= 2) {
        requestWithCrc.append(cmd.request.crc);
    }

    QByteArray responseWithCrc = cmd.response.rawBytes;
    if (cmd.response.crc.size() >= 2) {
        responseWithCrc.append(cmd.response.crc);
    }

    databaseConnection->insertRecord(sendTime,
                                     responseTime,
                                     cmd.id,
                                     qrCode,
                                     execStatus,
                                     qMax(0, cmd.sendCount - 1),
                                     requestWithCrc,
                                     responseWithCrc,
                                     description.isEmpty() ? QStringLiteral("OK") : description,
                                     UserPermission::Engineer);
}

void ReadDeviceVersionTask::writeDeviceSkipLog(const QString& qrCode,
                                               const QString& commandId,
                                               const QString& reason)
{
    deviceDetailLogger().warn(
        QString("[ReadDeviceVersionTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void ReadDeviceVersionTask::writeDeviceCommandLog(const QString& qrCode,
                                                  const ModbusCommand& cmd,
                                                  bool success)
{
    const std::string message = QString("[QRCode:%1] %2")
                                    .arg(qrCode)
                                    .arg(commandFrameLogString(cmd))
                                    .toStdString();
    if (success) {
        deviceDetailLogger().info(message);
    } else {
        deviceDetailLogger().warn(message);
    }
}

QString ReadDeviceVersionTask::commandFrameLogString(const ModbusCommand& cmd) const
{
    QString responseFrame;
    if (cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy) {
        responseFrame = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
    } else {
        const QString failureText = commandResultText(cmd);
        if (!cmd.received) {
            responseFrame = failureText;
        } else {
            const QString frameText = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
            responseFrame = (frameText == QStringLiteral("无"))
                                ? failureText
                                : QStringLiteral("%1, %2").arg(failureText, frameText);
        }
    }

    return QString("[ReadDeviceVersionTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString ReadDeviceVersionTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/read_device_version_task/detail");
}

ILogger& ReadDeviceVersionTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        m_deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return m_deviceLogger;
}
