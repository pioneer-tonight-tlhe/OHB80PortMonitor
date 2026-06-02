#include "modbuscommandreceiver.h"
#include "modbuscrc.h"
#include "modbuslogger.h"
#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>

namespace {
constexpr int RING_BUFFER_CAPACITY = 4096;

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

ModbusCommandReceiver::ModbusCommandReceiver(QTcpSocket& socket, const QString& masterId, QObject* parent)
    : QObject(parent)
    , m_masterId(const_cast<QString&>(masterId))
{
    m_ringBuffer.resize(RING_BUFFER_CAPACITY);

    m_responseTimer = new QTimer(this);
    m_responseTimer->setSingleShot(true);
    connect(m_responseTimer, &QTimer::timeout, this, &ModbusCommandReceiver::onResponseTimeout);

    m_socket = &socket;
    connect(m_socket, &QTcpSocket::readyRead, this, &ModbusCommandReceiver::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, &ModbusCommandReceiver::onSocketDisconnected);
}

bool ModbusCommandReceiver::beginReceive(const ModbusCommand& cmd)
{
    if (m_hasPendingCommand) {
        return false;
    }

    m_pendingCommand = cmd;
    m_hasPendingCommand = true;
    m_responseTimer->start(qMax(1, cmd.timeoutMs));
    processPendingFrame();
    return true;
}

void ModbusCommandReceiver::cancelPending()
{
    m_responseTimer->stop();
    m_hasPendingCommand = false;
}

bool ModbusCommandReceiver::hasPendingCommand() const
{
    return m_hasPendingCommand;
}

void ModbusCommandReceiver::disconnectSocketSignalSlots()
{
    if (m_socket) {
        cancelPending();
        disconnect(m_socket, &QTcpSocket::readyRead, this, &ModbusCommandReceiver::onReadyRead);
        disconnect(m_socket, &QTcpSocket::disconnected, this, &ModbusCommandReceiver::onSocketDisconnected);
//        qDebug() << "[ModbusCommandReceiver] [设备ID=" << m_masterId << "] disconnectSocketSignalSlots: receiver 已断开 socket 信号";
    }
}

void ModbusCommandReceiver::reconnectSocketSignalSlots()
{
    if (m_socket) {
        connect(m_socket, &QTcpSocket::readyRead, this, &ModbusCommandReceiver::onReadyRead, Qt::UniqueConnection);
        connect(m_socket, &QTcpSocket::disconnected, this, &ModbusCommandReceiver::onSocketDisconnected, Qt::UniqueConnection);
//        qDebug() << "[ModbusCommandReceiver] [设备ID=" << m_masterId << "] reconnectSocketSignalSlots: receiver 已重连 socket 信号";
    }
}

void ModbusCommandReceiver::onReadyRead()
{
    if (!m_socket) {
        return;
    }

    const QByteArray data = m_socket->readAll();
    if (data.isEmpty()) {
        return;
    }

//    if (!m_hasPendingCommand || m_pendingCommand.module != CommandModule::PeriodicCommandSender) {
//        qDebug() << "[TCP-接收] [设备ID=" << m_masterId << "] " << nowStr()
//                 << "len=" << data.size()
//                 << "raw=" << toHexSpaced(data);
//    }

    ringAppend(data);
    processPendingFrame();
}

void ModbusCommandReceiver::onResponseTimeout()
{
    if (!m_hasPendingCommand) {
        return;
    }

    ModbusCommand failed = m_pendingCommand;

    QString moduleStr;
    switch (failed.module) {
        case CommandModule::InitialCommandIssuer:  moduleStr = "INITIAL"; break;
        case CommandModule::PeriodicCommandSender: moduleStr = "PERIODIC"; break;
        case CommandModule::BusinessCommandIssuer: moduleStr = "BUSINESS"; break;
    }

//    if (failed.module != CommandModule::PeriodicCommandSender) {
//        qDebug() << "[TIMEOUT] [设备ID=" << m_masterId << "] " << nowStr()
//                 << "module=" << moduleStr
//                 << "id=" << failed.id
//                 << "uuid=" << failed.uuid
//                 << "timeout=" << failed.timeoutMs << "ms";
//        QString logMsg = QString("响应超时 - 设备ID=%1 module=%2 id=%3 uuid=%4 timeout=%5ms").arg(m_masterId).arg(moduleStr).arg(failed.id).arg(failed.uuid).arg(failed.timeoutMs);
//        LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][onResponseTimeout]：%1").arg(logMsg).toStdString());
//    }
    if (shouldLogCommand(failed)) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandReceiver", "onResponseTimeout",
            QString("响应超时 module=%1 id=%2 uuid=%3 timeout=%4ms")
                .arg(commandModuleToString(failed.module)).arg(failed.id).arg(failed.uuid).arg(failed.timeoutMs));
    }

    failPendingCommand("等待RTU响应超时", true, false);
}

