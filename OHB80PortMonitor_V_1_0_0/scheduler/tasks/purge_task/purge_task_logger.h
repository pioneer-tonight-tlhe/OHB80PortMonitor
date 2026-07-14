#ifndef PURGE_TASK_LOGGER_H
#define PURGE_TASK_LOGGER_H

#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "purge_task_types.h"

#include <QString>

class PurgeTaskLogger
{
public:
    PurgeTaskLogger();

    void logTaskStarting(const QString &taskId, const PurgeTaskDefinition &definition);
    void logTaskStarted(const QString &taskId,
                        const PurgeTaskDefinition &definition,
                        const QString &outputDir);
    void logTaskFinished(const QString &taskId,
                         const QString &qrCode,
                         bool success,
                         const QString &message,
                         qint64 elapsedMs);

    void logStagePreparing(const QString &taskId,
                           const QString &qrCode,
                           int stageNo,
                           const PurgeStageDefinition &stage);
    void logStageStarted(const QString &taskId,
                         const QString &qrCode,
                         int stageNo,
                         const PurgeStageDefinition &stage);
    void logStageFinished(const QString &taskId,
                          const QString &qrCode,
                          int stageNo,
                          const PurgeStageDefinition &stage,
                          qint64 elapsedMs);

    void logCommandSubmitting(const QString &taskId,
                              const QString &qrCode,
                              int stageNo,
                              int actionNo,
                              const PurgeActionDefinition &action,
                              const ModbusCommand &cmd);
    void logCommandSubmitFailed(const QString &taskId,
                                const QString &qrCode,
                                int stageNo,
                                int actionNo,
                                const PurgeActionDefinition &action,
                                const QString &reason);
    void logCommandRetry(const QString &taskId,
                         const QString &qrCode,
                         int stageNo,
                         int actionNo,
                         const ModbusCommand &cmd);
    void logCommandFinished(const QString &taskId,
                            const QString &qrCode,
                            int stageNo,
                            int actionNo,
                            const PurgeActionDefinition &action,
                            const ModbusCommand &cmd,
                            bool success,
                            const QString &message);

    void logError(const QString &taskId,
                  const QString &qrCode,
                  const QString &operation,
                  const QString &message);
    void flush();

private:
    static QString requestFrame(const ModbusCommand &cmd);
    static QString responseFrame(const ModbusCommand &cmd);
    static QString commandFailureReason(const ModbusCommand &cmd, const QString &fallbackMessage);
    static QString actionParams(const PurgeActionDefinition &action);
    static QString dateTimeText(qint64 timestampMs);

private:
    ILogger m_summaryLogger;
    ILogger m_commandLogger;
};

#endif // PURGE_TASK_LOGGER_H
