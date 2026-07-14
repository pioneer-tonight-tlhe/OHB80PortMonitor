#include "alarm_reset_task.h"

#include "app/shareddata.h"
#include "logdatabases/alarmlogdb/alarmlogdbcon.h"
#include "logdatabases/databasemanager.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"

#include <QDateTime>
#include <QTimer>
#include <QtGlobal>

AlarmResetTask::AlarmResetTask(QObject* parent)
    : SchedulerTask(parent)
{
}

void AlarmResetTask::start()
{
    if (m_finished) {
        return;
    }

    m_db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!m_db) {
        finishTask(false, QStringLiteral("Alarm log database unavailable"));
        return;
    }

    m_cancelRequested = false;
    m_resolvedCount = 0;
    m_emptyBatchRetryCount = 0;
    m_totalCount = queryUnresolvedCount();
    m_remainingCount = m_totalCount;

    setState(Running);
    emit resetStarted(m_totalCount);
    emit progress(0, QStringLiteral("Unresolved alarms: %1").arg(m_totalCount));

    if (m_totalCount <= 0) {
        finishTask(true, QStringLiteral("No unresolved alarm records"));
        return;
    }

    QTimer::singleShot(0, this, &AlarmResetTask::processNextBatch);
}

void AlarmResetTask::stop()
{
    m_cancelRequested = true;
    if (!m_finished) {
        finishTask(false, QStringLiteral("Alarm reset cancelled"));
    }
}

void AlarmResetTask::setBatchSize(int batchSize)
{
    m_batchSize = qBound(1, batchSize, 1000);
}

void AlarmResetTask::setBatchIntervalMs(int intervalMs)
{
    m_batchIntervalMs = qMax(0, intervalMs);
}

void AlarmResetTask::processNextBatch()
{
    if (m_finished) {
        return;
    }

    if (m_cancelRequested) {
        finishTask(false, QStringLiteral("Alarm reset cancelled"));
        return;
    }

    if (!m_db) {
        finishTask(false, QStringLiteral("Alarm log database unavailable"));
        return;
    }

    const QString resolveTime = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QList<AlarmRecord> records = m_db->resolveUnresolvedBatch(m_batchSize, resolveTime);

    if (records.isEmpty()) {
        m_remainingCount = queryUnresolvedCount();
        if (m_remainingCount <= 0) {
            finishTask(true, QStringLiteral("Alarm reset completed"));
            return;
        }

        ++m_emptyBatchRetryCount;
        if (m_emptyBatchRetryCount > 3) {
            finishTask(false, QStringLiteral("No records updated after retries"));
            return;
        }

        emit progress(qBound(0, m_totalCount > 0 ? (m_resolvedCount * 100 / m_totalCount) : 0, 99),
                      QStringLiteral("Waiting for database, remaining: %1").arg(m_remainingCount));
        scheduleNextBatch();
        return;
    }

    m_emptyBatchRetryCount = 0;
    m_resolvedCount += records.size();

    if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
        dispatcher->resetActiveAlarmsByRecords(records);
    }

    m_remainingCount = queryUnresolvedCount();
    m_totalCount = qMax(m_totalCount, m_resolvedCount + m_remainingCount);

    const int percent = (m_totalCount > 0)
        ? qBound(0, (m_resolvedCount * 100 / m_totalCount), 100)
        : 100;
    emit batchResolved(m_totalCount, m_resolvedCount, m_remainingCount, records.size());
    emit progress(percent,
                  QStringLiteral("Resolved %1/%2, remaining %3")
                      .arg(m_resolvedCount)
                      .arg(m_totalCount)
                      .arg(m_remainingCount));

    if (m_remainingCount <= 0) {
        finishTask(true, QStringLiteral("Alarm reset completed"));
        return;
    }

    scheduleNextBatch();
}

void AlarmResetTask::scheduleNextBatch()
{
    if (m_finished) {
        return;
    }

    QTimer::singleShot(m_batchIntervalMs, this, &AlarmResetTask::processNextBatch);
}

int AlarmResetTask::queryUnresolvedCount() const
{
    if (!m_db) {
        return 0;
    }

    return m_db->queryTotalCountWithConditions(/*alarmLevel*/ -1,
                                               /*qrCode*/ QString(),
                                               /*alarmType*/ QString(),
                                               /*isResolved*/ 0,
                                               /*startTime*/ QString(),
                                               /*endTime*/ QString(),
                                               /*resolveStartTime*/ QString(),
                                               /*resolveEndTime*/ QString(),
                                               /*maxUserPermission*/ -1);
}

void AlarmResetTask::finishTask(bool success, const QString& message)
{
    if (m_finished) {
        return;
    }

    m_finished = true;
    m_remainingCount = queryUnresolvedCount();
    setState(success ? Finished : (m_cancelRequested ? Cancelled : Failed));
    emit resetFinished(success, message, m_totalCount, m_resolvedCount, m_remainingCount);
    emit finished(success, message);
}
