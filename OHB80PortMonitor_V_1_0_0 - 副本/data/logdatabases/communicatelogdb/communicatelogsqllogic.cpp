#include "communicatelogsqllogic.h"
#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"
#include "qthelper.h"
#include "loggermanager.h"
#include "defer/defer.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QTimer>
#include <QDateTime>
#include <QDate>
#include <QStringList>
#include <QDebug>

namespace LogDB {

CommunicateLogSqlLogic::CommunicateLogSqlLogic(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_connectionName("CommunicateLogSqlLogicConnection")
    , m_sqlMapper(nullptr)
    , m_cleanupScheduler(nullptr)
    , m_diskCheckTimer(nullptr)
{
    QString sqlFilePath = QString("%1/communicate_log_queries.sql").arg(databasePath);
    m_sqlMapper = new SqlMapper(sqlFilePath);
}

CommunicateLogSqlLogic::~CommunicateLogSqlLogic()
{
    if (m_diskCheckTimer) {
        m_diskCheckTimer->stop();
    }
    if (m_cleanupScheduler) {
        m_cleanupScheduler->stop();
        delete m_cleanupScheduler;
        m_cleanupScheduler = nullptr;
    }
    if (m_database.isOpen()) {
        m_database.close();
    }
    if (m_sqlMapper) {
        delete m_sqlMapper;
    }
}

bool CommunicateLogSqlLogic::initializeDatabase()
{
    QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);
    m_database = DBConnectionHelper::openSqlite(dbFilePath, m_connectionName);
    if (!m_database.isOpen()) {
        return false;
    }

    initializeCleanupScheduler();
    initializeDiskCheckTimer();
    return true;
}

void CommunicateLogSqlLogic::initializeCleanupScheduler()
{
    LogCleanupScheduler::Config cfg;
    cfg.checkIntervalMs = 60000;
    cfg.retainMonths = 7;
    cfg.cleanupMonths = 1;
    cfg.logPath = "log_db/communicate_log_db/month_clean";
    m_cleanupScheduler = new LogCleanupScheduler(cfg, this);
    m_cleanupScheduler->setMonthRangeProvider([this]() {
        return queryMonthRange();
    });
    m_cleanupScheduler->setDeleteByRangeFn([this](const QString& s, const QString& e) {
        deleteByTimeRange(s, e);
    });
    m_cleanupScheduler->start();
}

bool CommunicateLogSqlLogic::insertRecord(const QString& sendTime,
                                          const QString& responseTime,
                                          const QString& commandId,
                                          const QString& qrCode,
                                          int execStatus,
                                          int retryCount,
                                          const QByteArray& sendFrame,
                                          const QByteArray& responseFrame,
                                          const QString& description,
                                          int userPermission)
{
    QString sql = m_sqlMapper->getSql("insert_record");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: insert_record";
        return false;
    }

    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql;
    result.sqlId = "insert_record";
    result.result = "";
    result.tableName = "communicate_log";
    result.opType = static_cast<int>(WriteOp::Insert);

    // 顺序与 SQL ((send_time, response_time, command_id, qr_code, exec_status,
    //                retry_count, send_frame, response_frame, description, user_permission)) 严格对齐
    result.params << sendTime
                  << (responseTime.isEmpty() ? QVariant() : QVariant(responseTime))
                  << commandId
                  << qrCode
                  << execStatus
                  << retryCount
                  << sendFrame
                  << (responseFrame.isEmpty() ? QVariant() : QVariant(responseFrame))
                  << description
                  << userPermission;

    emit writeExecuted(result);
    return true;
}

