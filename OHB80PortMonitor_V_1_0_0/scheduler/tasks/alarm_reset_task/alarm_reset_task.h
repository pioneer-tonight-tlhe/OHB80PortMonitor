/*******************************************************************************************
 * @file alarm_reset_task.h
 * @author Simon <Job No.13> 2026-07-02
 *
 * @class AlarmResetTask
 * @brief Resets unresolved alarm log records in slow background batches.
 *
 * Design goals:
 *      1. Query unresolved alarm count before the reset starts.
 *      2. Mark unresolved alarm records as resolved in small batches.
 *      3. Report progress to DebugPage while avoiding long scheduler-thread blocking.
 *******************************************************************************************/
#ifndef ALARM_RESET_TASK_H
#define ALARM_RESET_TASK_H

#include "alarmrecord.h"
#include "scheduler/scheduler_task.h"

#include <QList>
#include <QString>

namespace LogDB {
class AlarmLogDBCon;
}

class AlarmResetTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit AlarmResetTask(QObject* parent = nullptr);

    void start() override;
    void stop() override;
    QString taskType() const override { return QStringLiteral("AlarmResetTask"); }

    void setBatchSize(int batchSize);
    void setBatchIntervalMs(int intervalMs);

signals:
    void resetStarted(int totalCount);
    void batchResolved(int totalCount, int resolvedCount, int remainingCount, int batchCount);
    void resetFinished(bool success,
                       const QString& message,
                       int totalCount,
                       int resolvedCount,
                       int remainingCount);

private:
    void processNextBatch();
    void scheduleNextBatch();
    int queryUnresolvedCount() const;
    void finishTask(bool success, const QString& message);

private:
    LogDB::AlarmLogDBCon* m_db = nullptr;
    int m_batchSize = 100;
    int m_batchIntervalMs = 1000;
    int m_totalCount = 0;
    int m_resolvedCount = 0;
    int m_remainingCount = 0;
    int m_emptyBatchRetryCount = 0;
    bool m_cancelRequested = false;
    bool m_finished = false;
};

#endif // ALARM_RESET_TASK_H
