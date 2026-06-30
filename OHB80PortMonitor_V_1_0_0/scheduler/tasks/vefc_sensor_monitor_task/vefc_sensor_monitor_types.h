#ifndef VEFC_SENSOR_MONITOR_TYPES_H
#define VEFC_SENSOR_MONITOR_TYPES_H

#include "classes/vefcsensormonitorrecord.h"

#include <QList>
#include <QMetaType>
#include <QDate>
#include <QString>
#include <QStringList>
#include <QVector>

// ====================================================================
// VEFCSensorMonitorTypes - VEFC 监控任务的公共数据类型
//
// 设计目标：
//   1. 统一定义 VEFC 监控调度过程中使用的命令类型、设备快照、轮次状态和汇总结果。
//   2. 让 Task / Selector / RoundContext / RoundRunner 共享同一套轻量数据结构，避免重复定义。
//   3. 保持类型语义清晰，便于后续扩展更多 VEFC 监控字段或轮次统计信息。
// ====================================================================
namespace VEFCSensorMonitor {

// 单设备 VEFC 监控中需要下发的两类业务指令。
enum class SensorCommandType {
    Pressure,
    Temperature
};

// 单条待完成业务指令的最小跟踪信息。
struct PendingCommand {
    QString qrCode;
    SensorCommandType type = SensorCommandType::Pressure;
};

// 设备在“本轮准备提交指令之前”的可执行性快照。
struct DeviceInspection {
    QString qrCode;
    bool foupAvailable = false;
    bool masterAvailable = false;
    bool connected = false;
    bool senderAvailable = false;
    double gasPressure = 0.0;
    double actualFlow = 0.0;
    QString unavailableReason;

    bool canSubmitCommands() const
    {
        // 只有基础对象、网络连接和 sender 都可用时，才允许提交本轮业务指令。
        return foupAvailable && masterAvailable && connected && senderAvailable;
    }
};

// 单设备在一轮 VEFC 监控中的累计状态与结果。
struct DeviceRoundState {
    QString qrCode;
    VEFCSensorMonitorRecord record;
    bool pressureFinished = false;
    bool temperatureFinished = false;
    bool pressureOk = false;
    bool temperatureOk = false;
    bool persisted = false;
    bool skipped = false;
    QString failReason;

    bool isFinished() const
    {
        // 两条业务指令都已完成（无论成功还是失败）时，认为该设备本轮已收口。
        return pressureFinished && temperatureFinished;
    }

    bool isSuccessful() const
    {
        // 只有两条指令都成功且记录已落库，才认为本轮设备结果成功。
        return pressureOk && temperatureOk && persisted;
    }
};

// 一整轮 VEFC 监控结束后的轻量汇总。
struct RoundSummary {
    QString roundId;
    QString startTime;
    QString endTime;
    int totalCount = 0;
    int persistedCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    QList<DeviceRoundState> details;
    QStringList failedDevices;
    QStringList skippedDevices;
};

struct DebugMetricStats {
    QString name;
    QString unit;
    int count = 0;
    bool hasData = false;
    double min = 0.0;
    double max = 0.0;
    double average = 0.0;
};

struct DebugSnapshot {
    QDate date;
    bool databaseAvailable = false;
    QString errorMessage;
    QVector<VEFCSensorMonitorRecord> softwareFirstOpenRecords;
    QVector<VEFCSensorMonitorRecord> todayFirstRecords;
    QVector<VEFCSensorMonitorRecord> todayLatestRecords;
    QVector<VEFCSensorMonitorRecord> todayRecords;
    QVector<DebugMetricStats> statistics;
};

} // namespace VEFCSensorMonitor

Q_DECLARE_METATYPE(VEFCSensorMonitor::SensorCommandType)
Q_DECLARE_METATYPE(VEFCSensorMonitor::DeviceRoundState)
Q_DECLARE_METATYPE(QList<VEFCSensorMonitor::DeviceRoundState>)
Q_DECLARE_METATYPE(VEFCSensorMonitor::RoundSummary)
Q_DECLARE_METATYPE(VEFCSensorMonitor::DebugMetricStats)
Q_DECLARE_METATYPE(QVector<VEFCSensorMonitor::DebugMetricStats>)
Q_DECLARE_METATYPE(VEFCSensorMonitor::DebugSnapshot)

#endif // VEFC_SENSOR_MONITOR_TYPES_H
