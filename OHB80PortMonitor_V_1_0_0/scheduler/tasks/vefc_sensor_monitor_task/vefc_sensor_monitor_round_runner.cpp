#include "vefc_sensor_monitor_round_runner.h"

#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

VEFCSensorMonitorRoundRunner::VEFCSensorMonitorRoundRunner(QObject* parent)
    : QObject(parent)
{
}

VEFCSensorMonitorRoundRunner::~VEFCSensorMonitorRoundRunner()
{
    disconnectAllSenders();
}

void VEFCSensorMonitorRoundRunner::connectAllSenders()
{
    disconnectAllSenders();

    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    const QStringList ids = manager.masterIds();
    for (const QString& id : ids) {
        ModbusTcpMaster* master = manager.getMaster(id);
        if (!master || !master->sender()) {
            continue;
        }

        ModbusCommandSender* sender = master->sender();
        m_connections.append(connect(sender, &ModbusCommandSender::commandFinished,
                                     this, &VEFCSensorMonitorRoundRunner::commandFinished,
                                     Qt::QueuedConnection));
        m_connections.append(connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                     this, &VEFCSensorMonitorRoundRunner::commandRetrying,
                                     Qt::QueuedConnection));
    }
}

void VEFCSensorMonitorRoundRunner::disconnectAllSenders()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

bool VEFCSensorMonitorRoundRunner::submitCommand(const QString& qrCode,
                                                 VEFCSensorMonitor::SensorCommandType type,
                                                 const char* commandId)
{
    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    ModbusTcpMaster* master = manager.getMaster(qrCode);
    CommandPool* pool = manager.commandPool();
    if (!master || !master->sender() || !pool || !pool->contains(commandId)) {
        return false;
    }

    ModbusCommand cmd = pool->clone(commandId);
    if (!cmd.isValid()) {
        return false;
    }

    cmd.module = CommandModule::BusinessCommandIssuer;
    m_pendingCommands.insert(cmd.uuid, VEFCSensorMonitor::PendingCommand{qrCode, type});

    ModbusCommandSender* sender = master->sender();
    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);

    return true;
}

void VEFCSensorMonitorRoundRunner::clearPendingCommands()
{
    m_pendingCommands.clear();
}

bool VEFCSensorMonitorRoundRunner::containsPendingCommand(qint64 uuid) const
{
    return m_pendingCommands.contains(uuid);
}

VEFCSensorMonitor::PendingCommand VEFCSensorMonitorRoundRunner::pendingCommand(qint64 uuid) const
{
    return m_pendingCommands.value(uuid);
}

VEFCSensorMonitor::PendingCommand VEFCSensorMonitorRoundRunner::takePendingCommand(qint64 uuid)
{
    return m_pendingCommands.take(uuid);
}
