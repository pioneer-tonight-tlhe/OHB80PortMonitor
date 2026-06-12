#ifndef OPERATIONLOGQUERYTASK_H
#define OPERATIONLOGQUERYTASK_H

#include "scheduler/scheduler_task.h"
#include "operationlogdbcon.h"

#include "operationrecord.h"
#include "dbtypes.h"

#include <QAtomicInt>


// 运行日志查询任务（重新设计）。
//
// 概念分层：
//   - 范围 (range)：startTime / endTime —— 决定数据集（影响分页总数）
//   - 匹配条件 (conditions)：logType / keyword —— 决定哪些记录被高亮、可被
//     Pre / Next 跨页跟踪
//
// 调用方按需 setTimeRange / setLogType / setSearchKey / setPageSize /
// setTargetPage 后通过 Scheduler::submitTask 提交。
//
// 任务在工作线程上执行以下子查询，每完成一步发出对应信号：
//   - targetPageResult              —— 本次使用的页号
//   - currentPageResult             —— 范围内该页的全部记录
//   - matchedIdsOnPageResult        —— 该页中满足匹配条件的记录 id 集合
//                                      （保留 SQL 返回顺序，用于 Pre/Next 页内跳转）
//   - totalCountInRangeResult       —— 范围内总记录数（用于分页）
//   - totalMatchedCountResult       —— 范围 + 条件总命中数（用于 X/Y 显示）
//   - firstMatchedPositionResult    —— 该页首条命中在条件结果集中的全局顺序号
class OperationLogQueryTask : public SchedulerTask
{
    Q_OBJECT
public:
    explicit OperationLogQueryTask(QObject *parent = nullptr);

    // 任务生命周期
    void start() override;
    void stop() override;
    QString taskType() const override { return "OperationLogQueryTask"; }

    // 时间范围（不是查询条件，而是数据范围）
    void setTimeRange(const QString& startTime, const QString& endTime);
    // 匹配条件
    void setLogType(int logType);
    void setSearchKey(const QString& keyword);
    void setPageSize(int pageSize);
    // 显式指定目标页号；> 0 时跳过自动定位
    void setTargetPage(int page);

signals:
    void targetPageResult(int page);
    void currentPageResult(const QList<OperationRecord>& records);
    void matchedIdsOnPageResult(const QList<int>& matchedIds);
    void totalCountInRangeResult(int totalCount);
    void totalMatchedCountResult(int totalCount);
    void firstMatchedPositionResult(int position);

private:
    void executeQuery();
    bool isCancelled() const { return m_cancelRequested.loadRelaxed() != 0; }

    LogDB::OperationLogDBCon* m_db;

    // 范围
    QString m_startTime;   // 空字符串表示未限定
    QString m_endTime;     // 空字符串表示未限定
    // 匹配条件
    int     m_logType;     // -1 表示未启用
    QString m_keyword;     // 空字符串表示未启用
    // 分页
    int     m_pageSize;    // 默认 500
    int     m_targetPage;  // > 0 时直接使用该页号

    // 取消标志（线程安全；stop() 设置，executeQuery 在各子查询之间检查）
    QAtomicInt m_cancelRequested;

    const std::string m_taskLogPath = "scheduler/operationlogquerytask";
};

#endif // OPERATIONLOGQUERYTASK_H
