/*******************************************************************************************
 * @file communicatelogquerytask.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class CommunicateLogQueryTask
 * @brief 在调度线程中执行通讯日志条件分页查询并向 UI 返回结果。
 *
 * 设计目标：
 *      1. 将通讯日志历史查询从 UI 线程剥离，避免大范围查询阻塞界面。
 *      2. 统一封装指令、设备、执行状态、重试次数、时间范围和权限过滤条件。
 *      3. 通过分步信号返回分页记录和统计数量，便于界面独立刷新。
 *******************************************************************************************/
#ifndef COMMUNICATELOGQUERYTASK_H
#define COMMUNICATELOGQUERYTASK_H

#include <QList>
#include <QString>
#include <string>

#include "communicaterecord.h"
#include "communicatelogdbcon.h"
#include "dbtypes.h"
#include "scheduler/scheduler_task.h"

class CommunicateLogQueryTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit CommunicateLogQueryTask(QObject* parent = nullptr);

    // ============================ 任务生命周期 ============================
    void start() override;
    void stop() override;
    QString taskType() const override { return QStringLiteral("CommunicateLogQueryTask"); }

    // ============================ 查询参数 ============================
    void setPageNumber(int pageNumber);
    void setPageSize(int pageSize);
    void setCommandId(const QString& commandId);
    void setQRCode(const QString& qrCode);
    void setExecStatus(int execStatus);
    void setRetryCount(int retryCount);
    void setSendTimeRange(const QString& startTime, const QString& endTime);
    void setSortOrder(LogDB::SortOrder sortOrder);
    void setMaxUserPermission(int maxUserPermission);

private:
    // ---- 查询执行 ----
    void executeQuery();

signals:
    // ---- 查询结果 ----
    void pageWithConditionsResult(const QList<CommunicateRecord>& records);
    void totalCountWithConditionsResult(int totalCount);

private:
    // ---- 数据库成员 ----
    LogDB::CommunicateLogDBCon* m_db;

    // ---- 查询条件成员 ----
    QString m_commandId;
    QString m_qrCode;
    int m_execStatus;
    int m_retryCount;
    QString m_startTime;
    QString m_endTime;
    int m_pageNumber;
    int m_pageSize;
    LogDB::SortOrder m_sortOrder;
    int m_maxUserPermission;

    // ---- 任务日志成员 ----
    const std::string m_taskLogPath = "scheduler/communicatelogquerytask";
};

#endif // COMMUNICATELOGQUERYTASK_H
