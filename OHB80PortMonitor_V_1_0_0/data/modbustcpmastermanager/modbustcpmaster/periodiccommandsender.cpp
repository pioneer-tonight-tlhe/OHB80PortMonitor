#include "periodiccommandsender.h"
#include "modbuslogger.h"
#include <QDebug>
#include <QDateTime>
#include <QStringList>

namespace {
static inline QString nowStr()
{
    return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}
static inline QString toHexSpaced(const QByteArray& data)
{
    QString s;
    s.reserve(data.size() * 3);
    for (unsigned char b : data)
        s += QString::asprintf("%02X ", b);
    if (!s.isEmpty()) s.chop(1);
    return s;
}

static inline bool isCommunicationFailure(const ModbusCommand& cmd)
{
    if (cmd.received || cmd.deviceBusy || cmd.checksumError) {
        return false;
    }
    if (cmd.timedOut) {
        return true;
    }

    const QString error = cmd.errorMessage;
    return error.contains("TCP", Qt::CaseInsensitive)
        || error.contains("socket", Qt::CaseInsensitive)
        || error.contains("连接")
        || error.contains("发送")
        || error.contains("写出")
        || error.contains("RTU响应超时");
}

static inline QString summarizeFailedCommands(const QList<ModbusCommand>& failedCommands)
{
    QStringList parts;
    for (const auto& cmd : failedCommands) {
        parts << QString("id=%1 uuid=%2 timeout=%3 error=%4")
                     .arg(cmd.id)
                     .arg(cmd.uuid)
                     .arg(cmd.timedOut)
                     .arg(cmd.errorMessage);
    }
    return parts.join("; ");
}
}

// ============================================================
// PeriodicCommandSender - 定时循环指令发送器实现
// ============================================================

PeriodicCommandSender::PeriodicCommandSender(ModbusCommandSender& sender, const QString& masterId, QObject* parent)
    : CyclicCommandIssuer(sender, parent)
    , m_masterId(const_cast<QString&>(masterId))
{
    setExecutionCount(0);  // 无限循环
    // 指令成功信号转发为 commandCompleted，并附加 masterId
    connect(this, &CyclicCommandIssuer::commandSucceeded,
            this, [this](ModbusCommand cmd) {
//                qDebug() << "[PERIODIC-RECV]" << nowStr()
//                         << "设备ID=" << m_masterId
//                         << "id=" << cmd.id
//                         << "len=" << cmd.response.rawBytes.size()
//                         << "frame=" << toHexSpaced(cmd.response.rawBytes);
                emit commandCompleted(cmd, m_masterId);
            });
    connect(this, &CyclicCommandIssuer::roundFinished,
            this, &PeriodicCommandSender::onRoundComplete);
    connect(this, &CyclicCommandIssuer::logMessage,
            this, &PeriodicCommandSender::onLogMessage);
}

void PeriodicCommandSender::onRoundComplete(QList<ModbusCommand> failedCommands)
{
    if (failedCommands.isEmpty()) {
        if (m_consecutiveFailRounds > 0) {
            ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "PeriodicCommandSender", "onRoundComplete",
                QString("定时通讯恢复，连续失败轮次清零，之前连续失败轮次=%1").arg(m_consecutiveFailRounds));
        }
        m_consecutiveFailRounds = 0;
        return;
    }

    const int queueSize = commandQueue().size();
    const bool fullRoundFailed = queueSize > 0 && failedCommands.size() >= queueSize;
    bool hasCommunicationFailure = false;
    for (const auto& cmd : failedCommands) {
        if (isCommunicationFailure(cmd)) {
            hasCommunicationFailure = true;
            break;
        }
    }

    if (!fullRoundFailed || !hasCommunicationFailure) {
        if (m_consecutiveFailRounds > 0) {
            ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "PeriodicCommandSender", "onRoundComplete",
                QString("本轮非完整通讯失败，连续失败轮次清零，失败数=%1/%2，失败明细=%3")
                    .arg(failedCommands.size()).arg(queueSize).arg(summarizeFailedCommands(failedCommands)));
        }
        m_consecutiveFailRounds = 0;
        return;
    }

    m_consecutiveFailRounds++;
    const bool shouldLogFailure = (m_consecutiveFailRounds == 1)
                               || (m_consecutiveFailRounds % 10 == 0)
                               || (m_consecutiveFailRounds >= MAX_CONSECUTIVE_FAILURES);
    if (shouldLogFailure) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "PeriodicCommandSender", "onRoundComplete",
            QString("本轮定时通讯完整失败，失败数=%1/%2，连续失败轮次=%3/%4，失败明细=%5")
                .arg(failedCommands.size()).arg(queueSize)
                .arg(m_consecutiveFailRounds).arg(MAX_CONSECUTIVE_FAILURES)
                .arg(summarizeFailedCommands(failedCommands)));
    }

    if (m_consecutiveFailRounds >= MAX_CONSECUTIVE_FAILURES) {
//        qDebug() << "[PeriodicCommandSender] [设备ID=" << m_masterId << "] 连续失败达到阈値，请求断开设备";
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "PeriodicCommandSender", "onRoundComplete",
            QString("连续定时通讯失败达到阈值=%1，请求断开TCP连接并重连").arg(MAX_CONSECUTIVE_FAILURES));

//        const QString rawPath = QStringLiteral("raw_data/%1.log").arg(m_masterId);
//        LoggerManager::getInstance()->log(rawPath.toStdString(), Level::WARN,
//            QStringLiteral("[PeriodicCommandSender] 连续失败%1轮达到阈值，准备断开连接")
//                .arg(m_consecutiveFailRounds).toStdString());
//        LoggerManager::getInstance()->flush(rawPath.toStdString());

        stop();
        m_consecutiveFailRounds = 0;
        emit disconnectDevice();
    }
}

void PeriodicCommandSender::onLogMessage(QString /*message*/)
{
    // LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[PeriodicCommandSender]：设备ID=%1 %2").arg(m_masterId).arg(message).toStdString());
}
