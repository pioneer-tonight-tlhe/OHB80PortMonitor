#include "initialcommandissuer.h"
#include "modbuslogger.h"
#include <QDebug>

// ============================================================
// InitialCommandIssuer - 初始指令下发器实现
// ============================================================

InitialCommandIssuer::InitialCommandIssuer(ModbusCommandSender& sender, const QString& masterId, QObject* parent)
    : CyclicCommandIssuer(sender, parent)
    , m_masterId(const_cast<QString&>(masterId))
{
    setExecutionCount(1);
    connect(this, &CyclicCommandIssuer::roundFinished,
            this, &InitialCommandIssuer::onRoundComplete);
    connect(this, &CyclicCommandIssuer::logMessage,
            this, &InitialCommandIssuer::onLogMessage);
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
