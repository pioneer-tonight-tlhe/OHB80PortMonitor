#include "set_foup_in_auto_purge_enable_task.h"

#include "app/shareddata.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "ohbdeviceconfig.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QtGlobal>
#include <QVariantMap>

namespace {
constexpr const char *kCmdId = "WriteFoupInAutoPurgeEnable";
constexpr int kTotalTimeoutMs = 5000;

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
    return hexList.isEmpty() ? QStringLiteral("N/A") : hexList.join(QStringLiteral(" "));
}
} // namespace

SetFoupInAutoPurgeEnableTask::SetFoupInAutoPurgeEnableTask(const QVector<QString> &qrcodes,
                                                           int enableValue,
                                                           QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_enableValue(qBound(0, enableValue, 1))
    , deviceLogger("scheduler/set_foup_in_auto_purge_enable_task/detail")
{
    qDebug() << "[Scheduler][SetFoupInAutoPurgeEnableTask] create task qrcodes=" << qrcodes
             << "enableValue=" << m_enableValue;
}

SetFoupInAutoPurgeEnableTask::~SetFoupInAutoPurgeEnableTask()
{
    qDebug() << "[Scheduler][SetFoupInAutoPurgeEnableTask] destroy task";
}

void SetFoupInAutoPurgeEnableTask::start()
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
    m_targetQrCodes.clear();
    m_allFinishedEmitted = false;

    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_enableValue);
        emit finished(false, QStringLiteral("SetFoupInAutoPurgeEnableTask: qrcode list is empty"));
        return;
    }

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    CommandPool *pool = manager.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_enableValue);
        emit finished(false, QStringLiteral("SetFoupInAutoPurgeEnableTask: command '%1' not found").arg(kCmdId));
        return;
    }

    const QByteArray registerBytes = buildRegisterValue(static_cast<quint16>(m_enableValue));

    if (auto *opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetFoupInAutoPurgeEnable task started: enable=%1 for %2 devices")
                             .arg(m_enableValue)
                             .arg(m_qrcodes.size()),
                         0);
    }

    for (const QString &id : m_qrcodes) {
        m_targetQrCodes.append(id);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = manager.getMaster(id);
        if (!master || !master->isConnected() || !master->sender()) {
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("device unavailable"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto *opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        ModbusCommand cmd = pool->clone(kCmdId);
        if (!cmd.isValid()) {
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("clone command failed"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto *opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = registerBytes;
        cmd.request.byteCount = static_cast<quint8>(registerBytes.size());

        if (cmd.request.functionCode == 0x06
            && cmd.request.rawBytes.size() >= 6
            && registerBytes.size() >= 2) {
            cmd.request.rawBytes[4] = registerBytes[0];
            cmd.request.rawBytes[5] = registerBytes[1];
        }

        cmd.response.registerValue = registerBytes;
        if (cmd.response.rawBytes.size() >= 6 && registerBytes.size() >= 2) {
            cmd.response.rawBytes[4] = registerBytes[0];
            cmd.response.rawBytes[5] = registerBytes[1];
        }

        auto finishedConnection = connect(sender,
                                          &ModbusCommandSender::commandFinished,
                                          this,
                                          &SetFoupInAutoPurgeEnableTask::onCommandFinished,
                                          Qt::QueuedConnection);
        m_connections.append(finishedConnection);

        auto retryConnection = connect(sender,
                                       &ModbusCommandSender::commandTimeoutRetry,
                                       this,
                                       &SetFoupInAutoPurgeEnableTask::onCommandTimeoutRetry,
                                       Qt::QueuedConnection);
        m_connections.append(retryConnection);

        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        QMetaObject::invokeMethod(sender, [sender, cmd]() { sender->submit(cmd); }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        forceFinish();
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout, this, &SetFoupInAutoPurgeEnableTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

void SetFoupInAutoPurgeEnableTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetFoupInAutoPurgeEnableTask: cancelled"));
}

void SetFoupInAutoPurgeEnableTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped || !m_pendingMap.contains(cmd.uuid)) {
        return;
    }
    const QString qrCode = m_pendingMap.take(cmd.uuid);

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
        const QString responseTimeStr = cmd.responseMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString();

        QByteArray requestWithCrc = cmd.request.rawBytes;
        if (cmd.request.crc.size() >= 2) {
            requestWithCrc.append(cmd.request.crc);
        }

        QByteArray responseWithCrc = cmd.response.rawBytes;
        if (cmd.response.crc.size() >= 2) {
            responseWithCrc.append(cmd.response.crc);
        }

        db->insertRecord(sentTimeStr,
                         responseTimeStr,
                         cmd.id,
                         qrCode,
                         execStatus,
                         retryCount,
                         requestWithCrc,
                         responseWithCrc,
                         description,
                         UserPermission::Engineer);
    }

    const bool success = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;
    writeDeviceCommandLog(qrCode, cmd, success);

    if (success) {
        ++m_successCount;
    } else {
        if (!m_failedQrCodes.contains(qrCode)) {
            m_failedQrCodes.append(qrCode);
        }
        if (auto *opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, qrCode);
        }
    }

    checkAllFinished();
}

void SetFoupInAutoPurgeEnableTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped || !m_pendingMap.contains(cmd.uuid)) {
        return;
    }

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    emit deviceRetrying(qrCode, qMax(0, cmd.sendCount - 1), cmd.maxRetryCount);
}

