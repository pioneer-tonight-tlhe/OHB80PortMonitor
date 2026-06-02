#include "modbuscommandsender.h"
#include "modbuslogger.h"
#include <QDateTime>
#include <QDebug>
#include <QString>

namespace {
static inline QString nowStr()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}

static inline QString toHexSpaced(const QByteArray& data)
{
    QString s;
    s.reserve(data.size() * 3);
    for (unsigned char b : data) {
        s += QString::asprintf("%02X ", b);
    }
    if (!s.isEmpty()) s.chop(1);
    return s;
}

static inline QString commandModuleToString(CommandModule module)
{
    switch (module) {
        case CommandModule::InitialCommandIssuer:  return "INITIAL";
        case CommandModule::PeriodicCommandSender: return "PERIODIC";
        case CommandModule::BusinessCommandIssuer: return "BUSINESS";
    }
    return "UNKNOWN";
}

static inline bool shouldLogCommand(const ModbusCommand& cmd)
{
    return cmd.module != CommandModule::PeriodicCommandSender;
}
}

ModbusCommandSender::ModbusCommandSender(QTcpSocket& socket, const QString& masterId, QObject* parent)
    : QObject(parent)
    , m_masterId(const_cast<QString&>(masterId))
{
    m_socket = &socket;
    m_receiver = new ModbusCommandReceiver(socket, masterId, this);
    connect(m_receiver, &ModbusCommandReceiver::commandSucceeded,
            this, &ModbusCommandSender::onReceiverSucceeded);
    connect(m_receiver, &ModbusCommandReceiver::commandFailed,
            this, &ModbusCommandSender::onReceiverFailed);
}

void ModbusCommandSender::submit(const ModbusCommand& cmd)
{
    QMutexLocker locker(&m_mutex);

    QueueState* qs = nullptr;
    QString queueName;

    switch (cmd.module) {
        case CommandModule::InitialCommandIssuer:
            qs = &m_initialState;
            queueName = "初始下发指令队列";
            break;
        case CommandModule::BusinessCommandIssuer:
            qs = &m_businessState;
            queueName = "业务指令队列";
            break;
        case CommandModule::PeriodicCommandSender:
            qs = &m_periodicState;
            queueName = "定时指令队列";
            break;
    }

    if (!qs || !enqueue(*qs, cmd)) {
        ModbusCommand rejected = cmd;
        rejected.deviceBusy = true;
        rejected.received = false;
        rejected.timedOut = false;
        rejected.checksumError = false;
        rejected.errorMessage = QString("设备繁忙，拒绝指令下发（%1已满）").arg(queueName);
//        qDebug() << "[BUSY-REJECT] [设备ID=" << m_masterId << "] " << nowStr()
//                 << "id=" << rejected.id
//                 << "uuid=" << rejected.uuid
//                 << "queue=" << queueName;
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "submit",
            QString("指令队列已满，拒绝提交 module=%1 id=%2 uuid=%3 queue=%4")
                .arg(commandModuleToString(rejected.module)).arg(rejected.id).arg(rejected.uuid).arg(queueName));
        locker.unlock();
        emit commandFinished(rejected, m_masterId);
        return;
    }

    bool shouldDispatch = (m_running && !m_hasPendingCommand);
    locker.unlock();

    if (shouldDispatch) {
        dispatch();
    }
}

void ModbusCommandSender::setQueueCapacity(int capacity)
{
    QMutexLocker locker(&m_mutex);
    m_queueCapacity = capacity;
}

void ModbusCommandSender::start()
{
    m_running = true;
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "start", "指令发送器启动");
    if (!m_hasPendingCommand) {
        dispatch();
    }
}

void ModbusCommandSender::stop()
{
    m_running = false;
    m_hasPendingCommand = false;
    m_lastSendMs = 0;
    if (m_receiver) {
        m_receiver->cancelPending();
    }
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "stop", "指令发送器停止");
}

void ModbusCommandSender::dispatch()
{
    if (!m_running || m_hasPendingCommand || !m_socket || !m_receiver) {
        return;
    }

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    // 最小发送间隔节流：相邻两次 write 之间至少间隔 MIN_SEND_INTERVAL_MS，
    // 避免在对端无响应时多条 8 字节 RTU 帧被内核合并成一个 TCP segment，
    // 触发 HF2211 等串口转 TCP 网关因粘帧/缓冲溢出而 RST 关闭连接。
    if (m_lastSendMs > 0) {
        const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - m_lastSendMs;
        if (elapsed < MIN_SEND_INTERVAL_MS) {
            if (!m_dispatchScheduled) {
                m_dispatchScheduled = true;
                QTimer::singleShot(static_cast<int>(MIN_SEND_INTERVAL_MS - elapsed), this, [this]() {
                    m_dispatchScheduled = false;
                    dispatch();
                });
            }
            return;
        }
    }

    if (!m_retryState.queue.isEmpty()) {
        doSend(m_retryState.queue.dequeue());
        return;
    }

    if (!m_initialState.queue.isEmpty()) {
        doSend(m_initialState.queue.dequeue());
        return;
    }

    if (!m_businessState.queue.isEmpty()) {
        doSend(m_businessState.queue.dequeue());
        return;
    }

    if (!m_periodicState.queue.isEmpty()) {
        doSend(m_periodicState.queue.dequeue());
        return;
    }
}

