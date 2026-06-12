#include "set_ui_refresh_time_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QtGlobal>
#include <QVariantMap>

namespace {
constexpr const char *kCmdId          = "WriteUIRefreshTime";
constexpr int         kTotalTimeoutMs = 5000;
constexpr int         kPayloadBytes   = 6;

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
} // namespace

SetUIRefreshTimeTask::SetUIRefreshTimeTask(const QVector<QString> &qrcodes,
                                           int logoSec,
                                           int paramTotalSec,
                                           int paramSwitchSec,
                                           QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_logoSec(logoSec)
    , m_paramTotalSec(paramTotalSec)
    , m_paramSwitchSec(paramSwitchSec)
    , deviceLogger("scheduler/set_ui_refresh_time_task/detail")
{
    qDebug() << "[Scheduler][SetUIRefreshTimeTask] create task: qrcodes=" << qrcodes
             << "logoSec=" << logoSec << "paramTotalSec=" << paramTotalSec
             << "paramSwitchSec=" << paramSwitchSec;
}

SetUIRefreshTimeTask::~SetUIRefreshTimeTask()
{
    qDebug() << "[Scheduler][SetUIRefreshTimeTask] destroy task";
}

void SetUIRefreshTimeTask::start()
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
        emit allFinished(false, 0, {}, m_logoSec, m_paramTotalSec, m_paramSwitchSec);
        emit finished(false, QStringLiteral("SetUIRefreshTimeTask: qrcode 列表为空"));
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_logoSec, m_paramTotalSec, m_paramSwitchSec);
        emit finished(false, QStringLiteral("SetUIRefreshTimeTask: 指令 '%1' 不存在").arg(kCmdId));
        return;
    }

    const QByteArray payload = buildPayload();

    if (auto* opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QStringLiteral("设置 UI 刷新时间任务开始: logo=%1s total=%2s switch=%3s, %4 台设备")
                             .arg(m_logoSec)
                             .arg(m_paramTotalSec)
                             .arg(m_paramSwitchSec)
                             .arg(m_qrcodes.size()), 0);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][SetUIRefreshTimeTask] master not found, skip:" << id;
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
            qWarning() << "[Scheduler][SetUIRefreshTimeTask] device disconnected, skip:" << id;
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
            qWarning() << "[Scheduler][SetUIRefreshTimeTask] sender is null, skip:" << id;
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
            qWarning() << "[Scheduler][SetUIRefreshTimeTask] clone command failed:" << id;
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
        cmd.request.registerValue = payload;
        cmd.request.byteCount     = static_cast<quint8>(payload.size());

        if (cmd.request.functionCode == 0x10
            && cmd.request.rawBytes.size() >= 7 + kPayloadBytes
            && payload.size() == kPayloadBytes) {
            for (int i = 0; i < kPayloadBytes; ++i) {
                cmd.request.rawBytes[7 + i] = payload[i];
            }
        }

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetUIRefreshTimeTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &SetUIRefreshTimeTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        qDebug() << "[Scheduler][SetUIRefreshTimeTask] send to device" << id
                 << kCmdId << "payload=" << payload.toHex(' ').toUpper();

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
                this, &SetUIRefreshTimeTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

void SetUIRefreshTimeTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetUIRefreshTimeTask: 任务已取消"));
}

void SetUIRefreshTimeTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
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
        qDebug() << "[Scheduler][SetUIRefreshTimeTask] device success:" << qrCode;
    } else {
        if (!m_failedQrCodes.contains(qrCode)) {
            m_failedQrCodes.append(qrCode);
        }
        qWarning() << "[Scheduler][SetUIRefreshTimeTask] device failed:" << qrCode
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, qrCode);
        }
    }

    checkAllFinished();
}

void SetUIRefreshTimeTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;

    qDebug() << "[Scheduler][SetUIRefreshTimeTask] command timeout retry:"
             << qrCode << retryCount << "/" << maxRetry;

    emit deviceRetrying(qrCode, retryCount, maxRetry);
}

void SetUIRefreshTimeTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;
    forceFinish();
}

void SetUIRefreshTimeTask::onTimeout()
{
    qWarning() << "[Scheduler][SetUIRefreshTimeTask] timeout, pending:" << m_pendingMap.size();
    forceFinish();
}

void SetUIRefreshTimeTask::forceFinish()
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
            ? QStringLiteral("设置 UI 刷新时间任务完成: logo=%1s total=%2s switch=%3s, %4 台成功")
                  .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec).arg(m_successCount)
            : QStringLiteral("设置 UI 刷新时间任务结束: logo=%1s total=%2s switch=%3s, %4 台成功, %5 台失败")
                  .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec)
                  .arg(m_successCount).arg(m_failedQrCodes.count());
        opTaskEnd->log(allSuccess ? OperationDispatchTask::MsgType::Message
                                   : OperationDispatchTask::MsgType::Error,
                       desc, 0);
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes,
                     m_logoSec, m_paramTotalSec, m_paramSwitchSec);
    emit finished(allSuccess,
                  allSuccess
                      ? QStringLiteral("SetUIRefreshTimeTask: 设置完成（%1 台）").arg(m_successCount)
                      : QStringLiteral("SetUIRefreshTimeTask: %1 台成功，%2 台失败")
                            .arg(m_successCount).arg(m_failedQrCodes.count()));
}

void SetUIRefreshTimeTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QStringLiteral("[QRCode:%1]: 设置 UI 刷新时间失败 logo=%2s total=%3s switch=%4s")
        .arg(qrcode)
        .arg(m_logoSec)
        .arg(m_paramTotalSec)
        .arg(m_paramSwitchSec);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetUIRefreshTimeTask::buildPayload() const
{
    const quint16 logo  = static_cast<quint16>(qBound(0, m_logoSec,        0xFFFF));
    const quint16 total = static_cast<quint16>(qBound(0, m_paramTotalSec,  0xFFFF));
    const quint16 sw    = static_cast<quint16>(qBound(0, m_paramSwitchSec, 0xFFFF));

    QByteArray bytes(kPayloadBytes, 0);
    bytes[0] = static_cast<char>((logo  >> 8) & 0xFF);
    bytes[1] = static_cast<char>(logo        & 0xFF);
    bytes[2] = static_cast<char>((total >> 8) & 0xFF);
    bytes[3] = static_cast<char>(total       & 0xFF);
    bytes[4] = static_cast<char>((sw    >> 8) & 0xFF);
    bytes[5] = static_cast<char>(sw          & 0xFF);
    return bytes;
}

void SetUIRefreshTimeTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

void SetUIRefreshTimeTask::writeDeviceSkipLog(const QString& qrCode,
                                              const QString& commandId,
                                              const QString& reason)
{
    deviceDetailLogger().info(
        QString("[SetUIRefreshTimeTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SetUIRefreshTimeTask::writeDeviceCommandLog(const QString& qrCode,
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

QString SetUIRefreshTimeTask::commandFrameLogString(const ModbusCommand& cmd) const
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

    return QString("[SetUIRefreshTimeTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SetUIRefreshTimeTask::subFunctionName() const
{
    return QStringLiteral("set_ui_refresh_time");
}

QString SetUIRefreshTimeTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/set_ui_refresh_time_task/%1").arg(subFunctionName());
}

ILogger& SetUIRefreshTimeTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