void SetFoupInAutoPurgeEnableTask::onTimeout()
{
    qWarning() << "[Scheduler][SetFoupInAutoPurgeEnableTask] timeout, pending:" << m_pendingMap.size();
    forceFinish();
}

void SetFoupInAutoPurgeEnableTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) {
        return;
    }
    forceFinish();
}

void SetFoupInAutoPurgeEnableTask::forceFinish()
{
    if (m_allFinishedEmitted) {
        return;
    }
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }
    disconnectAll();

    OperationDispatchTask *opTaskPending = SharedData::getOperationDispatchTask();
    for (const QString &qrCode : m_pendingMap.values()) {
        if (!m_failedQrCodes.contains(qrCode)) {
            m_failedQrCodes.append(qrCode);
        }
        if (opTaskPending) {
            logFailedDevice(opTaskPending, qrCode);
        }
    }
    m_pendingMap.clear();

    QString persistErrorMessage;
    const bool persistSuccess = persistConfig(&persistErrorMessage);
    const bool allSuccess = m_failedQrCodes.isEmpty() && persistSuccess;
    setState(allSuccess ? Finished : Failed);

    if (auto *opTaskEnd = SharedData::getOperationDispatchTask()) {
        QString description;
        if (allSuccess) {
            description = QString("SetFoupInAutoPurgeEnable task completed: enable=%1, %2 devices succeeded")
                              .arg(m_enableValue)
                              .arg(m_successCount);
        } else if (!persistSuccess) {
            description = QString("SetFoupInAutoPurgeEnable task finished: config persistence failed (%1)")
                              .arg(persistErrorMessage);
        } else {
            description = QString("SetFoupInAutoPurgeEnable task finished: %1 succeeded, %2 failed")
                              .arg(m_successCount)
                              .arg(m_failedQrCodes.count());
        }
        opTaskEnd->log(allSuccess ? OperationDispatchTask::MsgType::Message
                                  : OperationDispatchTask::MsgType::Error,
                       description,
                       0);
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes, m_enableValue);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetFoupInAutoPurgeEnableTask: completed (%1 devices)").arg(m_successCount)
                      : (!persistSuccess
                             ? QString("SetFoupInAutoPurgeEnableTask: config persistence failed (%1)")
                                   .arg(persistErrorMessage)
                             : QString("SetFoupInAutoPurgeEnableTask: %1 succeeded, %2 failed")
                                   .arg(m_successCount)
                                   .arg(m_failedQrCodes.count())));
}

bool SetFoupInAutoPurgeEnableTask::persistConfig(QString *errorMessage)
{
    OHBDeviceConfig &config = OHBDeviceConfig::getInstance();
    QStringList persistFailedQrCodes;

    for (const QString &qrCode : qAsConst(m_targetQrCodes)) {
        if (m_failedQrCodes.contains(qrCode)) {
            continue;
        }

        if (!config.setFoupInAutoPurgeEnableByQRCode(qrCode, m_enableValue)) {
            persistFailedQrCodes.append(qrCode);
            if (!m_failedQrCodes.contains(qrCode)) {
                m_failedQrCodes.append(qrCode);
            }
        }
    }

    if (errorMessage) {
        if (persistFailedQrCodes.isEmpty()) {
            errorMessage->clear();
        } else {
            *errorMessage = QString("write FoupInAutoPurgeEnable to %1 failed, qrcodes=%2")
                                .arg(config.getConfigPath())
                                .arg(persistFailedQrCodes.join(", "));
        }
    }

    return persistFailedQrCodes.isEmpty();
}

QByteArray SetFoupInAutoPurgeEnableTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetFoupInAutoPurgeEnableTask::disconnectAll()
{
    for (const QMetaObject::Connection &connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

void SetFoupInAutoPurgeEnableTask::logFailedDevice(OperationDispatchTask *opTask, const QString &qrcode)
{
    const QString description = QString("[QRCode:%1]: SetFoupInAutoPurgeEnable failed, value=%2")
                                    .arg(qrcode)
                                    .arg(m_enableValue);
    opTask->log(OperationDispatchTask::MsgType::Error, description, 0);
}

void SetFoupInAutoPurgeEnableTask::writeDeviceSkipLog(const QString &qrCode,
                                                      const QString &commandId,
                                                      const QString &reason)
{
    deviceDetailLogger().info(
        QString("[SetFoupInAutoPurgeEnableTask][QRCode:%1] skip command\ncommand: %2\nreason: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SetFoupInAutoPurgeEnableTask::writeDeviceCommandLog(const QString &qrCode,
                                                         const ModbusCommand &cmd,
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

QString SetFoupInAutoPurgeEnableTask::commandFrameLogString(const ModbusCommand &cmd) const
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
            responseFrame = frameText == QStringLiteral("N/A")
                ? failureText
                : QString("%1, %2").arg(failureText, frameText);
        }
    }

    return QString("[SetFoupInAutoPurgeEnableTask] command finished\ncommand: %1\nrequest: %2\nresponse: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SetFoupInAutoPurgeEnableTask::subFunctionName() const
{
    return QStringLiteral("set_foup_in_auto_purge_enable");
}

QString SetFoupInAutoPurgeEnableTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/set_foup_in_auto_purge_enable_task/%1").arg(subFunctionName());
}

ILogger &SetFoupInAutoPurgeEnableTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