void ModbusCommandSender::onReceiverSucceeded(ModbusCommand cmd)
{
    finishCurrentCommand(cmd);
}

void ModbusCommandSender::onReceiverFailed(ModbusCommand cmd, bool timedOut, bool checksumError)
{
    handleFailedCommand(cmd, cmd.errorMessage, timedOut, checksumError);
}

void ModbusCommandSender::doSend(ModbusCommand cmd)
{
    if (!m_socket) {
        cmd.errorMessage = "未绑定QTcpSocket";
        cmd.timedOut = false;
        cmd.checksumError = false;
        cmd.deviceBusy = false;
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "doSend",
            QString("指令发送失败 module=%1 id=%2 uuid=%3 error=%4")
                .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid).arg(cmd.errorMessage));
        emit commandFinished(cmd, m_masterId);
        return;
    }

    const QByteArray requestFrame = buildRequestFrame(cmd);
    if (requestFrame.isEmpty()) {
        cmd.errorMessage = "请求帧为空，无法发送";
        cmd.timedOut = false;
        cmd.checksumError = false;
        cmd.deviceBusy = false;
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "doSend",
            QString("指令发送失败 module=%1 id=%2 uuid=%3 error=%4")
                .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid).arg(cmd.errorMessage));
        emit commandFinished(cmd, m_masterId);
        return;
    }

    cmd.sentMs = QDateTime::currentMSecsSinceEpoch();
    cmd.sendCount++;
    cmd.received = false;
    cmd.timedOut = false;
    cmd.checksumError = false;
    cmd.deviceBusy = false;
    if (requestFrame.size() >= 2) {
        cmd.request.crc = requestFrame.right(2);
    }

//    QString moduleStr;
//    switch (cmd.module) {
//        case CommandModule::InitialCommandIssuer:  moduleStr = "INITIAL"; break;
//        case CommandModule::PeriodicCommandSender: moduleStr = "PERIODIC"; break;
//        case CommandModule::BusinessCommandIssuer: moduleStr = "BUSINESS"; break;
//    }
//
//    QString logMsg = QString("[发送] %1 设备ID=%2 module=%3 id=%4 uuid=%5 sendCount=%6/%7 timeout=%8ms crc=%9 len=%10")
//            .arg(nowStr())
//            .arg(m_masterId)
//            .arg(moduleStr)
//            .arg(cmd.id)
//            .arg(cmd.uuid)
//            .arg(cmd.sendCount)
//            .arg(cmd.maxRetryCount + 1)
//            .arg(cmd.timeoutMs)
//            .arg(toHexSpaced(cmd.request.crc))
//            .arg(requestFrame.size());
//
//    QString frameHex = toHexSpaced(requestFrame);
//    QString logMsgWithFrame = logMsg + "\nframe=" + frameHex;
//
//    qDebug() << logMsgWithFrame;
    if (shouldLogCommand(cmd)) {
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "doSend",
            QString("发送指令 module=%1 id=%2 uuid=%3 sendCount=%4/%5 timeout=%6ms bytes=%7 crc=%8")
                .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid)
                .arg(cmd.sendCount).arg(cmd.maxRetryCount + 1).arg(cmd.timeoutMs)
                .arg(requestFrame.size()).arg(toHexSpaced(cmd.request.crc)));
    }

    const qint64 written = m_socket->write(requestFrame);
    if (written != requestFrame.size()) {
        handleFailedCommand(cmd, QString("发送失败: %1").arg(m_socket->errorString()), false, false);
        return;
    }
    m_lastSendMs = QDateTime::currentMSecsSinceEpoch();

    m_socket->flush();
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + qMin(cmd.timeoutMs, 1000);
    while (m_socket->bytesToWrite() > 0) {
        const int remaining = static_cast<int>(deadline - QDateTime::currentMSecsSinceEpoch());
        if (remaining <= 0) {
            handleFailedCommand(cmd, "等待发送缓冲区写出超时", false, false);
            return;
        }
        const int chunk = remaining < 200 ? remaining : 200;
        if (!m_socket->waitForBytesWritten(chunk)) {
            handleFailedCommand(cmd, "等待发送缓冲区写出超时", false, false);
            return;
        }
    }

    m_pendingCommand = cmd;
    m_hasPendingCommand = true;

    if (!m_receiver->beginReceive(cmd)) {
        handleFailedCommand(cmd, "接收器忙，无法登记待响应指令", false, false);
    }
}

bool ModbusCommandSender::enqueue(QueueState& qs, const ModbusCommand& cmd)
{
    if (qs.queue.size() >= m_queueCapacity) {
        return false;
    }
    qs.queue.enqueue(cmd);
    return true;
}

