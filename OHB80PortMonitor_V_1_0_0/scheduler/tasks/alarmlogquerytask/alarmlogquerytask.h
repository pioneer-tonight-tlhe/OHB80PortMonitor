/*******************************************************************************************
 * @file alarmlogquerytask.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class AlarmLogQueryTask
 * @brief 在调度线程中执行警报日志条件分页查询并向 UI 返回结果。
 *
 * 设计目标：
 *      1. 将警报日志历史查询从 UI 线程剥离，避免大范围查询阻塞界面。
 *      2. 统一封装警报级别、设备、状态、时间范围和权限过滤条件。
 *      3. 通过分步信号返回分页记录和统计数量，便于界面独立刷新。
 *******************************************************************************************/
#ifndef ALARMLOGQUERYTASK_H
#define ALARMLOGQUERYTASK_H

#include <QList>
#include <QString>

#include <string>

#include "alarmlogdbcon.h"
#include "alarmrecord.h"
#include "scheduler/scheduler_task.h"

class AlarmLogQueryTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit AlarmLogQueryTask(QObject* parent = nullptr);

    // ============================ 任务生命周期 ============================
    void start() override;
    void stop() override;
    QString taskType() const override { return QStringLiteral("AlarmLogQueryTask"); }

    // ============================ 查询参数 ============================
    void setPageNumber(int pageNumber);
    void setPageSize(int pageSize);
    void setAlarmLevel(int alarmLevel);
    void setQRCode(const QString& qrCode);
    void setAlarmType(const QString& alarmType);
    void setIsResolved(int isResolved);
    void setOccurTimeRange(const QString& startTime, const QString& endTime);
    void setResolveTimeRange(const QString& startTime, const QString& endTime);
    void setMaxUserPermission(int maxUserPermission);

private:
    // ---- 查询执行 ----
    void executeQuery();

signals:
    // ---- 查询结果 ----
    void pageWithConditionsResult(const QList<AlarmRecord>& records);
    void totalCountWithConditionsResult(int totalCount);

private:
    // ---- 数据库成员 ----
    LogDB::AlarmLogDBCon* m_db;

    // ---- 查询条件成员 ----
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

    // ---- 任务日志成员 ----
    const std::string m_taskLogPath = "scheduler/alarmlogquerytask";
};

#endif // ALARMLOGQUERYTASK_H