void ModbusCommandReceiver::onSocketDisconnected()
{
    if (!m_hasPendingCommand) {
        return;
    }

    ModbusCommand failed = m_pendingCommand;

    QString moduleStr;
    switch (failed.module) {
        case CommandModule::InitialCommandIssuer:  moduleStr = "INITIAL"; break;
        case CommandModule::PeriodicCommandSender: moduleStr = "PERIODIC"; break;
        case CommandModule::BusinessCommandIssuer: moduleStr = "BUSINESS"; break;
    }

//    if (failed.module != CommandModule::PeriodicCommandSender) {
//        qDebug() << "[DISCONNECT] [设备ID=" << m_masterId << "] " << nowStr()
//                 << "module=" << moduleStr
//                 << "id=" << failed.id
//                 << "uuid=" << failed.uuid;
//        QString logMsg = QString("TCP连接断开 - 设备ID=%1 module=%2 id=%3 uuid=%4").arg(m_masterId).arg(moduleStr).arg(failed.id).arg(failed.uuid);
//        LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][onSocketDisconnected]：%1").arg(logMsg).toStdString());
//    }
    if (shouldLogCommand(failed)) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandReceiver", "onSocketDisconnected",
            QString("TCP连接断开，当前等待指令失败 module=%1 id=%2 uuid=%3")
                .arg(commandModuleToString(failed.module)).arg(failed.id).arg(failed.uuid));
    }

    failPendingCommand("TCP连接已断开", false, false);
}

