#include "set_vefc_gas_type_task.h"
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

namespace {
constexpr const char *kCmdId          = "WriteVEFCGasType";
constexpr int         kTotalTimeoutMs = 5000;

QString bytesToHexWithCrc(const QByteArray& bytes, const QByteArray& crc)
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
    return hexList.isEmpty() ? QStringLiteral("无") : hexList.join(QStringLiteral(" "));
}

QString gasTypeName(int gasType)
{
    switch (gasType) {
    case SetVEFCGasTypeTask::CDA:
        return QStringLiteral("CDA");
    case SetVEFCGasTypeTask::N2:
        return QStringLiteral("N2");
    case SetVEFCGasTypeTask::Ar:
        return QStringLiteral("Ar");
    case SetVEFCGasTypeTask::CO2:
        return QStringLiteral("CO2");
    case SetVEFCGasTypeTask::O2:
        return QStringLiteral("O2");
    default:
        return QStringLiteral("Unknown");
    }
}
} // namespace

SetVEFCGasTypeTask::SetVEFCGasTypeTask(const QVector<QString> &qrcodes,
                                       int gasType,
                                       QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_gasType(gasType)
    , deviceLogger("scheduler/set_vefc_gas_type_task/detail")
{
    qDebug() << "[Scheduler][SetVEFCGasTypeTask] create task: qrcodes=" << qrcodes
             << "gasType=" << gasType;
}

SetVEFCGasTypeTask::~SetVEFCGasTypeTask()
{
    qDebug() << "[Scheduler][SetVEFCGasTypeTask] destroy task";
}

void SetVEFCGasTypeTask::start()
{
    disconnectAll();

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
        emit allFinished(false, 0, {}, m_gasType);
        emit finished(false, QStringLiteral("SetVEFCGasTypeTask: qrcode 列表为空"));
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_gasType);
        emit finished(false, QStringLiteral("SetVEFCGasTypeTask: 指令 '%1' 不存在").arg(kCmdId));
        return;
    }

    const quint16 regVal = static_cast<quint16>(qBound(0, m_gasType, 0xFFFF));
    const QByteArray regBytes = buildRegisterValue(regVal);

    if (auto* opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QStringLiteral("设置 VEFC 气体类型任务开始: gasType=%1(%2), %3 台设备")
                             .arg(m_gasType)
                             .arg(gasTypeName(m_gasType))
                             .arg(m_qrcodes.size()), 0);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][SetVEFCGasTypeTask] master not found, skip:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("Master 不存在"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }
        if (!master->isConnected()) {
            qWarning() << "[Scheduler][SetVEFCGasTypeTask] device disconnected, skip:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("设备未连接"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }
        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            qWarning() << "[Scheduler][SetVEFCGasTypeTask] sender is null, skip:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("Sender 为空"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommand cmd = pool->clone(kCmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetVEFCGasTypeTask] clone command failed:" << id;
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
        cmd.request.byteCount     = static_cast<quint8>(regBytes.size());

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
                            this, &SetVEFCGasTypeTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &SetVEFCGasTypeTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        qDebug() << "[Scheduler][SetVEFCGasTypeTask] send to device" << id
                 << kCmdId << "regVal=" << regVal;

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
                this, &SetVEFCGasTypeTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

void SetVEFCGasTypeTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetVEFCGasTypeTask: 任务已取消"));
}

void SetVEFCGasTypeTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
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
            if (cmd.request.crc.size() >= 2) {
                requestWithCrc.append(cmd.request.crc);
            }

            QByteArray responseWithCrc = cmd.response.rawBytes;
            if (cmd.response.crc.size() >= 2) {
                responseWithCrc.append(cmd.response.crc);
            }

            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, qrCode,
                             execStatus, retryCount,
                             requestWithCrc, responseWithCrc, description,
                             UserPermission::Engineer);
        }
    }

    const bool success = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;
    writeDeviceCommandLog(qrCode, cmd, success);

    if (success) {
        ++m_successCount;
        qDebug() << "[Scheduler][SetVEFCGasTypeTask] device success:" << qrCode;
    } else {
        if (!m_failedQrCodes.contains(qrCode)) {
            m_failedQrCodes.append(qrCode);
        }
        qWarning() << "[Scheduler][SetVEFCGasTypeTask] device failed:" << qrCode
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, qrCode);
        }
    }

    checkAllFinished();
}

void SetVEFCGasTypeTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;

    qDebug() << "[Scheduler][SetVEFCGasTypeTask] command timeout retry:"
             << qrCode << retryCount << "/" << maxRetry;

    emit deviceRetrying(qrCode, retryCount, maxRetry);
}

void SetVEFCGasTypeTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;
    forceFinish();
}

void SetVEFCGasTypeTask::onTimeout()
{
    qWarning() << "[Scheduler][SetVEFCGasTypeTask] timeout, pending:" << m_pendingMap.size();
    forceFinish();
}

void SetVEFCGasTypeTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();

    auto* opTaskPending = SharedData::getOperationDispatchTask();
    for (const QString &qr : m_pendingMap.values()) {
        if (!m_failedQrCodes.contains(qr)) {
            m_failedQrCodes.append(qr);
        }
        if (opTaskPending) {
            logFailedDevice(opTaskPending, qr);
        }
    }
    m_pendingMap.clear();

    const bool allSuccess = m_failedQrCodes.isEmpty();
    setState(allSuccess ? Finished : Failed);

    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        const QString desc = allSuccess
            ? QStringLiteral("设置 VEFC 气体类型任务完成: gasType=%1(%2), %3 台成功")
                  .arg(m_gasType).arg(gasTypeName(m_gasType)).arg(m_successCount)
            : QStringLiteral("设置 VEFC 气体类型任务结束: gasType=%1(%2), %3 台成功, %4 台失败")
                  .arg(m_gasType).arg(gasTypeName(m_gasType))
                  .arg(m_successCount).arg(m_failedQrCodes.count());
        opTaskEnd->log(allSuccess ? OperationDispatchTask::MsgType::Message
                                   : OperationDispatchTask::MsgType::Error,
                       desc, 0);
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes, m_gasType);
    emit finished(allSuccess,
                  allSuccess
                      ? QStringLiteral("SetVEFCGasTypeTask: 设置完成（%1 台）").arg(m_successCount)
                      : QStringLiteral("SetVEFCGasTypeTask: %1 台成功，%2 台失败")
                            .arg(m_successCount).arg(m_failedQrCodes.count()));
}

void SetVEFCGasTypeTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QStringLiteral("[QRCode:%1]: 设置 VEFC 气体类型失败 gasType=%2(%3)")
        .arg(qrcode)
        .arg(m_gasType)
        .arg(gasTypeName(m_gasType));
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetVEFCGasTypeTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetVEFCGasTypeTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

void SetVEFCGasTypeTask::writeDeviceSkipLog(const QString& qrCode,
                                            const QString& commandId,
                                            const QString& reason)
{
    deviceDetailLogger().info(
        QString("[SetVEFCGasTypeTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SetVEFCGasTypeTask::writeDeviceCommandLog(const QString& qrCode,
                                               const ModbusCommand& cmd,
                                               bool success)
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

QString SetVEFCGasTypeTask::commandFrameLogString(const ModbusCommand& cmd) const
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

    return QString("[SetVEFCGasTypeTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SetVEFCGasTypeTask::subFunctionName() const
{
    return QStringLiteral("set_vefc_gas_type");
}

QString SetVEFCGasTypeTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/set_vefc_gas_type_task/%1").arg(subFunctionName());
}

ILogger& SetVEFCGasTypeTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
