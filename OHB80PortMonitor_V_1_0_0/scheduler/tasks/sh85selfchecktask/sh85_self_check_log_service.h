#ifndef SH85_SELF_CHECK_LOG_SERVICE_H
#define SH85_SELF_CHECK_LOG_SERVICE_H

#include "sh85_self_check_types.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QString>
#include <QStringList>

class ModbusCommand;

// ====================================================================
// SH85SelfCheckLogService - SH85 自检日志服务
//
// 设计目标：
//   1. 将通讯日志落库逻辑从 Task3 调度编排中拆出。
//   2. 后续增加每任务/每设备日志文件时，只扩展该服务，不回改调度代码。
// ====================================================================
class SH85SelfCheckLogService
{
public:
    void writeTaskConstructed() const;
    void writeTaskStarted(int periodSec,
                          bool singleDeviceMode,
                          const QString& singleDeviceQrcode) const;
    void writeTaskStartIgnored(const QString& reason) const;
    void writeTaskStopping(int pendingCount) const;
    void writeTaskStopped() const;
    void writeEnabledChanged(bool enabled) const;
    void writePeriodChanged(int value, const QString& unit, int totalSec) const;
    void writeSingleDeviceChanged(bool singleDeviceMode, const QString& qrcode) const;
    void writeBootDelayScheduled(int delaySeconds) const;
    void writeBootDelaySkipped(const QString& reason) const;
    void writeTriggerSkipped(const QString& roundId, int pendingCount) const;
    void writeTaskStateChanged(const QString& stateText) const;
    void writeNextRoundScheduled(int periodSec) const;

    void writeRoundStarted(const QString& roundId,
                           const QString& startTime,
                           int totalCount,
                           const QStringList& orderedQrcodes) const;
    void writeRoundFinished(const QString& roundId,
                            const QString& startTime,
                            const QString& endTime,
                            const SH85SelfCheck::RoundSummary& summary,
                            const QStringList& failedDevices,
                            const QStringList& skippedDevices) const;

    void writeDeviceSkipped(const QString& roundId,
                            const QString& qrcode,
                            const QString& reason) const;
    void writeDeviceStarted(const QString& roundId, const QString& qrcode) const;
    void writeDeviceStartFailed(const QString& roundId,
                                const QString& qrcode,
                                SH85SelfChecker::Result result,
                                const QString& reason) const;
    void writeDeviceFinished(const QString& roundId,
                             const QString& qrcode,
                             bool success,
                             SH85SelfChecker::Result result,
                             const QString& description) const;
    void writeCheckerStateChanged(const QString& roundId,
                                  const QString& qrcode,
                                  SH85SelfChecker::State state) const;
    void writeCheckerCommandRetrying(const QString& roundId,
                                     const QString& qrcode,
                                     const ModbusCommand& cmd) const;
    void writeCheckerError(const QString& roundId,
                           const QString& qrcode,
                           SH85SelfChecker::Result result,
                           const QString& message) const;

    void writeCommunicateLog(const ModbusCommand& cmd, const QString& masterId) const;
};

#endif // SH85_SELF_CHECK_LOG_SERVICE_H
