#ifndef VEFC_SENSOR_MONITOR_ROUND_RUNNER_H
#define VEFC_SENSOR_MONITOR_ROUND_RUNNER_H

#include "vefc_sensor_monitor_types.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>

// ====================================================================
// VEFCSensorMonitorRoundRunner - VEFC 监控轮次执行器
//
// 设计目标：
//   1. 统一管理所有 ModbusCommandSender 的信号连接与断开。
//   2. 统一负责业务指令提交和 pending command 跟踪。
//   3. 只负责执行层中继，不直接判断业务成败，也不写轮次上下文。
// ====================================================================
class VEFCSensorMonitorRoundRunner : public QObject
{
    Q_OBJECT

public:
    explicit VEFCSensorMonitorRoundRunner(QObject* parent = nullptr);
    ~VEFCSensorMonitorRoundRunner() override;

    // 连接或断开全部 sender；Task 只在 start()/stop() 时统一调用。
    void connectAllSenders();
    void disconnectAllSenders();

    // 提交单条业务指令，并将其加入 pending 集合等待后续回调收口。
    bool submitCommand(const QString& qrCode,
                       VEFCSensorMonitor::SensorCommandType type,
                       const char* commandId);

    // pending 集合只跟踪“本轮已提交但尚未收口”的业务指令。
    void clearPendingCommands();
    bool hasPendingCommands() const { return !m_pendingCommands.isEmpty(); }
    int pendingCount() const { return m_pendingCommands.size(); }
    bool containsPendingCommand(qint64 uuid) const;
    VEFCSensorMonitor::PendingCommand pendingCommand(qint64 uuid) const;
    VEFCSensorMonitor::PendingCommand takePendingCommand(qint64 uuid);

signals:
    // ---- 中继底层 sender 回调，供 Task 统一处理业务结果 ----
    void commandFinished(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);

private:
    // ---- 执行层内部状态：sender 连接和待收口命令集合 ----
    QList<QMetaObject::Connection> m_connections;
    QHash<qint64, VEFCSensorMonitor::PendingCommand> m_pendingCommands;
};

#endif // VEFC_SENSOR_MONITOR_ROUND_RUNNER_H
