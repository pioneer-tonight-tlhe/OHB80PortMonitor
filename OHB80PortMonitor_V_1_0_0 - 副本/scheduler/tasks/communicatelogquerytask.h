#ifndef COMMUNICATELOGQUERYTASK_H
#define COMMUNICATELOGQUERYTASK_H

#include "scheduler/scheduler_task.h"
#include "communicatelogdbcon.h"
#include "communicaterecord.h"
#include "dbtypes.h"


class CommunicateLogQueryTask : public SchedulerTask
{
    Q_OBJECT
public:
    explicit CommunicateLogQueryTask(QObject *parent = nullptr);

    // 任务生命周期
    void start() override;
    void stop() override;
    QString taskType() const override { return "CommunicateLogQueryTask"; }

    // 查询条件设置接口（可选设置，未调用的条件不启用）
    void setPageNumber(int pageNumber);
    void setPageSize(int pageSize);
    void setCommandId(const QString& commandId);
    void setQRCode(const QString& qrCode);
    void setExecStatus(int execStatus);
    void setRetryCount(int retryCount);
    void setSendTimeRange(const QString& startTime, const QString& endTime);
    void setSortOrder(LogDB::SortOrder sortOrder);

    // 执行查询
    void executeQuery();

signals:
    // 有条件查询：当前页中所有满足条件的记录
    void pageWithConditionsResult(const QList<CommunicateRecord>& records);

    // 有条件查询：总记录数
    void totalCountWithConditionsResult(int totalCount);

private:
    LogDB::CommunicateLogDBCon* m_db;

    // 查询条件
    QString m_commandId;                // 空字符串表示未设置
    QString m_qrCode;                   // 空字符串表示未设置
    int m_execStatus;                   // -1 表示未设置
    int m_retryCount;                   // -1 表示未设置
    QString m_startTime;                // 空字符串表示未设置
    QString m_endTime;                  // 空字符串表示未设置
    int m_pageNumber;                   // 1-based，0 表示未设置
    int m_pageSize;                     // 默认500
    LogDB::SortOrder m_sortOrder;       // 按 send_time 的排序方向，默认降序

    const std::string m_taskLogPath = "scheduler/communicatelogquerytask";
};

#endif // COMMUNICATELOGQUERYTASK_H
