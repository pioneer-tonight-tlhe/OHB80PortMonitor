#include "send_command_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"

#include <QDateTime>
#include <QDebug>
#include <QVariantMap>

namespace {
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

SendCommandTask::SendCommandTask(QObject *parent)
    : SchedulerTask(parent)
    , deviceLogger("scheduler/send_command_task/detail")
{
    qDebug() << "[Scheduler][SendCommandTask] create task";
}

SendCommandTask::~SendCommandTask()
{
    qDebug() << "[Scheduler][SendCommandTask] destroy task";
}

void SendCommandTask::setSendToDevices(const QVector<QString> &qrcodes,
                                        const QString &commandName,
                                        const QVector<quint16> &params)
{
    m_targetQrcodes = qrcodes;
    m_commandName = commandName;
    m_params = params;
    m_sendToAll = false;
}

void SendCommandTask::setSendToAll(const QString &commandName,
                                   const QVector<quint16> &params)
{
    m_commandName = commandName;
    m_params = params;
    m_sendToAll = true;
}

void SendCommandTask::start()
{
    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_completedCount.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_resultSuccessCount = 0;
    m_resultFailedIds.clear();

    if (m_commandName.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, QStringLiteral("SendCommandTask: command name is empty"));
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, QStringLiteral("SendCommandTask: CommandPool is not initialized"));
        return;
    }

    if (!pool->contains(m_commandName)) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, QStringLiteral("SendCommandTask: command template '%1' not found").arg(m_commandName));
        return;
    }

    const QStringList targetIds = m_sendToAll
        ? mgr.masterIds()
        : QStringList(m_targetQrcodes.begin(), m_targetQrcodes.end());

    if (targetIds.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, 0, {});
        emit finished(false, QStringLiteral("SendCommandTask: no target device"));
        return;
    }

    const QByteArray overrideRegisterValue = buildRegisterValue(m_params);

    for (const QString &id : targetIds) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][SendCommandTask] master not found:" << id;
            writeDeviceSkipLog(id, m_commandName, QStringLiteral("Master 不存在"));
            if (!m_resultFailedIds.contains(id)) {
                m_resultFailedIds.append(id);
            }
            continue;
        }

        if (!master->isConnected()) {
            qWarning() << "[Scheduler][SendCommandTask] device disconnected:" << id;
            writeDeviceSkipLog(id, m_commandName, QStringLiteral("设备未连接"));
            if (!m_resultFailedIds.contains(id)) {
                m_resultFailedIds.append(id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            qWarning() << "[Scheduler][SendCommandTask] sender is null:" << id;
            writeDeviceSkipLog(id, m_commandName, QStringLiteral("Sender 为空"));
            if (!m_resultFailedIds.contains(id)) {
                m_resultFailedIds.append(id);
            }
            continue;
        }

        ModbusCommand cmd = pool->clone(m_commandName);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SendCommandTask] clone command failed:" << id;
            writeDeviceSkipLog(id, m_commandName, QStringLiteral("克隆指令失败"));
            if (!m_resultFailedIds.contains(id)) {
                m_resultFailedIds.append(id);
            }
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;

        if (!overrideRegisterValue.isEmpty()) {
            cmd.request.registerValue = overrideRegisterValue;
            cmd.request.byteCount = static_cast<quint8>(overrideRegisterValue.size());

            if (cmd.request.functionCode == 0x06
                && cmd.request.rawBytes.size() >= 6
                && overrideRegisterValue.size() >= 2) {
                cmd.request.rawBytes[4] = overrideRegisterValue[0];
                cmd.request.rawBytes[5] = overrideRegisterValue[1];
            }

            if (cmd.request.functionCode == 0x06
                && overrideRegisterValue.size() >= 2) {
                cmd.response.registerValue = overrideRegisterValue;
                if (cmd.response.rawBytes.size() >= 6) {
                    cmd.response.rawBytes[4] = overrideRegisterValue[0];
                    cmd.response.rawBytes[5] = overrideRegisterValue[1];
                }
            }
        }

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SendCommandTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &SendCommandTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        qDebug() << "[Scheduler][SendCommandTask] send to device" << id
                 << "command=" << m_commandName;

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        disconnectAll();
        setState(Failed);
        emit allFinished(false, 0, m_resultFailedIds.count(), m_resultFailedIds);
        emit finished(false, QString("SendCommandTask: no device accepted command, failed=%1")
                                .arg(m_resultFailedIds.count()));
    }
}

void SendCommandTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SendCommandTask: cancelled"));
}

void SendCommandTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
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
        ++m_resultSuccessCount;
        emit dataResult(qrCode, cmd);
        qDebug() << "[Scheduler][SendCommandTask] device success:" << qrCode
                 << "command=" << cmd.id;
    } else {
        if (!m_resultFailedIds.contains(qrCode)) {
            m_resultFailedIds.append(qrCode);
        }
        qWarning() << "[Scheduler][SendCommandTask] device failed:" << qrCode
                   << "command=" << cmd.id
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
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
                      ? QString("Command '%1' completed on %2 devices")
                            .arg(m_commandName).arg(done)
                      : QString("Command '%1' completed: %2 succeeded, %3 failed")
                            .arg(m_commandName)
                            .arg(m_resultSuccessCount)
                            .arg(m_resultFailedIds.count()));
}

void SendCommandTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;

    qDebug() << "[Scheduler][SendCommandTask] command timeout retry:" << qrCode
             << retryCount << "/" << maxRetry;

    emit deviceRetrying(qrCode, retryCount, maxRetry);
}

QByteArray SendCommandTask::buildRegisterValue(const QVector<quint16> &params) const
{
    QByteArray bytes;
    bytes.reserve(params.size() * 2);
    for (quint16 v : params) {
        bytes.append(static_cast<char>((v >> 8) & 0xFF));
        bytes.append(static_cast<char>(v & 0xFF));
    }
    return bytes;
}

void SendCommandTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

void SendCommandTask::writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason)
{
    deviceDetailLogger().warn(
        QString("[SendCommandTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SendCommandTask::writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success)
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

QString SendCommandTask::commandFrameLogString(const ModbusCommand& cmd) const
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

    return QString("[SendCommandTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SendCommandTask::subFunctionName() const
{
    return safeLogPathSegment(m_commandName);
}

QString SendCommandTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/send_command_task/%1").arg(subFunctionName());
}

QString SendCommandTask::safeLogPathSegment(const QString& value)
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

ILogger& SendCommandTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
