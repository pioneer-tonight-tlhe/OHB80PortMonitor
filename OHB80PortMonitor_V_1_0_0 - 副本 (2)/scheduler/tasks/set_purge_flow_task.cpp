#include "set_purge_flow_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task.h"

#include <QDateTime>
#include <QDebug>
#include <QtGlobal>
#include <QVariantMap>

namespace {
constexpr const char *kCmdId = "WritePurgeFlow";

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

SetPurgeFlowTask::SetPurgeFlowTask(const QVector<QString> &qrcodes,
                                   int flowValue,
                                   QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_flowValue(flowValue)
    , deviceLogger("scheduler/set_purge_flow_task/detail")
{
    qDebug() << "[Scheduler][SetPurgeFlowTask] create task qrcodes=" << qrcodes
             << "flow=" << flowValue;
}

SetPurgeFlowTask::~SetPurgeFlowTask()
{
    qDebug() << "[Scheduler][SetPurgeFlowTask] destroy task";
}

void SetPurgeFlowTask::start()
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
        emit allFinished(false, 0, {}, m_flowValue);
        emit finished(false, QStringLiteral("SetPurgeFlowTask: qrcode list is empty"));
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_flowValue);
        emit finished(false, QStringLiteral("SetPurgeFlowTask: command '%1' not found").arg(kCmdId));
        return;
    }

    const qint64 raw = static_cast<qint64>(m_flowValue) * kRegisterScale;
    const quint16 regVal = static_cast<quint16>(qBound<qint64>(0, raw, 0xFFFF));
    const QByteArray regBytes = buildRegisterValue(regVal);

    if (auto* opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetPurgeFlow task started: flow=%1 L/Min for %2 devices")
                             .arg(m_flowValue)
                             .arg(m_qrcodes.size()),
                         0);
    }

    for (const QString &id : m_qrcodes) {
        m_targetQrCodes.append(id);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master || !master->isConnected() || !master->sender()) {
            qWarning() << "[Scheduler][SetPurgeFlowTask] device unavailable, skip:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("设备不可用"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        ModbusCommand cmd = pool->clone(kCmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetPurgeFlowTask] clone command failed:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("克隆指令失败"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

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

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetPurgeFlowTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &SetPurgeFlowTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        qDebug() << "[Scheduler][SetPurgeFlowTask] send to device" << id
                 << kCmdId << "realValue=" << m_flowValue << "writeValue=" << regVal;

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        forceFinish();
    }
}

void SetPurgeFlowTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetPurgeFlowTask: cancelled"));
}

void SetPurgeFlowTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;
    const QString qrCode = m_pendingMap.take(cmd.uuid);

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

            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, qrCode,
                             execStatus, retryCount,
                             requestWithCrc, responseWithCrc, description);
        }
    }

    const bool success = cmd.received
                      && !cmd.timedOut
                      && !cmd.checksumError
                      && !cmd.deviceBusy;

    writeDeviceCommandLog(qrCode, cmd, success);

    if (success) {
        ++m_successCount;
        qDebug() << "[Scheduler][SetPurgeFlowTask] device success:" << qrCode;
    } else {
        if (!m_failedQrCodes.contains(qrCode)) {
            m_failedQrCodes.append(qrCode);
        }
        qWarning() << "[Scheduler][SetPurgeFlowTask] device failed:" << qrCode
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;

        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, qrCode);
        }
    }

    checkAllFinished();
}

void SetPurgeFlowTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;

    forceFinish();
}

void SetPurgeFlowTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;

    qDebug() << "[Scheduler][SetPurgeFlowTask] command timeout retry:" << qrCode
             << retryCount << "/" << maxRetry;

    emit deviceRetrying(qrCode, retryCount, maxRetry);
}

void SetPurgeFlowTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    disconnectAll();

    auto* opTaskPending = SharedData::getOperationDispatchTask();
    for (const QString &qrCode : m_pendingMap.values()) {
        if (!m_failedQrCodes.contains(qrCode)) {
            m_failedQrCodes.append(qrCode);
        }
        if (opTaskPending) {
            logFailedDevice(opTaskPending, qrCode);
        }
    }
    m_pendingMap.clear();

    const bool allSuccess = m_failedQrCodes.isEmpty();
    setState(allSuccess ? Finished : Failed);

    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        const QString desc = allSuccess
            ? QString("SetPurgeFlow flow=%1 L/Min task completed: %2 devices succeeded")
                  .arg(m_flowValue)
                  .arg(m_successCount)
            : QString("SetPurgeFlow flow=%1 L/Min task finished: %2 succeeded, %3 failed")
                  .arg(m_flowValue)
                  .arg(m_successCount)
                  .arg(m_failedQrCodes.count());
        opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes, m_flowValue);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetPurgeFlowTask: flow=%1 completed (%2 devices)")
                            .arg(m_flowValue).arg(m_successCount)
                      : QString("SetPurgeFlowTask: flow=%1 completed, %2 succeeded, %3 failed")
                            .arg(m_flowValue).arg(m_successCount).arg(m_failedQrCodes.count()));
}

void SetPurgeFlowTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("[QRCode:%1]: SetPurgeFlow flow=%2 L/Min task failed")
        .arg(qrcode)
        .arg(m_flowValue);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetPurgeFlowTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetPurgeFlowTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

void SetPurgeFlowTask::writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason)
{
    deviceDetailLogger().info(
        QString("[SetPurgeFlowTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SetPurgeFlowTask::writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success)
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

QString SetPurgeFlowTask::commandFrameLogString(const ModbusCommand& cmd) const
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

    return QString("[SetPurgeFlowTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SetPurgeFlowTask::subFunctionName() const
{
    return QStringLiteral("set_purge_flow");
}

QString SetPurgeFlowTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/set_purge_flow_task/%1").arg(subFunctionName());
}

QString SetPurgeFlowTask::safeLogPathSegment(const QString& value)
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

ILogger& SetPurgeFlowTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
