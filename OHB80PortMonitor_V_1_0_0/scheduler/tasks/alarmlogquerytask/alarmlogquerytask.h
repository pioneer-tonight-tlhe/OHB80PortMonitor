#ifndef ALARMLOGQUERYTASK_H
#define ALARMLOGQUERYTASK_H

#include "alarmrecord.h"
#include "alarmlogdbcon.h"
#include "scheduler/scheduler_task.h"

#include <QString>

#include <string>

class AlarmLogQueryTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit AlarmLogQueryTask(QObject* parent = nullptr);

    void start() override;
    void stop() override;
    QString taskType() const override { return "AlarmLogQueryTask"; }

    void setPageNumber(int pageNumber);
    void setPageSize(int pageSize);
    void setAlarmLevel(int alarmLevel);
    void setQRCode(const QString& qrCode);
    void setAlarmType(const QString& alarmType);
    void setIsResolved(int isResolved);
    void setOccurTimeRange(const QString& startTime, const QString& endTime);
    void setResolveTimeRange(const QString& startTime, const QString& endTime);
    void setMaxUserPermission(int maxUserPermission);

    void executeQuery();

signals:
    void pageWithConditionsResult(const QList<AlarmRecord>& records);
    void totalCountWithConditionsResult(int totalCount);

private:
    LogDB::AlarmLogDBCon* m_db;

    int m_alarmLevel;
    QString m_qrCode;
    QString m_alarmType;
    int m_isResolved;
    QString m_startTime;
    QString m_endTime;
    QString m_resolveStartTime;
    QString m_resolveEndTime;
    int m_maxUserPermission;
    int m_pageNumber;
    int m_pageSize;

    const std::string m_taskLogPath = "scheduler/alarmlogquerytask";
};

#endif // ALARMLOGQUERYTASK_H
