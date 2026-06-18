#include "initialcommandissuer.h"
#include "modbustcpmaster.h"
#include "modbuslogger.h"
#include <QDebug>

// ============================================================
// InitialCommandIssuer - 初始指令下发器实现
// ============================================================

namespace {
constexpr const char* kReadVersionCommandId = "ReadVersion";

QString parseFirmwareVersion(const QByteArray& registerValue)
{
    if (registerValue.size() < 2) {
        return QString();
    }

    const quint8 majorByte = static_cast<quint8>(registerValue.at(0));
    const quint8 minorByte = static_cast<quint8>(registerValue.at(1));
    const bool majorIsDigit = majorByte >= '0' && majorByte <= '9';
    const bool minorIsDigit = minorByte >= '0' && minorByte <= '9';

    if (majorIsDigit && minorIsDigit) {
        return QString("%1.%2").arg(majorByte - '0').arg(minorByte - '0');
    }

    return QString("%1.%2").arg(majorByte).arg(minorByte);
}
} // namespace

InitialCommandIssuer::InitialCommandIssuer(ModbusCommandSender& sender,
                                           const QString& masterId,
                                           ModbusTcpMaster* master,
                                           QObject* parent)
    : CyclicCommandIssuer(sender, parent)
    , m_masterId(const_cast<QString&>(masterId))
    , m_master(master)
{
    setExecutionCount(1);
    connect(this, &CyclicCommandIssuer::commandSucceeded,
            this, &InitialCommandIssuer::onCommandSucceeded);
    connect(this, &CyclicCommandIssuer::roundFinished,
            this, &InitialCommandIssuer::onRoundComplete);
    connect(this, &CyclicCommandIssuer::logMessage,
            this, &InitialCommandIssuer::onLogMessage);
}

void InitialCommandIssuer::onCommandSucceeded(ModbusCommand cmd)
{
    if (cmd.id != QLatin1String(kReadVersionCommandId)) {
        return;
    }

    const QString version = parseFirmwareVersion(cmd.response.registerValue);
    if (version.isEmpty()) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onCommandSucceeded",
            QString("ReadVersion 响应解析失败，响应寄存器字节数=%1")
                .arg(cmd.response.registerValue.size()));
        return;
    }

    if (!m_master) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onCommandSucceeded",
            QString("ReadVersion 解析成功但 Master 指针为空，版本号=%1").arg(version));
        return;
    }

    m_master->m_firmwareVersion = version;

    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] ReadVersion parsed:" << version;
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onCommandSucceeded",
        QString("ReadVersion 解析完成，固件版本号=%1").arg(version));
}

void InitialCommandIssuer::onRoundComplete(QList<ModbusCommand> failedCommands)
{
    qDebug() << "[InitialCommandIssuer] [设备ID=" << m_masterId << "] 完成，失败" << failedCommands.size() << "条";
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onRoundComplete",
        QString("初始化指令完成，失败数=%1").arg(failedCommands.size()));
    emit finished(failedCommands);
    deleteLater();
}

void InitialCommandIssuer::onLogMessage(QString message)
{
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "InitialCommandIssuer", "onLogMessage", message);
}