bool ModbusCommandReceiver::processPendingFrame()
{
    if (!m_hasPendingCommand) {
        return false;
    }

    QByteArray frame;
    int discardedPrefixBytes = 0;
    QByteArray discardedData;
    if (!tryExtractMatchedFrame(m_pendingCommand, frame, discardedPrefixBytes, discardedData)) {
        return false;
    }

    ModbusCommand finished = m_pendingCommand;

    QString moduleStr;
    switch (finished.module) {
        case CommandModule::InitialCommandIssuer:  moduleStr = "INITIAL"; break;
        case CommandModule::PeriodicCommandSender: moduleStr = "PERIODIC"; break;
        case CommandModule::BusinessCommandIssuer: moduleStr = "BUSINESS"; break;
    }

//    if (discardedPrefixBytes > 0) {
//        QString logMsg = QString("[接收-重新同步] %1 设备ID=%2 module=%3 id=%4 uuid=%5 因帧头不匹配丢弃了%6字节的前缀数据")
//                .arg(nowStr())
//                .arg(m_masterId)
//                .arg(moduleStr)
//                .arg(finished.id)
//                .arg(finished.uuid)
//                .arg(discardedPrefixBytes);
//        QString logMsgWithDiscarded = logMsg + "\n丢弃数据=" + toHexSpaced(discardedData);
//        logMsgWithDiscarded += "\n有效帧=" + toHexSpaced(frame);
//
//        if (finished.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << logMsgWithDiscarded;
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, logMsgWithDiscarded.toStdString());
//        }
//    }
    if (discardedPrefixBytes > 0 && shouldLogCommand(finished)) {
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusCommandReceiver", "processPendingFrame",
            QString("接收重新同步 module=%1 id=%2 uuid=%3 丢弃前缀字节数=%4 丢弃数据=%5 有效帧=%6")
                .arg(commandModuleToString(finished.module)).arg(finished.id).arg(finished.uuid)
                .arg(discardedPrefixBytes).arg(toHexSpaced(discardedData)).arg(toHexSpaced(frame)));
    }

    QString errorMessage;
    if (!validateResponseFrame(finished, frame, errorMessage)) {
//        QString logMsg = QString("[接收-脏帧] %1 设备ID=%2 module=%3 id=%4 uuid=%5 error=%6")
//                .arg(nowStr())
//                .arg(m_masterId)
//                .arg(moduleStr)
//                .arg(finished.id)
//                .arg(finished.uuid)
//                .arg(errorMessage);
//
//        QString frameHex = toHexSpaced(frame);
//        QString logMsgWithFrame = logMsg + "\nframe=" + frameHex;
//
//        // 如果是CRC错误，额外打印期望和实际CRC
//        if (errorMessage.contains("CRC")) {
//            const QByteArray payload = frame.left(frame.size() - 2);
//            const quint16 expectedCrc = crc16(payload);
//            const quint16 actualCrc = static_cast<quint8>(frame[frame.size() - 2]) |
//                                      (static_cast<quint16>(static_cast<quint8>(frame[frame.size() - 1])) << 8);
//            QString crcMsg = QString("\nexpectedCrc=0x%1 actualCrc=0x%2")
//                    .arg(expectedCrc, 4, 16, QLatin1Char('0'))
//                    .arg(actualCrc, 4, 16, QLatin1Char('0'));
//            logMsgWithFrame += crcMsg;
//        }
//
//        if (finished.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << logMsgWithFrame;
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][processPendingFrame]：%1").arg(logMsgWithFrame).toStdString());
//        }
        if (shouldLogCommand(finished)) {
            ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandReceiver", "processPendingFrame",
                QString("响应帧校验失败 module=%1 id=%2 uuid=%3 error=%4 frame=%5")
                    .arg(commandModuleToString(finished.module)).arg(finished.id).arg(finished.uuid)
                    .arg(errorMessage).arg(toHexSpaced(frame)));
        }
        return processPendingFrame();
    }

//    QString logMsg = QString("[接收-帧] %1 设备ID=%2 module=%3 id=%4 uuid=%5 len=%6")
//            .arg(nowStr())
//            .arg(m_masterId)
//            .arg(moduleStr)
//            .arg(finished.id)
//            .arg(finished.uuid)
//            .arg(frame.size());
//
//    QString frameHex = toHexSpaced(frame);
//    QString logMsgWithFrame = logMsg + "\nframe=" + frameHex;
//
//    if (finished.module != CommandModule::PeriodicCommandSender) {
//        qDebug() << logMsgWithFrame;
//        LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusCommandReceiver][processPendingFrame]：%1").arg(logMsgWithFrame).toStdString());
//    }

    succeedPendingCommand(frame);
    return true;
}

int ModbusCommandReceiver::expectedResponseLength(const ModbusCommand& cmd) const
{
    // 对于功能码 0x01-0x04，响应长度由帧内的字节计数字段决定
    // 这里只返回最小长度（从站+功能码+字节计数+CRC=5字节）
    // 实际提取时根据字节计数动态计算完整长度
    const quint8 fc = cmd.request.functionCode;

    switch (fc) {
    case 0x01: // Read Coils
    case 0x02: // Read Discrete Inputs
    case 0x03: // Read Holding Registers
    case 0x04: // Read Input Registers
        // 最小响应长度：从站(1) + 功能码(1) + 字节计数(1) + CRC(2) = 5
        return 5;
    case 0x05: // Write Single Coil
    case 0x06: // Write Single Register
    case 0x0F: // Write Multiple Coils
    case 0x10: // Write Multiple Registers
        // 响应格式: 从站(1) + 功能码(1) + 起始地址(2) + 数量(2) + CRC(2)
        return 1 + 1 + 2 + 2 + 2;
    default:
//        qDebug() << "[expectedResponseLength] 未知功能码:" << fc;
        return 0;
    }
}

