/*******************************************************************************************
 * @file operationlogquerytask.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class OperationLogQueryTask
 * @brief 在调度线程中执行运行日志历史分页查询并向 UI 返回分步结果。
 *
 * 设计目标：
 *      1. 将运行日志历史查询从 UI 线程剥离，避免大范围查询阻塞界面。
 *      2. 统一封装时间范围、日志类型、关键词、分页定位和权限过滤条件。
 *      3. 通过分步信号返回页数据、命中记录和统计数量，便于界面独立刷新。
 *******************************************************************************************/
#ifndef OPERATIONLOGQUERYTASK_H
#define OPERATIONLOGQUERYTASK_H

#include <QAtomicInt>
#include <QList>
#include <QString>

#include <string>

#include "dbtypes.h"
#include "operationlogdbcon.h"
#include "operationrecord.h"
#include "scheduler/scheduler_task.h"

class OperationLogQueryTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit OperationLogQueryTask(QObject* parent = nullptr);

    // ============================ 任务生命周期 ============================
    void start() override;
    void stop() override;
    QString taskType() const override { return QStringLiteral("OperationLogQueryTask"); }

    // ============================ 查询参数 ============================
    void setTimeRange(const QString& startTime, const QString& endTime);
    void setLogType(int logType);
    void setSearchKey(const QString& keyword);
    void setPageSize(int pageSize);
    void setMaxUserPermission(int maxUserPermission);
    void setTargetPage(int page);
    void setNextPageAnchorId(int recordId);
    void setPreviousPageAnchorId(int recordId);
    void setCachedTotalCountInRange(int totalCount);
    void setCachedTotalMatchedCount(int totalCount);
    void setAdjacentPagePositionHint(bool pageAfterSource,
                                     int sourceFirstMatchedPosition,
                                     int sourceMatchedCount);
    void setNavigateToAdjacentMatch(int anchorRecordId, bool next);
    void setNavigateToMatchedPosition(int position);

private:
    // ---- 查询执行 ----
    void executeQuery();
    bool isCancelled() const { return m_cancelRequested.loadRelaxed() != 0; }

signals:
    // ---- 查询结果 ----
    void targetPageResult(int page);
    void pendingSelectIdResult(int recordId);
    void currentPageResult(const QList<OperationRecord>& records);
    void matchedIdsOnPageResult(const QList<int>& matchedIds);
    void totalCountInRangeResult(int totalCount);
    void totalMatchedCountResult(int totalCount);
    void firstMatchedPositionResult(int position);

private:
    // ---- 数据库成员 ----
    LogDB::OperationLogDBCon* m_db;

    // ---- 查询条件成员 ----
    QString m_startTime;
    QString m_endTime;
    int m_logType;
    QString m_keyword;
    int m_maxUserPermission;

    // ---- 分页成员 ----
    int m_pageSize;
    int m_targetPage;
    int m_anchorRecordId;
    bool m_queryNextPageFromAnchor;
    bool m_queryPreviousPageFromAnchor;
    bool m_hasCachedTotalCountInRange;
    int m_cachedTotalCountInRange;
    bool m_hasCachedTotalMatchedCount;
    int m_cachedTotalMatchedCount;
    bool m_hasAdjacentPagePositionHint;
    bool m_pageAfterSource;
    int m_sourceFirstMatchedPosition;
    int m_sourceMatchedCount;
    bool m_navigateToAdjacentMatch;
    bool m_navigationNext;
    int m_navigationAnchorRecordId;
    bool m_navigateToMatchedPosition;
    int m_navigationTargetPosition;

    // ---- 任务状态成员 ----
    QAtomicInt m_cancelRequested;
    const std::string m_taskLogPath = "scheduler/operationlogquerytask";
};

#endif // OPERATIONLOGQUERYTASK_H