QList<QVariantMap> CommunicateLogSqlLogic::queryPageWithConditions(const QString& commandId,
                                                                   const QString& qrCode,
                                                                   int execStatus,
                                                                   int retryCount,
                                                                   const QString& startTime,
                                                                   const QString& endTime,
                                                                   int pageSize,
                                                                   int pageNumber,
                                                                   int sortOrder)
{
    QList<QVariantMap> results;

    if (!m_database.isOpen()) {
        return results;
    }

    QString sql = m_sqlMapper->getSql("query_page_with_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_page_with_conditions";
        return results;
    }

    // 将 {ORDER} 占位符替换为 ASC 或 DESC（白名单，避免 SQL 注入）
    const QString orderKeyword =
        (static_cast<SortOrder>(sortOrder) == SortOrder::Asc) ? QStringLiteral("ASC")
                                                              : QStringLiteral("DESC");
    sql.replace(QStringLiteral("{ORDER}"), orderKeyword);

    int offset = calculateOffset(pageSize, pageNumber);

    QSqlQuery query(m_database);
    query.prepare(sql);
    // command_id（NULL检查 + 值）
    query.addBindValue(commandId.isEmpty() ? QVariant() : commandId);
    query.addBindValue(commandId.isEmpty() ? QVariant() : commandId);
    // qr_code
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    // exec_status（-1表示不应用）
    query.addBindValue(execStatus == -1 ? QVariant() : execStatus);
    query.addBindValue(execStatus == -1 ? QVariant() : execStatus);
    // retry_count（-1表示不应用）
    query.addBindValue(retryCount == -1 ? QVariant() : retryCount);
    query.addBindValue(retryCount == -1 ? QVariant() : retryCount);
    // 时间区间（NULL检查以 startTime 为准；BETWEEN 上下界分别绑 start / end）
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(endTime.isEmpty()   ? QVariant() : endTime);
    // 分页参数（在 ORDER BY 之后的 LIMIT / OFFSET）
    query.addBindValue(pageSize);
    query.addBindValue(offset);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return results;
    }

    while (query.next()) {
        QSqlRecord record = query.record();
        QVariantMap row;
        for (int i = 0; i < record.count(); ++i) {
            row[record.fieldName(i)] = record.value(i);
        }
        results.append(row);
    }

    return results;
}

int CommunicateLogSqlLogic::queryTotalCount()
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_total_count");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count";
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }

    int count = 0;
    if (query.next()) {
        count = query.value(0).toInt();
    }
    return count;
}

int CommunicateLogSqlLogic::queryTotalCountWithConditions(const QString& commandId,
                                                          const QString& qrCode,
                                                          int execStatus,
                                                          int retryCount,
                                                          const QString& startTime,
                                                          const QString& endTime)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_total_count_with_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count_with_conditions";
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(commandId.isEmpty() ? QVariant() : commandId);
    query.addBindValue(commandId.isEmpty() ? QVariant() : commandId);
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    query.addBindValue(execStatus == -1 ? QVariant() : execStatus);
    query.addBindValue(execStatus == -1 ? QVariant() : execStatus);
    query.addBindValue(retryCount == -1 ? QVariant() : retryCount);
    query.addBindValue(retryCount == -1 ? QVariant() : retryCount);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(endTime.isEmpty()   ? QVariant() : endTime);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }

    int count = 0;
    if (query.next()) {
        count = query.value(0).toInt();
    }
    return count;
}

QVariantMap CommunicateLogSqlLogic::queryMonthRange()
{
    QVariantMap result;

    if (!m_database.isOpen()) {
        qWarning() << "Database is not open";
        return result;
    }

    QString sql = m_sqlMapper->getSql("query_month_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_month_range";
        return result;
    }

    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        qWarning() << "Query failed:" << query.lastError().text();
        return result;
    }

    if (query.next()) {
        result["earliest_time"] = query.value("earliest_time");
        result["latest_time"] = query.value("latest_time");
        result["earliest_date"] = query.value("earliest_date");
    }

    return result;
}

bool CommunicateLogSqlLogic::deleteByTimeRange(const QString& startTime, const QString& endTime)
{
    QString sql = m_sqlMapper->getSql("delete_by_time_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: delete_by_time_range";
        return false;
    }

    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql;
    result.sqlId = "delete_by_time_range";
    result.result = "";
    result.tableName = "communicate_log";
    result.opType = static_cast<int>(WriteOp::Delete);

    result.params << startTime << endTime;

    emit writeExecuted(result);
    return true;
}

void CommunicateLogSqlLogic::initializeDiskCheckTimer()
{
    m_diskCheckTimer = new QTimer(this);
    connect(m_diskCheckTimer, &QTimer::timeout,
            this, &CommunicateLogSqlLogic::checkDiskSpaceAndCleanup);
    m_diskCheckTimer->start(DISK_CHECK_INTERVAL_MS);
    LoggerManager::getInstance()->log("log_db/communicate_log_db/clean", Level::INFO,
        QString("磁盘空间检查定时器已启动，间隔: %1ms, 阈值: %2%")
            .arg(DISK_CHECK_INTERVAL_MS)
            .arg(DISK_USAGE_THRESHOLD * 100)
            .toStdString());
    LoggerManager::getInstance()->flush("log_db/communicate_log_db/clean");
}