bool ModbusCommandReceiver::tryExtractMatchedFrame(const ModbusCommand& cmd, QByteArray& frame, int& discardedPrefixBytes, QByteArray& discardedData)
{
    discardedPrefixBytes = 0;

    const int minLength = expectedResponseLength(cmd);
    if (minLength <= 0 || ringSize() < minLength) {
        return false;
    }

    const quint8 expectedSlave = cmd.response.slaveAddr;
    const quint8 expectedFunction = cmd.response.functionCode;
    const quint8 fc = cmd.request.functionCode;
    const bool isReadFc = (fc == 0x01 || fc == 0x02 || fc == 0x03 || fc == 0x04);

    // 滑动查找匹配帧头（从站+功能码）的起始位置
    const int maxOffset = ringSize() - minLength;
    for (int offset = 0; offset <= maxOffset; ++offset) {
        // 先读取最小长度以确认帧头
        QByteArray head = ringPeek(offset, minLength);
        if (head.size() != minLength) {
            return false;
        }

        const quint8 slave    = static_cast<quint8>(head[0]);
        const quint8 function = static_cast<quint8>(head[1]);
        if (slave != expectedSlave || function != expectedFunction) {
            continue;
        }

        // 根据功能码动态计算实际帧总长度
        int actualLength = minLength;
        if (isReadFc) {
            // 读类响应: 从站(1) + 功能码(1) + 字节计数(1) + 数据(N) + CRC(2)
            const quint8 frameByteCount = static_cast<quint8>(head[2]);
            // 非标准设备：响应功能码与请求功能码不同时，帧内BC字段可能不等于实际数据字节数。
            // 此时使用 XML 配置的 response.byteCount 作为帧长依据，确保完整帧被提取。
            const quint8 effectiveBC = (cmd.response.functionCode != cmd.request.functionCode
                                        && cmd.response.byteCount > 0)
                                       ? cmd.response.byteCount
                                       : frameByteCount;
            actualLength = 1 + 1 + 1 + effectiveBC + 2;
        }
        // 写类响应固定长度，actualLength 保持 minLength 即可

        // 缓冲区数据不足整个帧 → 等待更多数据
        if (ringSize() - offset < actualLength) {
            // 注意：不要把前面不匹配的前缀保留太多；此处只等待更多数据，下一次readyRead再尝试
            return false;
        }

        // 提取完整帧
        QByteArray candidate = ringPeek(offset, actualLength);
        if (candidate.size() != actualLength) {
            return false;
        }

        if (offset > 0) {
            discardedData = ringPeek(0, offset);
        }
        ringConsume(offset + actualLength);
        discardedPrefixBytes = offset;
        frame = candidate;
        return true;
    }

    // 整段缓冲都没找到匹配帧头：保留末尾 minLength-1 字节，丢弃其余
    const int preserveBytes = minLength - 1;
    const int dirtyBytes = ringSize() - preserveBytes;
    if (dirtyBytes > 0) {
//        if (cmd.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << "[接收-脏帧] [设备ID=" << m_masterId << "] " << nowStr()
//                     << "id=" << cmd.id
//                     << "uuid=" << cmd.uuid
//                     << "dropBytes=" << dirtyBytes
//                     << "reason=未找到合法RTU响应帧";
//            QString logMsg = QString("丢弃脏数据 - 设备ID=%1 id=%2 uuid=%3 dropBytes=%4 reason=未找到合法RTU响应帧").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(dirtyBytes);
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][tryExtractMatchedFrame]：%1").arg(logMsg).toStdString());
//        }
        if (shouldLogCommand(cmd)) {
            ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusCommandReceiver", "tryExtractMatchedFrame",
                QString("丢弃脏数据 module=%1 id=%2 uuid=%3 dropBytes=%4 reason=未找到合法RTU响应帧")
                    .arg(commandModuleToString(cmd.module)).arg(cmd.id).arg(cmd.uuid).arg(dirtyBytes));
        }
        ringConsume(dirtyBytes);
        discardedPrefixBytes += dirtyBytes;
    }

    return false;
}

