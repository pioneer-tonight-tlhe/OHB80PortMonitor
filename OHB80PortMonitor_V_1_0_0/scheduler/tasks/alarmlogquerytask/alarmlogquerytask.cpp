#include "alarmlogquerytask.h"
#include "databasemanager.h"
#include "loggermanager.h"
#include "defer/defer.h"

AlarmLogQueryTask::AlarmLogQueryTask(QObject *parent)
    : SchedulerTask{parent}
    , m_db(nullptr)
    , m_alarmLevel(-1)
    , m_isResolved(-1)
    , m_pageNumber(0)
    , m_pageSize(500)
{
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO, "[AlarmLogQueryTask] 警报日志查询任务已构造");
    LoggerManager::getInstance()->flush(m_taskLogPath);
}

void AlarmLogQueryTask::start()
{
    // 获取警报日志数据库连  
    m_db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!m_db) {
        setState(Failed);
        LoggerManager::getInstance()->log(m_taskLogPath, Level::ERROR, "[start] 警报日志数据库不可用");
        emit finished(false, "AlarmLogDBCon unavailable");
        return;
    }

    setState(Running);
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO, "[start] 警报日志查询任务已启");
    executeQuery();
}

void AlarmLogQueryTask::stop()
{
    // 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([this]() {
        LoggerManager::getInstance()->flush(m_taskLogPath);
    });

    setState(Finished);
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO, "[stop] 警报日志查询任务已停");
}

void AlarmLogQueryTask::setPageNumber(int pageNumber)
{
    m_pageNumber = pageNumber;
}

void AlarmLogQueryTask::setPageSize(int pageSize)
{
    m_pageSize = pageSize;
}

void AlarmLogQueryTask::setAlarmLevel(int alarmLevel)
{
    m_alarmLevel = alarmLevel;
}

void AlarmLogQueryTask::setQRCode(const QString& qrCode)
{
    m_qrCode = qrCode;
}

void AlarmLogQueryTask::setAlarmType(const QString& alarmType)
{
    m_alarmType = alarmType;
}

void AlarmLogQueryTask::setIsResolved(int isResolved)
{
    m_isResolved = isResolved;
}

void AlarmLogQueryTask::setOccurTimeRange(const QString& startTime, const QString& endTime)
{
    m_startTime = startTime;
    m_endTime = endTime;
}

void AlarmLogQueryTask::executeQuery()
{
    // 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([this]() {
        LoggerManager::getInstance()->flush(m_taskLogPath);
    });

    if (!m_db) {
        setState(Failed);
        LoggerManager::getInstance()->log(m_taskLogPath, Level::ERROR, "[executeQuery] 数据库连接不可用");
        emit finished(false, "Database connection not available");
        return;
    }

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

        } else {

        }
    }

    // 记录查询条件
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[executeQuery] 查询条件: alarmLevel(告警级别)=%1, qrCode(设备标识)=%2, alarmType(告警类型)=%3, isResolved(是否解决)=%4, occurTime(时间范围)=%5 -> %6")
            .arg(m_alarmLevel == -1 ? "全部" : QString::number(m_alarmLevel))
            .arg(m_qrCode.isEmpty() ? "全部" : m_qrCode)
            .arg(m_alarmType.isEmpty() ? "全部" : m_alarmType)
            .arg(m_isResolved == -1 ? "全部" : (m_isResolved == 0 ? "未解" : "已解"))
            .arg(m_startTime.isEmpty() ? "不限" : m_startTime)
            .arg(m_endTime.isEmpty() ? "不限" : m_endTime)
            .toStdString());

    // 有条件查询：当前页中所有满足条件的记录
    int pageRecordCount = 0;
    if (m_pageNumber > 0) {
        QList<AlarmRecord> pageRecords = m_db->queryPageWithConditions(
            m_alarmLevel, m_qrCode, m_alarmType, m_isResolved,
            m_startTime, m_endTime,
            m_pageSize, m_pageNumber);
        pageRecordCount = pageRecords.size();
        emit pageWithConditionsResult(pageRecords);
    }

    // 有条件查询：总记录数
    int totalCountWithConditions = m_db->queryTotalCountWithConditions(
        m_alarmLevel, m_qrCode, m_alarmType, m_isResolved,
        m_startTime, m_endTime);
    emit totalCountWithConditionsResult(totalCountWithConditions);

    setState(Finished);

    // 记录查询结果
    if (totalCountWithConditions == 0) {
        LoggerManager::getInstance()->log(m_taskLogPath, Level::WARN,
            QString("[executeQuery] 查询成功但无结果: 总记录数=%1, 当前页记录数=%2, 页码=%3, 每页大小=%4")
                .arg(totalCountWithConditions)
                .arg(pageRecordCount)
                .arg(m_pageNumber)
                .arg(m_pageSize)
                .toStdString());
    } else {
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[executeQuery] 查询成功: 总记录数=%1, 当前页记录数=%2, 页码=%3, 每页大小=%4")
                .arg(totalCountWithConditions)
                .arg(pageRecordCount)
                .arg(m_pageNumber)
                .arg(m_pageSize)
                .toStdString());
    }

    emit finished(true, "Query completed successfully");
}
