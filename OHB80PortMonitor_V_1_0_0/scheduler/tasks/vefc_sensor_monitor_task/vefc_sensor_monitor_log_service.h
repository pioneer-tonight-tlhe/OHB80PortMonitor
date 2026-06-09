#ifndef VEFC_SENSOR_MONITOR_LOG_SERVICE_H
#define VEFC_SENSOR_MONITOR_LOG_SERVICE_H

#include "vefc_sensor_monitor_task_logger.h"
#include "vefc_sensor_monitor_types.h"

#include <QString>

class ModbusCommand;

class VEFCSensorMonitorLogService
{
public:
    VEFCSensorMonitorLogService();

    void writeTaskConstructed();
    void writeTaskStarted(int intervalMs, const QString& startTime);
    void writeTaskStopped(qint64 runtimeMs, const QString& stopTime);
    void writeTriggerSkipped(const QString& roundId, int pendingCount);

    void writeRoundStarted(const QString& roundId,
                           const QString& startTime,
                           qint64 recordTimestamp,
                           int totalCount);
    void writeRoundCancelled(const QString& roundId, const QString& reason);
    void writeRoundFinished(const VEFCSensorMonitor::RoundSummary& summary);

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
    static QString commandFailureReason(const ModbusCommand& cmd);
    static QString commandRequestFrame(const ModbusCommand& cmd);
    static QString commandResponseFrame(const ModbusCommand& cmd);

private:
    VEFCSensorMonitorTaskLogger m_logger;
};

#endif // VEFC_SENSOR_MONITOR_LOG_SERVICE_H