bool ModbusCommandReceiver::isFrameHeaderMatch(const ModbusCommand& cmd, const QByteArray& frame) const
{
    if (frame.size() < 2) {
        return false;
    }
    return static_cast<quint8>(frame[0]) == cmd.response.slaveAddr
        && static_cast<quint8>(frame[1]) == cmd.response.functionCode;
}

bool ModbusCommandReceiver::validateResponseFrame(const ModbusCommand& cmd, const QByteArray& frame, QString& errorMessage) const
{
    if (frame.size() < 4) {
        errorMessage = "响应帧长度过短";
//        if (cmd.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                     << "id=" << cmd.id
//                     << "uuid=" << cmd.uuid
//                     << "reason=帧长度过短"
//                     << "actual=" << frame.size()
//                     << "min=4";
//            QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=帧长度过短 actual=%4 min=5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(frame.size());
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//        }
        return false;
    }

    if (!isFrameHeaderMatch(cmd, frame)) {
        errorMessage = "响应帧头不匹配";
//        if (cmd.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                     << "id=" << cmd.id
//                     << "uuid=" << cmd.uuid
//                     << "reason=帧头不匹配"
//                     << "expectedSlave=" << QString::number(cmd.response.slaveAddr, 16)
//                     << "actualSlave=" << QString::number(static_cast<quint8>(frame[0]), 16)
//                     << "expectedFunc=" << QString::number(cmd.response.functionCode, 16)
//                     << "actualFunc=" << QString::number(static_cast<quint8>(frame[1]), 16);
//            QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=帧头不匹配 expectedSlave=0x%4 actualSlave=0x%5 expectedFunc=0x%6 actualFunc=0x%7")
//                    .arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(cmd.response.slaveAddr, 0, 16).arg(static_cast<quint8>(frame[0]), 0, 16).arg(cmd.response.functionCode, 0, 16).arg(static_cast<quint8>(frame[1]), 0, 16);
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//        }
        return false;
    }

    const QByteArray payload = frame.left(frame.size() - 2);
    const quint16 expectedCrc = crc16(payload);
    const quint16 actualCrc = static_cast<quint8>(frame[frame.size() - 2]) |
                              (static_cast<quint16>(static_cast<quint8>(frame[frame.size() - 1])) << 8);
    if (expectedCrc != actualCrc) {
        errorMessage = QString("响应帧CRC校验失败 expectedCrc=0x%1 actualCrc=0x%2")
                           .arg(expectedCrc, 4, 16, QChar('0'))
                           .arg(actualCrc, 4, 16, QChar('0'));
//        if (cmd.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                     << "id=" << cmd.id
//                     << "uuid=" << cmd.uuid
//                     << "reason=CRC不匹配"
//                     << "expected=0x" << QString::number(expectedCrc, 16)
//                     << "actual=0x" << QString::number(actualCrc, 16);
//            QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=CRC不匹配 expected=0x%4 actual=0x%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(expectedCrc, 0, 16).arg(actualCrc, 0, 16);
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//        }
        return false;
    }

    if (static_cast<quint8>(payload[0]) != cmd.response.slaveAddr) {
        errorMessage = "从站地址不匹配";
//        if (cmd.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                     << "id=" << cmd.id
//                     << "uuid=" << cmd.uuid
//                     << "reason=从站地址不匹配"
//                     << "expected=" << cmd.response.slaveAddr
//                     << "actual=" << static_cast<quint8>(payload[0]);
//            QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=从站地址不匹配 expected=%4 actual=%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(cmd.response.slaveAddr).arg(static_cast<quint8>(payload[0]));
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//        }
        return false;
    }

    if (static_cast<quint8>(payload[1]) != cmd.response.functionCode) {
        errorMessage = QString("功能码不匹配: 0x%1")
                           .arg(static_cast<quint8>(payload[1]), 2, 16, QChar('0'));
//        if (cmd.module != CommandModule::PeriodicCommandSender) {
//            qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                     << "id=" << cmd.id
//                     << "uuid=" << cmd.uuid
//                     << "reason=功能码不匹配"
//                     << "expected=0x" << QString::number(cmd.response.functionCode, 16)
//                     << "actual=0x" << QString::number(static_cast<quint8>(payload[1]), 16);
//            QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=功能码不匹配 expected=0x%4 actual=0x%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(cmd.response.functionCode, 0, 16).arg(static_cast<quint8>(payload[1]), 0, 16);
//            LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//        }
        return false;
    }

    switch (cmd.response.functionCode) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04: {
            if (payload.size() < 3) {
                errorMessage = "读响应帧长度不足";
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=读响应帧长度不足"
//                             << "actual=" << payload.size()
//                             << "min=3";
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=读响应帧长度不足 actual=%4 min=3").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(payload.size());
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            const quint8 byteCount = static_cast<quint8>(payload[2]);
            if (byteCount != cmd.response.byteCount) {
                errorMessage = QString("响应字节数不匹配: %1").arg(byteCount);
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=字节数不匹配"
//                             << "expected=" << cmd.response.byteCount
//                             << "actual=" << byteCount;
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=字节数不匹配 expected=%4 actual=%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(cmd.response.byteCount).arg(byteCount);
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            if (payload.size() != 3 + byteCount) {
                errorMessage = "读响应帧长度与字节数不一致";
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=读响应帧长度与字节数不一致"
//                             << "expected=" << (3 + byteCount)
//                             << "actual=" << payload.size();
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=读响应帧长度与字节数不一致 expected=%4 actual=%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(3 + byteCount).arg(payload.size());
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            break;
        }
        case 0x05:
        case 0x06: {
            if (payload.size() != 6) {
                errorMessage = "写响应帧长度错误";
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=写响应帧长度错误"
//                             << "expected=6"
//                             << "actual=" << payload.size();
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=写响应帧长度错误 expected=6 actual=%4").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(payload.size());
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            const quint16 startAddr = (static_cast<quint8>(payload[2]) << 8) |
                                      static_cast<quint8>(payload[3]);
            if (startAddr != cmd.response.startAddr) {
                errorMessage = QString("响应起始地址不匹配: 0x%1").arg(startAddr, 4, 16, QChar('0'));
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=起始地址不匹配"
//                             << "expected=0x" << QString::number(cmd.response.startAddr, 16)
//                             << "actual=0x" << QString::number(startAddr, 16);
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=起始地址不匹配 expected=0x%4 actual=0x%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(cmd.response.startAddr, 0, 16).arg(startAddr, 0, 16);
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            const QByteArray echoedValue = payload.mid(4, 2);
            if (echoedValue != cmd.response.registerValue) {
                errorMessage = QString("响应写入值不匹配: %1").arg(toHexSpaced(echoedValue));
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=写入值不匹配"
//                             << "expected=" << toHexSpaced(cmd.response.registerValue)
//                             << "actual=" << toHexSpaced(echoedValue);
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=写入值不匹配 expected=%4 actual=%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(toHexSpaced(cmd.response.registerValue)).arg(toHexSpaced(echoedValue));
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            break;
        }
        case 0x0F:
        case 0x10: {
            if (payload.size() != 6) {
                errorMessage = "写响应帧长度错误";
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=写响应帧长度错误"
//                             << "expected=6"
//                             << "actual=" << payload.size();
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=写响应帧长度错误 expected=6 actual=%4").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(payload.size());
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            const quint16 startAddr = (static_cast<quint8>(payload[2]) << 8) |
                                      static_cast<quint8>(payload[3]);
            if (startAddr != cmd.response.startAddr) {
                errorMessage = QString("响应起始地址不匹配: 0x%1").arg(startAddr, 4, 16, QChar('0'));
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=起始地址不匹配"
//                             << "expected=0x" << QString::number(cmd.response.startAddr, 16)
//                             << "actual=0x" << QString::number(startAddr, 16);
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=起始地址不匹配 expected=0x%4 actual=0x%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(cmd.response.startAddr, 0, 16).arg(startAddr, 0, 16);
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            const quint16 count = (static_cast<quint8>(payload[4]) << 8) |
                                  static_cast<quint8>(payload[5]);
            if (count != cmd.response.count) {
                errorMessage = QString("响应数量不匹配: 0x%1").arg(count, 4, 16, QChar('0'));
//                if (cmd.module != CommandModule::PeriodicCommandSender) {
//                    qDebug() << "[验证-失败] [设备ID=" << m_masterId << "] " << nowStr()
//                             << "id=" << cmd.id
//                             << "uuid=" << cmd.uuid
//                             << "reason=数量不匹配"
//                             << "expected=" << cmd.response.count
//                             << "actual=" << count;
//                    QString logMsg = QString("验证失败 - 设备ID=%1 id=%2 uuid=%3 reason=数量不匹配 expected=%4 actual=%5").arg(m_masterId).arg(cmd.id).arg(cmd.uuid).arg(cmd.response.count).arg(count);
//                    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::WARN, QString("[data][ModbusCommandReceiver][validateResponseFrame]：%1").arg(logMsg).toStdString());
//                }
                return false;
            }
            break;
        }
        default:
            break;
    }

    return true;
}

void ModbusCommandReceiver::fillResponseFrame(ModbusCommand& cmd, const QByteArray& frame) const
{
    const QByteArray payload = frame.left(frame.size() - 2);
    cmd.response.rawBytes = payload;
    if (payload.size() >= 2) {
        cmd.response.slaveAddr = static_cast<quint8>(payload[0]);
        cmd.response.functionCode = static_cast<quint8>(payload[1]);
    }

    switch (cmd.response.functionCode) {
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x12: // 非标准读寄存器响应（本设备对 FC04 返回 FC=0x12）
            if (payload.size() >= 3) {
                cmd.response.byteCount = static_cast<quint8>(payload[2]);
                cmd.response.registerValue = payload.mid(3);
            }
            break;
        case 0x05:
        case 0x06:
        case 0x0F:
        case 0x10:
            if (payload.size() >= 6) {
                cmd.response.startAddr = (static_cast<quint8>(payload[2]) << 8) |
                                         static_cast<quint8>(payload[3]);
                cmd.response.count = (static_cast<quint8>(payload[4]) << 8) |
                                     static_cast<quint8>(payload[5]);
                if (cmd.response.functionCode == 0x05 || cmd.response.functionCode == 0x06) {
                    cmd.response.registerValue = payload.mid(4, 2);
                }
            }
            break;
    }
}

void ModbusCommandReceiver::failPendingCommand(const QString& errorMessage, bool timedOut, bool checksumError)
{
    if (!m_hasPendingCommand) {
        return;
    }

    m_responseTimer->stop();
    ModbusCommand failed = m_pendingCommand;
    m_hasPendingCommand = false;

    failed.errorMessage = errorMessage;
    failed.timedOut = timedOut;
    failed.checksumError = checksumError;

    logRawFrame(failed.id, failed.request.rawBytes, QByteArray(),
                errorMessage + (timedOut ? QStringLiteral(" (timeout)") : QString()));

    emit commandFailed(failed, timedOut, checksumError);
}

void ModbusCommandReceiver::succeedPendingCommand(const QByteArray& frame)
{
    if (!m_hasPendingCommand) {
        return;
    }

    m_responseTimer->stop();
    ModbusCommand finished = m_pendingCommand;
    m_hasPendingCommand = false;

    if (frame.size() >= 2) {
        finished.response.crc = frame.right(2);
    }
    fillResponseFrame(finished, frame);
    finished.received = true;
    finished.timedOut = false;
    finished.checksumError = false;
    finished.errorMessage.clear();
    finished.responseMs = QDateTime::currentMSecsSinceEpoch();

    QString moduleStr;
    switch (finished.module) {
        case CommandModule::InitialCommandIssuer:  moduleStr = "INITIAL"; break;
        case CommandModule::PeriodicCommandSender: moduleStr = "PERIODIC"; break;
        case CommandModule::BusinessCommandIssuer: moduleStr = "BUSINESS"; break;
    }

    const qint64 elapsed = finished.responseMs - finished.sentMs;
//    if (finished.module != CommandModule::PeriodicCommandSender) {
//        qDebug() << "[接收-成功] [设备ID=" << m_masterId << "] " << nowStr()
//                 << "module=" << moduleStr
//                 << "id=" << finished.id
//                 << "uuid=" << finished.uuid
//                 << "crc=" << toHexSpaced(finished.response.crc)
//                 << "elapsed=" << elapsed << "ms";
//        QString logMsg = QString("接收成功 - 设备ID=%1 module=%2 id=%3 uuid=%4 crc=%5 elapsed=%6ms").arg(m_masterId).arg(moduleStr).arg(finished.id).arg(finished.uuid).arg(toHexSpaced(finished.response.crc)).arg(elapsed);
//        LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusCommandReceiver][succeedPendingCommand]：%1").arg(logMsg).toStdString());
//    }
    if (shouldLogCommand(finished)) {
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusCommandReceiver", "succeedPendingCommand",
            QString("接收成功 module=%1 id=%2 uuid=%3 crc=%4 elapsed=%5ms")
                .arg(commandModuleToString(finished.module)).arg(finished.id).arg(finished.uuid)
                .arg(toHexSpaced(finished.response.crc)).arg(elapsed));
    }

    logRawFrame(finished.id, finished.request.rawBytes, finished.response.rawBytes, QString());

    emit commandSucceeded(finished);
}

void ModbusCommandReceiver::logRawFrame(const QString& cmdId,
                                         const QByteArray& requestBytes,
                                         const QByteArray& responseBytes,
                                         const QString& errorInfo)
{
    const QString txHex = toHexSpaced(requestBytes);

    QStringList lines;
    lines << QStringLiteral("\n[%1] TX: %2").arg(cmdId, txHex);

    if (!responseBytes.isEmpty()) {
        const QString rxHex = toHexSpaced(responseBytes);
        lines << QStringLiteral("[%1] RX: %2").arg(cmdId, rxHex);
    } else {
        lines << QStringLiteral("[%1] ERROR: %2").arg(cmdId, errorInfo);
    }

    //    const QString logPath = QStringLiteral("raw_data/%1.log").arg(m_masterId);
//    LoggerManager::getInstance()->log(logPath.toStdString(),
//                                  responseBytes.isEmpty() ? Level::WARN : Level::INFO,
//                                  lines.join(QStringLiteral("\n")).toStdString());
    // LoggerManager::instance().flush(logPath.toStdString());
}

void ModbusCommandReceiver::ringAppend(const QByteArray& data)
{
    for (char ch : data) {
        if (m_ringSize >= m_ringBuffer.size()) {
            m_ringHead = (m_ringHead + 1) % m_ringBuffer.size();
            --m_ringSize;
        }
        const int tail = (m_ringHead + m_ringSize) % m_ringBuffer.size();
        m_ringBuffer[tail] = static_cast<quint8>(ch);
        ++m_ringSize;
    }
}

QByteArray ModbusCommandReceiver::ringPeek(int offset, int length) const
{
    QByteArray data;
    if (offset < 0 || length <= 0 || offset + length > m_ringSize) {
        return data;
    }

    data.reserve(length);
    for (int i = 0; i < length; ++i) {
        const int index = (m_ringHead + offset + i) % m_ringBuffer.size();
        data.append(static_cast<char>(m_ringBuffer[index]));
    }
    return data;
}

void ModbusCommandReceiver::ringConsume(int length)
{
    if (length <= 0) {
        return;
    }
    const int actual = qMin(length, m_ringSize);
    m_ringHead = (m_ringHead + actual) % m_ringBuffer.size();
    m_ringSize -= actual;
}

int ModbusCommandReceiver::ringSize() const
{
    return m_ringSize;
}

void ModbusCommandReceiver::clearRingBuffer()
{
    m_ringHead = 0;
    m_ringSize = 0;
}

quint16 ModbusCommandReceiver::crc16(const QByteArray& data)
{
    return ModbusCrc::crc16(data);
}