void CommunicateLogSqlLogic::checkDiskSpaceAndCleanup()
{
    const std::string logPath = "log_db/communicate_log_db/clean";
// 使用 Defer 确保函数退出时刷新日志
    Tool::Defer defer([logPath]() {
        LoggerManager::getInstance()->flush(logPath);
    });

    
    qint64 totalBytes = QtHelper::diskTotalBytes(m_databasePath);
    qint64 usedBytes  = QtHelper::diskUsedBytes(m_databasePath);

    if (totalBytes <= 0 || usedBytes < 0) {
        LoggerManager::getInstance()->log(logPath, Level::WARN,
            QString("无法获取磁盘空间信息，路径: %1").arg(m_databasePath).toStdString());
        return;
    }

    double usageRatio = static_cast<double>(usedBytes) / totalBytes;
    double totalGB = totalBytes / 1024.0 / 1024.0 / 1024.0;
    double usedGB  = usedBytes  / 1024.0 / 1024.0 / 1024.0;
    bool needCleanup = usageRatio >= DISK_USAGE_THRESHOLD;

    // 每次检测都打印磁盘使用情况
    LoggerManager::getInstance()->log(logPath, Level::INFO,
        QString("磁盘空间检测: 使用率 %1% (已用 %2 GB / %3 GB), 阈值 %4%, 需要清理: %5")
            .arg(QString::number(usageRatio * 100, 'f', 1))
            .arg(QString::number(usedGB, 'f', 2))
            .arg(QString::number(totalGB, 'f', 2))
            .arg(DISK_USAGE_THRESHOLD * 100)
            .arg(needCleanup ? "是" : "否")
            .toStdString());

    if (!needCleanup) {
        return;
    }

    LoggerManager::getInstance()->log(logPath, Level::WARN,
        QString("磁盘使用率超过阈值，触发日志清理").toStdString());

    QVariantMap monthRange = queryMonthRange();
    QString earliestDate = monthRange.value("earliest_date").toString();
    QString latestDate   = monthRange.value("latest_time").toString();
    if (earliestDate.isEmpty()) {
        LoggerManager::getInstance()->log(logPath, Level::WARN,
            "无法获取日志月份范围，跳过清理");
        return;
    }

    QDate earliest = QDate::fromString(earliestDate.left(10), "yyyy-MM-dd");
    if (!earliest.isValid()) {
        LoggerManager::getInstance()->log(logPath, Level::ERROR,
            QString("无效的最早日期: %1").arg(earliestDate).toStdString());
        return;
    }

    QDate cutoffDate = earliest.addMonths(DISK_CLEANUP_MONTHS);
    QString startTime = earliest.toString("yyyy-MM-dd 00:00:00");
    QString endTime   = cutoffDate.toString("yyyy-MM-dd 23:59:59");

    // 查询待删除区间的记录数
    int deleteCount = queryTotalCountWithConditions(QString(), QString(), -1, -1, startTime, endTime);

    // 计算涉及的月份列表
    QStringList monthsList;
    QDate monthIter = earliest;
    while (monthIter <= cutoffDate) {
        monthsList.append(monthIter.toString("yyyy-MM"));
        monthIter = monthIter.addMonths(1);
        monthIter = QDate(monthIter.year(), monthIter.month(), 1);
    }
    monthsList.removeDuplicates();
    QString monthsStr = monthsList.join(", ");

    LoggerManager::getInstance()->log(logPath, Level::INFO,
        QString("正在清理日志: %1 至 %2, 涉及月份: [%3], 预计删除记录数: %4, 数据库最早: %5, 最新: %6")
            .arg(startTime, endTime, monthsStr)
            .arg(deleteCount)
            .arg(earliestDate.left(10), latestDate.left(10))
            .toStdString());

    bool success = deleteByTimeRange(startTime, endTime);
    if (success) {
        LoggerManager::getInstance()->log(logPath, Level::INFO,
            QString("清理成功: 已删除 %1 至 %2 的日志记录, 涉及月份: [%3], 删除记录数: %4")
                .arg(startTime, endTime, monthsStr).arg(deleteCount).toStdString());
    } else {
        LoggerManager::getInstance()->log(logPath, Level::ERROR,
            QString("清理失败: 无法删除 %1 至 %2 的日志记录").arg(startTime, endTime).toStdString());
    }
}

int CommunicateLogSqlLogic::calculateOffset(int pageSize, int pageNumber)
{
    if (pageNumber <= 0) {
        return 0;
    }
    return pageSize * (pageNumber - 1);
}

} // namespace LogDB
