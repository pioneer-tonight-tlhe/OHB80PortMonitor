#include "set_humidity_offset_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QtGlobal>
#include <QVariantMap>
#include <cmath>

namespace {
constexpr const char *kCmdThreshold = "WriteHumidityOffsetThreshold";
constexpr const char *kCmdOffset = "WriteHumidityOffset";
constexpr int kTotalTimeoutMs = 8000;

QString bytesToHexWithCrc(const QByteArray& bytes, const QByteArray& crc)
{
    QStringList hexList;
    for (int i = 0; i < bytes.size(); ++i) {
        hexList << QString("%1").arg(static_cast<quint8>(bytes[i]), 2, 16, QLatin1Char('0')).toUpper();
    }
    if (crc.size() >= 2) {
        hexList << QString("%1").arg(static_cast<quint8>(crc[0]), 2, 16, QLatin1Char('0')).toUpper();
        hexList << QString("%1").arg(static_cast<quint8>(crc[1]), 2, 16, QLatin1Char('0')).toUpper();
    }
    return hexList.isEmpty() ? QStringLiteral("无") : hexList.join(QStringLiteral(" "));
}
} // namespace

SetHumidityOffsetTask::SetHumidityOffsetTask(const QVector<QString> &qrcodes, QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , deviceLogger("scheduler/set_humidity_offset_task/detail")
{
    qDebug() << "[Scheduler][SetHumidityOffsetTask] create task qrcodes=" << qrcodes;
}

SetHumidityOffsetTask::~SetHumidityOffsetTask()
{
    qDebug() << "[Scheduler][SetHumidityOffsetTask] destroy task";
}

void SetHumidityOffsetTask::setThreshold(double thresholdPct)
{
    m_thresholdSet = true;
    m_thresholdPct = thresholdPct;
}

void SetHumidityOffsetTask::setOffset(double offsetPct)
{
    m_offsetSet = true;
    m_offsetPct = offsetPct;
}

void SetHumidityOffsetTask::start()
{
    disconnectAll();

    setState(Running);
    m_stopped = false;
    m_totalDevices = 0;
    m_completedDevices.storeRelease(0);
    m_pendingMap.clear();
    m_pendingCommands.clear();
    m_connections.clear();
    m_deviceSuccessCount.clear();
    m_deviceFailed.clear();
    m_successCount = 0;
    m_failedQrCodes.clear();
    m_targetQrCodes.clear();
    m_allFinishedEmitted = false;

    m_subCmdPerDevice = (m_thresholdSet ? 1 : 0) + (m_offsetSet ? 1 : 0);
    if (m_subCmdPerDevice == 0) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_thresholdSet, m_thresholdPct, m_offsetSet, m_offsetPct);
        emit finished(false, QStringLiteral("SetHumidityOffsetTask: no threshold/offset command configured"));
        return;
    }

    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_thresholdSet, m_thresholdPct, m_offsetSet, m_offsetPct);
        emit finished(false, QStringLiteral("SetHumidityOffsetTask: qrcode list is empty"));
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool
        || (m_thresholdSet && !pool->contains(kCmdThreshold))
        || (m_offsetSet && !pool->contains(kCmdOffset))) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_thresholdSet, m_thresholdPct, m_offsetSet, m_offsetPct);
        emit finished(false, QStringLiteral("SetHumidityOffsetTask: required command is missing"));
        return;
    }

    auto pctToReg = [](double pct) -> quint16 {
        const double scaled = pct * kRegisterScale;
        const quint32 raw = static_cast<quint32>(std::round(scaled));
        return static_cast<quint16>(qBound<quint32>(0, raw, 0xFFFF));
    };

    const quint16 thresholdReg = m_thresholdSet ? pctToReg(m_thresholdPct) : 0;
    const quint16 offsetReg = m_offsetSet ? pctToReg(m_offsetPct) : 0;
    const QByteArray thresholdBytes = buildRegisterValue(thresholdReg);
    const QByteArray offsetBytes = buildRegisterValue(offsetReg);

    QStringList commandLines;
    if (m_thresholdSet) {
        commandLines << QString("%1: Threshold=%2%, register=%3")
                            .arg(kCmdThreshold)
                            .arg(m_thresholdPct)
                            .arg(thresholdReg);
    }
    if (m_offsetSet) {
        commandLines << QString("%1: Offset=%2%, register=%3")
                            .arg(kCmdOffset)
                            .arg(m_offsetPct)
                            .arg(offsetReg);
    }

    if (auto* opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetHumidityOffset task started: %1 for %2 devices")
                             .arg(commandLines.join("; "))
                             .arg(m_qrcodes.size()),
                         0);
    }

    auto fillCmd = [](ModbusCommand &cmd, const QByteArray &regBytes) {
        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = regBytes;
        cmd.request.byteCount = static_cast<quint8>(regBytes.size());

        if (cmd.request.functionCode == 0x06
            && cmd.request.rawBytes.size() >= 6
            && regBytes.size() >= 2) {
            cmd.request.rawBytes[4] = regBytes[0];
            cmd.request.rawBytes[5] = regBytes[1];
        }

        cmd.response.registerValue = regBytes;
        if (cmd.response.rawBytes.size() >= 6 && regBytes.size() >= 2) {
            cmd.response.rawBytes[4] = regBytes[0];
            cmd.response.rawBytes[5] = regBytes[1];
        }
    };

    for (const QString &id : m_qrcodes) {
        m_targetQrCodes.append(id);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master || !master->isConnected() || !master->sender()) {
            qWarning() << "[Scheduler][SetHumidityOffsetTask] device unavailable, skip:" << id;
            writeDeviceSkipLog(id, m_thresholdSet ? kCmdThreshold : kCmdOffset,
                               QStringLiteral("设备不可用"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetHumidityOffsetTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &SetHumidityOffsetTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        m_deviceSuccessCount[id] = 0;
        m_deviceFailed[id] = false;
        ++m_totalDevices;

        QVector<ModbusCommand> cmdsToSubmit;
        cmdsToSubmit.reserve(m_subCmdPerDevice);
        bool cloneOk = true;

        if (m_thresholdSet) {
            ModbusCommand cmd = pool->clone(kCmdThreshold);
            if (!cmd.isValid()) {
                qWarning() << "[Scheduler][SetHumidityOffsetTask] clone threshold command failed:" << id;
                writeDeviceSkipLog(id, kCmdThreshold, QStringLiteral("克隆指令失败"));
                cloneOk = false;
            } else {
                fillCmd(cmd, thresholdBytes);
                m_pendingMap[cmd.uuid] = Pending{id, CmdKind::Threshold};
                m_pendingCommands[cmd.uuid] = cmd;
                cmdsToSubmit.append(cmd);
            }
        }

        if (cloneOk && m_offsetSet) {
            ModbusCommand cmd = pool->clone(kCmdOffset);
            if (!cmd.isValid()) {
                qWarning() << "[Scheduler][SetHumidityOffsetTask] clone offset command failed:" << id;
                writeDeviceSkipLog(id, kCmdOffset, QStringLiteral("克隆指令失败"));
                cloneOk = false;
            } else {
                fillCmd(cmd, offsetBytes);
                m_pendingMap[cmd.uuid] = Pending{id, CmdKind::Offset};
                m_pendingCommands[cmd.uuid] = cmd;
                cmdsToSubmit.append(cmd);
            }
        }

        if (!cloneOk) {
            for (const ModbusCommand &cmd : cmdsToSubmit) {
                m_pendingMap.remove(cmd.uuid);
                m_pendingCommands.remove(cmd.uuid);
            }
            markDeviceFailed(id);
            continue;
        }

        qDebug() << "[Scheduler][SetHumidityOffsetTask] send to device" << id
                 << cmdsToSubmit.size() << "sub commands";

        QMetaObject::invokeMethod(sender, [sender, cmdsToSubmit]() {
            for (const ModbusCommand &cmd : cmdsToSubmit) {
                sender->submit(cmd);
            }
        }, Qt::QueuedConnection);
    }

    if (m_totalDevices == 0) {
        forceFinish();
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &SetHumidityOffsetTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

void SetHumidityOffsetTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetHumidityOffsetTask: cancelled"));
}

void SetHumidityOffsetTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;
    const Pending pending = m_pendingMap.take(cmd.uuid);
    m_pendingCommands.remove(cmd.uuid);

    {
        const QString sentTimeStr = cmd.sentMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-");
        int execStatus = 3;
        if (cmd.received) execStatus = 0;
        else if (cmd.timedOut) execStatus = 1;
        else if (cmd.sendCount > 1) execStatus = 2;

        const int retryCount = qMax(0, cmd.sendCount - 1);
        QString description;
        if (execStatus != 0) {
            description = cmd.errorMessage;
        } else {
            QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
            if (!parsedData.isEmpty()) {
                QStringList parts;
                for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                    parts << QString("%1=%2").arg(it.key(), it.value().toString());
                }
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

            QByteArray requestWithCrc = cmd.request.rawBytes;
            if (cmd.request.crc.size() >= 2) {
                requestWithCrc.append(cmd.request.crc);
            }

            QByteArray responseWithCrc = cmd.response.rawBytes;
            if (cmd.response.crc.size() >= 2) {
                responseWithCrc.append(cmd.response.crc);
            }

            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, pending.qrcode,
                             execStatus, retryCount,
                             requestWithCrc, responseWithCrc, description,
                             UserPermission::Engineer);
        }
    }

    const bool success = cmd.received
                      && !cmd.timedOut
                      && !cmd.checksumError
                      && !cmd.deviceBusy;

    writeDeviceCommandLog(pending.qrcode, cmd, success);

    if (success) {
        ++m_deviceSuccessCount[pending.qrcode];
        qDebug() << "[Scheduler][SetHumidityOffsetTask] device sub-command success:"
                 << pending.qrcode << cmd.id;
        tryMarkDeviceSuccess(pending.qrcode);
    } else {
        qWarning() << "[Scheduler][SetHumidityOffsetTask] device sub-command failed:"
                   << pending.qrcode << cmd.id
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        markDeviceFailed(pending.qrcode);
    }
}