QByteArray ModbusCommandSender::buildRequestFrame(const ModbusCommand& cmd) const
{
    if (cmd.request.rawBytes.isEmpty()) {
        return {};
    }

    QByteArray frame = cmd.request.rawBytes;
    const quint16 crc = crc16(frame);
    frame.append(static_cast<char>(crc & 0xFF));
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    return frame;
}

void ModbusCommandSender::finishCurrentCommand(ModbusCommand cmd)
{
    m_hasPendingCommand = false;
    emit commandFinished(cmd, m_masterId);
    if (m_running) {
        dispatch();
    }
}

void ModbusCommandSender::handleFailedCommand(ModbusCommand cmd, const QString& errorMessage, bool timedOut, bool checksumError)
{
    m_hasPendingCommand = false;

    cmd.errorMessage = errorMessage;
    cmd.timedOut = timedOut;
    cmd.checksumError = checksumError;

//    QString moduleStr;
//    switch (cmd.module) {
//        case CommandModule::InitialCommandIssuer:  moduleStr = "INITIAL"; break;
//        case CommandModule::PeriodicCommandSender: moduleStr = "PERIODIC"; break;
//        case CommandModule::BusinessCommandIssuer: moduleStr = "BUSINESS"; break;
//    }

    // 定时查询指令失败不进入重发队列（依赖下一次周期轮询，避免堆积）
    const bool shouldRetry = !cmd.deviceBusy && !cmd.checksumError
                           && cmd.sendCount <= cmd.maxRetryCount
                           && cmd.module != CommandModule::PeriodicCommandSender;

    if (!shouldRetry) {
//        qDebug() << "[FAILED] [设备ID=" << m_masterId << "] " << nowStr()
//                 << "module=" << moduleStr
//                 << "id=" << cmd.id
//                 << "uuid=" << cmd.uuid
//                 << "error=" << errorMessage
//                 << "sendCount=" << cmd.sendCount
//                 << "/" << (cmd.maxRetryCount + 1)
//                 << (cmd.deviceBusy ? " (deviceBusy)" : QString())
//                 << (cmd.checksumError ? " (checksumError)" : QString());
        if (shouldLogCommand(cmd)) {
            ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "handleFailedCommand",
                QString("指令失败 module=%1 id=%2 uuid=%3 error=%4 sendCount=%5/%6 timedOut=%7 checksumError=%8 deviceBusy=%9")
                    .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid).arg(errorMessage)
                    .arg(cmd.sendCount).arg(cmd.maxRetryCount + 1)
                    .arg(timedOut).arg(checksumError).arg(cmd.deviceBusy));
        }
        emit commandFinished(cmd, m_masterId);
        if (m_running) {
            dispatch();
        }
        return;
    }

//    qDebug() << "[RETRY] [设备ID=" << m_masterId << "] " << nowStr()
//             << "module=" << moduleStr
//             << "id=" << cmd.id
//             << "uuid=" << cmd.uuid
//             << "error=" << errorMessage
//             << "sendCount=" << cmd.sendCount
//             << "/" << (cmd.maxRetryCount + 1);
    if (shouldLogCommand(cmd)) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "handleFailedCommand",
            QString("指令失败，准备重发 module=%1 id=%2 uuid=%3 error=%4 sendCount=%5/%6")
                .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid).arg(errorMessage)
                .arg(cmd.sendCount).arg(cmd.maxRetryCount + 1));
    }

    // 超时重发：先通知外部底层正在尝试重新发送该指令，再投递到重发队列
    if (timedOut) {
        emit commandTimeoutRetry(cmd, m_masterId);
    }

    addToRetryQueue(cmd);
}

void ModbusCommandSender::addToRetryQueue(ModbusCommand cmd)
{
    if (!enqueue(m_retryState, cmd)) {
        cmd.errorMessage = "重发队列已满，放弃发送";
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "addToRetryQueue",
            QString("重发队列已满，放弃发送 module=%1 id=%2 uuid=%3")
                .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid));
        emit commandFinished(cmd, m_masterId);
    } else {
//        QString moduleStr;
//        switch (cmd.module) {
//            case CommandModule::InitialCommandIssuer:  moduleStr = "INITIAL"; break;
//            case CommandModule::PeriodicCommandSender: moduleStr = "PERIODIC"; break;
//            case CommandModule::BusinessCommandIssuer: moduleStr = "BUSINESS"; break;
//        }
//        qDebug() << "[RETRY-QUEUE] [设备ID=" << m_masterId << "] " << nowStr()
//                 << "module=" << moduleStr
//                 << "id=" << cmd.id
//                 << "uuid=" << cmd.uuid
//                 << "sendCount=" << cmd.sendCount
//                 << "/" << (cmd.maxRetryCount + 1);
        if (shouldLogCommand(cmd)) {
            ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusCommandSender", "addToRetryQueue",
                QString("指令已加入重发队列 module=%1 id=%2 uuid=%3 sendCount=%4/%5")
                    .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid)
                    .arg(cmd.sendCount).arg(cmd.maxRetryCount + 1));
        }
    }

    if (m_running) {
        dispatch();
    }
}

quint16 ModbusCommandSender::crc16(const QByteArray& data)
{
    quint16 crc = 0xFFFF;
    for (char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
