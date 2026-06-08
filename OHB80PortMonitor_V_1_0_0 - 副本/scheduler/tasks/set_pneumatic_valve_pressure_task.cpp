#include "set_pneumatic_valve_pressure_task.h"
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
#include <cmath>

namespace {
constexpr const char *kCmdId = "WritePneumaticValvePressure";

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

SetPneumaticValvePressureTask::SetPneumaticValvePressureTask(const QVector<QString> &qrcodes,
                                                             double pressureBar,
                                                             QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_pressureBar(pressureBar)
    , deviceLogger("scheduler/set_pneumatic_valve_pressure_task/detail")
{
    qDebug() << "[Scheduler][SetPneumaticValvePressureTask] create task qrcodes="
             << qrcodes << "pressure=" << pressureBar << "bar";
}

SetPneumaticValvePressureTask::~SetPneumaticValvePressureTask()
{
    qDebug() << "[Scheduler][SetPneumaticValvePressureTask] destroy task";
}

void SetPneumaticValvePressureTask::start()
{
    // 断开所有之前的信号连接
    disconnectAll();

    // 初始化任务状态
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

    // 检查设备列表是否为空
    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_pressureBar);
        emit finished(false, QStringLiteral("SetPneumaticValvePressureTask: qrcode list is empty"));
        return;
    }

    // 获取 Modbus 管理器和指令池
    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_pressureBar);
        emit finished(false, QStringLiteral("SetPneumaticValvePressureTask: CommandPool is not initialized"));
        return;
    }

    // 检查指令池中是否包含所需指令
    if (!pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_pressureBar);
        emit finished(false, QStringLiteral("SetPneumaticValvePressureTask: command '%1' not found").arg(kCmdId));
        return;
    }

    // 计算压力值的寄存器值（按寄存器倍率缩放）
    const double scaled = m_pressureBar * kRegisterScale;
    const quint32 raw = static_cast<quint32>(std::round(scaled));
    const quint16 regVal = static_cast<quint16>(qBound<quint32>(0, raw, 0xFFFF));
    const QByteArray regBytes = buildRegisterValue(regVal);

    // 记录任务启动日志
    if (auto* opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetPneumaticValvePressure task started: %1 bar for %2 devices")
                             .arg(m_pressureBar)
                             .arg(m_qrcodes.size()),
                         0);
    }

    // 保存目标设备列表（用于边界日志）
    for (const QString &id : m_qrcodes) {
        m_targetQrCodes.append(id);
    }

    // 遍历所有设备，发送气动阀门压力设置指令
    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        // 检查设备是否可用
        if (!master || !master->isConnected() || !master->sender()) {
            qWarning() << "[Scheduler][SetPneumaticValvePressureTask] device unavailable, skip:" << id;
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
        // 检查指令克隆是否成功
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetPneumaticValvePressureTask] clone command failed:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("克隆指令失败"));
            if (!m_failedQrCodes.contains(id)) {
                m_failedQrCodes.append(id);
            }
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        // 设置指令参数
        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = regBytes;
        cmd.request.byteCount = static_cast<quint8>(regBytes.size());

        // 手动填充请求帧的寄存器值（功能码 0x06 写单个寄存器）
        if (cmd.request.functionCode == 0x06
            && cmd.request.rawBytes.size() >= 6
            && regBytes.size() >= 2) {
            cmd.request.rawBytes[4] = regBytes[0];  // 寄存器值高字节
            cmd.request.rawBytes[5] = regBytes[1];  // 寄存器值低字节
        }

        // 手动填充响应帧的寄存器值（用于测试/模拟）
        cmd.response.registerValue = regBytes;
        if (cmd.response.rawBytes.size() >= 6 && regBytes.size() >= 2) {
            cmd.response.rawBytes[4] = regBytes[0];
            cmd.response.rawBytes[5] = regBytes[1];
        }

        // 连接信号：指令完成回调
        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetPneumaticValvePressureTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        // 连接信号：指令超时重试回调
        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &SetPneumaticValvePressureTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        // 记录待处理的指令
        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        qDebug() << "[Scheduler][SetPneumaticValvePressureTask] send to device" << id
                 << kCmdId << "pressure=" << m_pressureBar << "registerValue=" << regVal;

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    // 如果没有成功发送任何指令，强制完成任务
    if (m_totalCount == 0) {
        forceFinish();
        return;
    }

}

void SetPneumaticValvePressureTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetPneumaticValvePressureTask: cancelled"));
}

void SetPneumaticValvePressureTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
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
        qDebug() << "[Scheduler][SetPneumaticValvePressureTask] device success:" << qrCode;
    } else {
        if (!m_failedQrCodes.contains(qrCode)) {
            m_failedQrCodes.append(qrCode);
        }
        qWarning() << "[Scheduler][SetPneumaticValvePressureTask] device failed:" << qrCode
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, qrCode);
        }
    }

    checkAllFinished();
}

void SetPneumaticValvePressureTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;
    qDebug() << "[Scheduler][SetPneumaticValvePressureTask] command timeout retry:" << qrCode
             << retryCount << "/" << maxRetry;

    emit deviceRetrying(qrCode, retryCount, maxRetry);
}

void SetPneumaticValvePressureTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;

    forceFinish();
}

void SetPneumaticValvePressureTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    disconnectAll();

    auto* opTaskPending = SharedData::getOperationDispatchTask();
    for (auto it = m_pendingMap.constBegin(); it != m_pendingMap.constEnd(); ++it) {
        const QString qrCode = it.value();
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
            ? QString("SetPneumaticValvePressure %1 bar task completed: %2 devices succeeded")
                  .arg(m_pressureBar)
                  .arg(m_successCount)
            : QString("SetPneumaticValvePressure %1 bar task finished: %2 succeeded, %3 failed")
                  .arg(m_pressureBar)
                  .arg(m_successCount)
                  .arg(m_failedQrCodes.count());
        opTaskEnd->log(allSuccess ? OperationDispatchTask::MsgType::Message
                                  : OperationDispatchTask::MsgType::Error,
                       desc, 0);
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes, m_pressureBar);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetPneumaticValvePressureTask: pressure %1 bar completed (%2 devices)")
                            .arg(m_pressureBar).arg(m_successCount)
                      : QString("SetPneumaticValvePressureTask: pressure %1 bar completed, %2 succeeded, %3 failed")
                            .arg(m_pressureBar).arg(m_successCount).arg(m_failedQrCodes.count()));
}

void SetPneumaticValvePressureTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("SetPneumaticValvePressure %1 bar task failed: device %2")
        .arg(m_pressureBar)
        .arg(qrcode);
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
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

void SetPneumaticValvePressureTask::writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason)
{
    deviceDetailLogger().info(
        QString("[SetPneumaticValvePressureTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void SetPneumaticValvePressureTask::writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success)
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

QString SetPneumaticValvePressureTask::commandFrameLogString(const ModbusCommand& cmd) const
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

    return QString("[SetPneumaticValvePressureTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString SetPneumaticValvePressureTask::subFunctionName() const
{
    return QStringLiteral("set_pneumatic_valve_pressure");
}

QString SetPneumaticValvePressureTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/set_pneumatic_valve_pressure_task/%1").arg(subFunctionName());
}

QString SetPneumaticValvePressureTask::safeLogPathSegment(const QString& value)
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

ILogger& SetPneumaticValvePressureTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
