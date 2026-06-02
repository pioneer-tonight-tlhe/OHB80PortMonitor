#include "operationlogquerytask.h"
#include "databasemanager.h"
#include "loggermanager.h"
#include "defer/defer.h"
#include <QDebug>

OperationLogQueryTask::OperationLogQueryTask(QObject *parent)
    : SchedulerTask{parent}
    , m_db(nullptr)
    , m_logType(-1)
    , m_pageSize(500)
    , m_targetPage(0)
    , m_cancelRequested(0)
{
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO, "[OperationLogQueryTask] 运行日志查询任务已构造");
    LoggerManager::getInstance()->flush(m_taskLogPath);
}

void OperationLogQueryTask::setTimeRange(const QString& startTime, const QString& endTime)
{
    m_startTime = startTime;
    m_endTime   = endTime;
}

void OperationLogQueryTask::setLogType(int logType)
{
    m_logType = logType;
}

void OperationLogQueryTask::setSearchKey(const QString& keyword)
{
    m_keyword = keyword;
}

void OperationLogQueryTask::setPageSize(int pageSize)
{
    m_pageSize = pageSize;
}

void OperationLogQueryTask::setTargetPage(int page)
{
    m_targetPage = page;
}

void OperationLogQueryTask::start()
{
    // 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([this]() {
        LoggerManager::getInstance()->flush(m_taskLogPath);
    });

    m_db = LogDB::DatabaseManager::instance().operationLogCon();
    if (!m_db) {
        setState(Failed);
        LoggerManager::getInstance()->log(m_taskLogPath, Level::ERROR, "[start] 运行日志数据库不可用");
        emit finished(false, "OperationLogDBCon unavailable");
        return;
    }

    setState(Running);
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO, "[start] 运行日志查询任务已启动");
    executeQuery();
}

void OperationLogQueryTask::stop()
{
    // 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([this]() {
        LoggerManager::getInstance()->flush(m_taskLogPath);
    });

    // 仅设置取消标志，executeQuery 在各子查询之间检查
    m_cancelRequested.storeRelaxed(1);
    setState(Cancelled);
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO, "[stop] 运行日志查询任务已停止");
}

