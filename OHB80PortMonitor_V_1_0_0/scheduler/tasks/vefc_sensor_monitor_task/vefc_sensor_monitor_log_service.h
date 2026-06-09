#ifndef VEFC_SENSOR_MONITOR_LOG_SERVICE_H
#define VEFC_SENSOR_MONITOR_LOG_SERVICE_H

#include "vefc_sensor_monitor_task_logger.h"
#include "vefc_sensor_monitor_types.h"

#include <QString>

class ModbusCommand;

// ====================================================================
// VEFCSensorMonitorLogService - VEFC 监控日志服务
//
// 设计目标：
//   1. 将任务日志、轮次日志、设备日志的输出职责从 Task 中拆出来。
//   2. 后续如果需要扩展更细粒度日志文件或输出格式，只修改本类即可。
//   3. 本类只负责写日志，不参与业务判断和轮次状态维护。
// ====================================================================
class VEFCSensorMonitorLogService
{
public:
    VEFCSensorMonitorLogService();

    // ================================ 任务级日志 ================================
    void writeTaskConstructed();
    void writeTaskStarted(int intervalMs, const QString& startTime);
    void writeTaskStopped(qint64 runtimeMs, const QString& stopTime);
    void writeTriggerSkipped(const QString& roundId, int pendingCount);

    // ================================ 轮次级日志 ================================
    void writeRoundStarted(const QString& roundId,
                           const QString& startTime,
                           qint64 recordTimestamp,
                           int totalCount);
    void writeRoundCancelled(const QString& roundId, const QString& reason);
    void writeRoundFinished(const VEFCSensorMonitor::RoundSummary& summary);

    // ================================ 设备级日志 ================================
    void writeDeviceSkipped(const QString& roundId,
                            const QString& qrCode,
                            const QString& reason,
                            const QString& recordTime);
    void writeCommandRetrying(const QString& roundId,
                              const QString& qrCode,
                              const ModbusCommand& cmd);
    void writeCommandSucceeded(const QString& roundId,
                               const QString& qrCode,
                               const ModbusCommand& cmd);
    void writeCommandFailed(const QString& roundId,
                            const QString& qrCode,
                            const ModbusCommand& cmd,
                            const QString& reason);
    void writePersistFailed(const QString& roundId,
                            const VEFCSensorMonitor::DeviceRoundState& state,
                            const QString& reason);
    void writeRecordPersisted(const QString& roundId,
                              const VEFCSensorMonitor::DeviceRoundState& state);

private:
    // ================================ 日志辅助方法 ================================
    static QString commandFailureReason(const ModbusCommand& cmd);
    static QString commandRequestFrame(const ModbusCommand& cmd);
    static QString commandResponseFrame(const ModbusCommand& cmd);

private:
    VEFCSensorMonitorTaskLogger m_logger;
};

#endif // VEFC_SENSOR_MONITOR_LOG_SERVICE_H