void SetHumidityOffsetTask::tryMarkDeviceSuccess(const QString &qrcode)
{
    if (m_deviceFailed.value(qrcode, false)) return;
    if (m_deviceSuccessCount.value(qrcode, 0) < m_subCmdPerDevice) return;

    ++m_successCount;
    qDebug() << "[Scheduler][SetHumidityOffsetTask] device success:" << qrcode;
    checkAllFinished();
}

void SetHumidityOffsetTask::markDeviceFailed(const QString &qrcode)
{
    if (m_deviceFailed.value(qrcode, false)) return;

    m_deviceFailed[qrcode] = true;
    if (!m_failedQrCodes.contains(qrcode)) {
        m_failedQrCodes.append(qrcode);
    }

    qWarning() << "[Scheduler][SetHumidityOffsetTask] device failed:" << qrcode;
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        logFailedDevice(opTask, qrcode);
    }

    checkAllFinished();
}

void SetHumidityOffsetTask::checkAllFinished()
{
    const int done = m_completedDevices.fetchAndAddOrdered(1) + 1;
    if (done < m_totalDevices) return;

    forceFinish();
}

void SetHumidityOffsetTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const QString qrcode = m_pendingMap.value(cmd.uuid).qrcode;
    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;
    qDebug() << "[Scheduler][SetHumidityOffsetTask] command timeout retry:" << qrcode
             << retryCount << "/" << maxRetry;

    emit deviceRetrying(qrcode, retryCount, maxRetry);
}