void OperationLogQueryTask::executeQuery()
{
    // 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([this]() {
        LoggerManager::getInstance()->flush(m_taskLogPath);
    });


    QString dbEarliest;
    QString dbLatest;
    m_db->queryTimeBounds(dbEarliest, dbLatest);
    if (!dbEarliest.isEmpty() && !dbLatest.isEmpty()) {
        const QString effStart = m_startTime.isEmpty() ? dbEarliest : m_startTime;
        const QString effEnd   = m_endTime.isEmpty()   ? dbLatest   : m_endTime;
        const bool noOverlap = (effStart > dbLatest) || (effEnd < dbEarliest);

        if (!noOverlap) {
            if (m_startTime.isEmpty() || m_startTime < dbEarliest) {
                m_startTime = dbEarliest;
            }
            if (m_endTime.isEmpty() || m_endTime > dbLatest) {
                m_endTime = dbLatest;
            }
            if (m_startTime > m_endTime) {
                qSwap(m_startTime, m_endTime);
            }
            LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                QString("[executeQuery] 钳制后时间范围: %1 -> %2 (数据库: %3 -> %4)")
                    .arg(m_startTime, m_endTime, dbEarliest, dbLatest).toStdString());
        } else {
            LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                QString("[executeQuery] 请求窗口与数据库范围无重叠，不钳制: %1 -> %2 (数据库: %3 -> %4)")
                    .arg(m_startTime, m_endTime, dbEarliest, dbLatest).toStdString());
        }
    }

    // 记录查询条件
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[executeQuery] 查询条件: logType(日志类型)=%1, keyword(关键字)=%2, timeRange(时间范围)=%3 -> %4, pageSize(每页大小)=%5, targetPage(目标页)=%6")
            .arg(m_logType == -1 ? "全部" : QString::number(m_logType))
            .arg(m_keyword.isEmpty() ? "不限" : m_keyword)
            .arg(m_startTime.isEmpty() ? "不限" : m_startTime)
            .arg(m_endTime.isEmpty() ? "不限" : m_endTime)
            .arg(m_pageSize)
            .arg(m_targetPage > 0 ? QString::number(m_targetPage) : QString("自动定位"))
            .toStdString());

    const bool hasMatchConditions = (m_logType != -1 || !m_keyword.isEmpty());

    // 1. 确定本次查询使用的页号：
    int targetPage = m_targetPage;
    if (targetPage <= 0) {
        if (hasMatchConditions) {
            // 两步法：先找范围+条件下首条命中 id，再算它在范围内的真实页号
            const int firstId = m_db->queryFirstMatchedId(
                m_startTime, m_endTime, m_logType, m_keyword);
            if (firstId > 0) {
                targetPage = m_db->queryRecordPageInRange(
                    firstId, m_startTime, m_endTime, m_pageSize);
            }
        }
        if (targetPage <= 0) {
            targetPage = 1;
        }
    }
    emit targetPageResult(targetPage);
    if (isCancelled()) { emit finished(false, "Cancelled"); return; }

    // 2. 范围内分页：该页全部记录（用于显示）
    QList<OperationRecord> currentPageRecords =
        m_db->queryPaginationInRange(m_startTime, m_endTime, m_pageSize, targetPage);
    emit currentPageResult(currentPageRecords);
    if (isCancelled()) { emit finished(false, "Cancelled"); return; }

    // 3. 该页中满足匹配条件的记录 id 集合（用于高亮 / Pre/Next 页内跳转）
    QList<int> matchedIds;
    if (hasMatchConditions) {
        // queryPageWithConditions 在子查询里 LIMIT/OFFSET 全表，对"范围分页"
        // 不能直接套用——这里改为对当前页记录在内存里按条件过滤。
        const int logType = m_logType;
        const QString kw = m_keyword.toLower();
        for (const OperationRecord& rec : currentPageRecords) {
            const bool typeOK = (logType == -1) || (rec.logType == logType);
            const bool kwOK   = kw.isEmpty() || rec.description.toLower().contains(kw);
            if (typeOK && kwOK) {
                matchedIds.append(rec.id);
            }
        }
    }
    emit matchedIdsOnPageResult(matchedIds);
    if (isCancelled()) { emit finished(false, "Cancelled"); return; }

    // 4. 范围内总记录数（用于分页总页数）
    int totalCountInRange = m_db->queryTotalCountInRange(m_startTime, m_endTime);
    emit totalCountInRangeResult(totalCountInRange);
    if (isCancelled()) { emit finished(false, "Cancelled"); return; }

    // 5. 范围 + 条件总命中数（用于 X/Y 显示）
    int totalMatched = 0;
    if (hasMatchConditions) {
        totalMatched = m_db->queryTotalCountWithConditions(
            m_startTime, m_endTime, m_logType, m_keyword);
    }
    emit totalMatchedCountResult(totalMatched);
    if (isCancelled()) { emit finished(false, "Cancelled"); return; }

    // 6. 该页首条命中记录在范围+条件结果集中的全局顺序号（1-based；0 表示无命中）
    int position = 0;
    if (hasMatchConditions && !matchedIds.isEmpty()) {
        const int firstId = matchedIds.first();
        if (firstId > 0) {
            position = m_db->queryRecordPosition(
                firstId, m_startTime, m_endTime, m_logType, m_keyword);
        }
    }
    emit firstMatchedPositionResult(position);
    if (isCancelled()) { emit finished(false, "Cancelled"); return; }

    setState(Finished);

    // 记录查询结果
    if (totalMatched == 0 && hasMatchConditions) {
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            QString("[executeQuery] 查询成功但无匹配结果: 总记录数=%1, 匹配数=%2, 当前页=%3, 每页大小=%4")
                .arg(totalCountInRange)
                .arg(totalMatched)
                .arg(targetPage)
                .arg(m_pageSize)
                .toStdString());
    } else if (totalCountInRange == 0) {
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            QString("[executeQuery] 查询成功但范围内无数据: 总记录数=%1, 当前页=%2, 每页大小=%3")
                .arg(totalCountInRange)
                .arg(targetPage)
                .arg(m_pageSize)
                .toStdString());
    } else {
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[executeQuery] 查询成功: 总记录数=%1, 匹配数=%2, 当前页=%3, 每页大小=%4")
                .arg(totalCountInRange)
                .arg(totalMatched)
                .arg(targetPage)
                .arg(m_pageSize)
                .toStdString());
    }

    emit finished(true, "Query completed successfully");
}
