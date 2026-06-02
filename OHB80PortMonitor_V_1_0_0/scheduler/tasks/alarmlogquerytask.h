#ifndef ALARMLOGQUERYTASK_H
#define ALARMLOGQUERYTASK_H

#include "scheduler/scheduler_task.h"
#include "alarmlogdbcon.h"
#include "alarmrecord.h"


class AlarmLogQueryTask : public SchedulerTask
{
    Q_OBJECT
public:
    explicit AlarmLogQueryTask(QObject *parent = nullptr);

    // 任务生命周期
    void start() override;
    void stop() override;
    QString taskType() const override { return "AlarmLogQueryTask"; }

    // 查询条件设置接口（可选设置，未调用的条件不启用）
    void setPageNumber(int pageNumber);
    void setPageSize(int pageSize);
    void setAlarmLevel(int alarmLevel);
    void setQRCode(const QString& qrCode);
    void setAlarmType(const QString& alarmType);
    void setIsResolved(int isResolved);
    void setOccurTimeRange(const QString& startTime, const QString& endTime);

    // 执行查询
    void executeQuery();

signals:
    // 有条件查询：当前页中所有满足条件的记录
    void pageWithConditionsResult(const QList<AlarmRecord>& records);

    // 有条件查询：总记录数
    void totalCountWithConditionsResult(int totalCount);

private:
    LogDB::AlarmLogDBCon* m_db;

    // 查询条件
    int     m_alarmLevel;       // -1 表示未设置
    QString m_qrCode;           // 空字符串表示未设置
    QString m_alarmType;        // 空字符串表示未设置
    int     m_isResolved;       // -1 表示未设置
    QString m_startTime;        // 空字符串表示未设置
    QString m_endTime;          // 空字符串表示未设置
    int     m_pageNumber;       // 1-based，0 表示未设置
    int     m_pageSize;         // 默认500

    const std::string m_taskLogPath = "scheduler/alarmlogquerytask";
};

#endif // ALARMLOGQUERYTASK_H