void SetHumidityOffsetTask::onTimeout()
{
    qWarning() << "[Scheduler][SetHumidityOffsetTask] timeout, remaining"
               << m_pendingMap.size() << "sub commands";
    forceFinish();
}

void SetHumidityOffsetTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();

    auto* opTaskPending = SharedData::getOperationDispatchTask();
    for (auto it = m_pendingMap.constBegin(); it != m_pendingMap.constEnd(); ++it) {
        ModbusCommand timeoutCmd = m_pendingCommands.value(it.key());
        timeoutCmd.timedOut = true;
        timeoutCmd.received = false;
        timeoutCmd.errorMessage = QStringLiteral("任务超时，未收到指令完成回调");

        const QString qrCode = it.value().qrcode;
        writeDeviceCommandLog(qrCode, timeoutCmd, false);
        if (!m_deviceFailed.value(qrCode, false)) {
            m_deviceFailed[qrCode] = true;
            if (!m_failedQrCodes.contains(qrCode)) {
                m_failedQrCodes.append(qrCode);
            }
            if (opTaskPending) {
                logFailedDevice(opTaskPending, qrCode);
            }
        }
    }
    m_pendingMap.clear();
    m_pendingCommands.clear();
    const bool allSuccess = m_failedQrCodes.isEmpty();
    setState(allSuccess ? Finished : Failed);

    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        const QString desc = allSuccess
            ? QString("SetHumidityOffset task completed: %1 devices succeeded")
                  .arg(m_successCount)
            : QString("SetHumidityOffset task finished: %1 succeeded, %2 failed")
                  .arg(m_successCount)
                  .arg(m_failedQrCodes.count());
        opTaskEnd->log(allSuccess ? OperationDispatchTask::MsgType::Message
                                  : OperationDispatchTask::MsgType::Error,
                       desc, 0);
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes,
                     m_thresholdSet, m_thresholdPct,
                     m_offsetSet, m_offsetPct);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetHumidityOffsetTask: completed, %1 devices succeeded")
                            .arg(m_successCount)
                      : QString("SetHumidityOffsetTask: completed, %1 succeeded, %2 failed")
                            .arg(m_successCount)
                            .arg(m_failedQrCodes.count()));
}

void SetHumidityOffsetTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("SetHumidityOffset task failed: device %1").arg(qrcode);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetHumidityOffsetTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetHumidityOffsetTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

void SetHumidityOffsetTask::writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason)
{
    deviceDetailLogger().info(
        QString("[SetHumidityOffsetTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SetHumidityOffsetTask::writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success)
{
    const std::string msg = QString("[QRCode:%1] %2")
        .arg(qrCode)
        .arg(commandFrameLogString(cmd))
        .toStdString();
    if (success) {
        deviceDetailLogger().info(msg);
    } else {
        deviceDetailLogger().warn(msg);
    }
}

QString SetHumidityOffsetTask::commandFrameLogString(const ModbusCommand& cmd) const
{
    QString responseFrame;
    if (cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy) {
        responseFrame = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
    } else {
        QStringList failureReasons;
        if (cmd.timedOut) failureReasons << QStringLiteral("超时");
        if (cmd.checksumError) failureReasons << QStringLiteral("校验错误");
        if (cmd.deviceBusy) failureReasons << QStringLiteral("设备忙");
        if (!cmd.errorMessage.isEmpty()) failureReasons << cmd.errorMessage;

        const QString failureText = failureReasons.isEmpty()
            ? QStringLiteral("失败")
            : failureReasons.join(QStringLiteral(", "));
        if (!cmd.received) {
            responseFrame = failureText;
        } else {
            const QString frameText = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
            responseFrame = frameText == QStringLiteral("无")
                ? failureText
                : QString("%1, %2").arg(failureText, frameText);
        }
    }

    return QString("[SetHumidityOffsetTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SetHumidityOffsetTask::subFunctionName() const
{
    QStringList parts;
    if (m_thresholdSet) parts << QStringLiteral("threshold");
    if (m_offsetSet) parts << QStringLiteral("offset");
    return parts.isEmpty() ? QStringLiteral("unknown") : parts.join(QStringLiteral("_"));
}

QString SetHumidityOffsetTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/set_humidity_offset_task/%1").arg(subFunctionName());
}

QString SetHumidityOffsetTask::safeLogPathSegment(const QString& value)
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

ILogger& SetHumidityOffsetTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
