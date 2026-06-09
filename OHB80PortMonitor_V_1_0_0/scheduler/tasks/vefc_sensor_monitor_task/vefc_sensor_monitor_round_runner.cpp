#include "vefc_sensor_monitor_round_runner.h"

#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

// ====================================================================
// VEFCSensorMonitorRoundRunner - 执行器实现
//
// 说明：
//   1. 本类不关心“业务上是否成功”，只负责 sender 连接、提交命令和保留 pending。
//   2. 具体解析和落库逻辑仍由 Task 在收到中继信号后完成。
// ====================================================================
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

    // 按 MasterManager 当前已有设备列表建立中继连接。
    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    const QStringList ids = manager.masterIds();
    for (const QString& id : ids) {
        ModbusTcpMaster* master = manager.getMaster(id);
        if (!master || !master->sender()) {
            continue;
        }

        // 跨线程统一使用 QueuedConnection，避免 sender 所在线程与 Task 线程直接耦合。
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
    // 提交前只检查必要依赖是否可用；业务成败由后续 commandFinished 决定。
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
