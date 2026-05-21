#include "operationlogsqllogic.h"
#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QDebug>

namespace LogDB {

OperationLogSqlLogic::OperationLogSqlLogic(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_connectionName("OperationLogSqlLogicConnection")
    , m_sqlMapper(nullptr)
    , m_cleanupScheduler(nullptr)
{
    QString sqlFilePath = QString("%1/operation_log_queries.sql").arg(databasePath);
    m_sqlMapper = new SqlMapper(sqlFilePath);
}

OperationLogSqlLogic::~OperationLogSqlLogic()
{
    if (m_cleanupScheduler) {
        m_cleanupScheduler->stop();
        delete m_cleanupScheduler;
        m_cleanupScheduler = nullptr;
    }
    if (m_database.isOpen()) {
        m_database.close();
    }
    // 不手动移除数据库连接，SQLite会自动处理
    if (m_sqlMapper) {
        delete m_sqlMapper;
    }
}

bool OperationLogSqlLogic::initializeDatabase()
{
    // 使用通用模块创建并优化SQLite连接
    QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);
    m_database = DBConnectionHelper::openSqlite(dbFilePath, m_connectionName);
    if (!m_database.isOpen()) {
        return false;
    }

    // 向后兼容迁移：若 operation_log 表缺少 user_permission 列则补加
    {
        QSqlQuery pragma(m_database);
        pragma.exec(QStringLiteral("PRAGMA table_info(operation_log)"));
        bool hasUserPermission = false;
        while (pragma.next()) {
            if (pragma.value(QStringLiteral("name")).toString()
                    == QStringLiteral("user_permission")) {
                hasUserPermission = true;
                break;
            }
        }
        if (!hasUserPermission) {
            QSqlQuery migrate(m_database);
            if (!migrate.exec(QStringLiteral(
                    "ALTER TABLE operation_log "
                    "ADD COLUMN user_permission INTEGER NOT NULL DEFAULT 0"))) {
                qWarning() << "[OperationLogSqlLogic] 迁移 user_permission 列失败:"
                           << migrate.lastError().text();
            } else {
                qDebug() << "[OperationLogSqlLogic] 迁移完成：已添加 user_permission 列";
            }
        }
    }

    // 初始化清理调度器
    initializeCleanupScheduler();

    return true;
}

void OperationLogSqlLogic::initializeCleanupScheduler()
{
    LogCleanupScheduler::Config cfg;
    cfg.checkIntervalMs = 60000;
    cfg.retainMonths = 7;
    cfg.cleanupMonths = 1;
    cfg.logPath = "log_db/operation_log_db/month_clean";
    m_cleanupScheduler = new LogCleanupScheduler(cfg, this);
    m_cleanupScheduler->setMonthRangeProvider([this]() {
        return queryMonthRange();
    });
    m_cleanupScheduler->setDeleteByRangeFn([this](const QString& s, const QString& e) {
        deleteByTimeRange(s, e);
    });
    m_cleanupScheduler->start();
}

bool OperationLogSqlLogic::insertRecord(const QString& occurTime, int logType, const QString& description,
                                       int userPermission)
{
    QString sql = m_sqlMapper->getSql("insert_record");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: insert_record";
        return false;
    }

    // 构造WriteResult传递给WriteSqlDBCon
    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql; // 传递带占位符的SQL语句
    result.sqlId = "insert_record";
    result.result = ""; // 待WriteSqlDBCon执行后填充
    result.tableName = "operation_log";
    result.opType = static_cast<int>(WriteOp::Insert);

    // 构造参数列表（顺序与 SQL ((occur_time, log_type, description, user_permission)) 严格对齐）
    result.params << occurTime << logType << description << userPermission;

    // 发出信号，传递SQL语句和参数给WriteSqlDBCon
    emit writeExecuted(result);

    return true;
}

QList<QVariantMap> OperationLogSqlLogic::queryPagination(int pageSize, int pageNumber)
{
    QList<QVariantMap> results;

    if (!m_database.isOpen()) {
        return results;
    }

    QString sql = m_sqlMapper->getSql("query_pagination");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_pagination";
        return results;
    }

    int offset = calculateOffset(pageSize, pageNumber);

    QSqlQuery query(m_database);
    query.prepare(sql);
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

int OperationLogSqlLogic::queryTotalCount()
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

QList<QVariantMap> OperationLogSqlLogic::queryPaginationInRange(const QString& startTime, const QString& endTime,
                                                                int pageSize, int pageNumber)
{
    QList<QVariantMap> results;
    if (!m_database.isOpen()) {
        return results;
    }
    QString sql = m_sqlMapper->getSql("query_pagination_in_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_pagination_in_range";
        return results;
    }
    int offset = calculateOffset(pageSize, pageNumber);

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
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

int OperationLogSqlLogic::queryTotalCountInRange(const QString& startTime, const QString& endTime)
{
    if (!m_database.isOpen()) {
        return 0;
    }
    // 无范围时直接取缓存表的全表总数
    if (startTime.isEmpty()) {
        return queryTotalCount();
    }
    QString sql = m_sqlMapper->getSql("query_total_count_in_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count_in_range";
        return 0;
    }
    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(startTime);
    query.addBindValue(startTime);
    query.addBindValue(endTime);
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

int OperationLogSqlLogic::queryRecordPageInRange(int recordId, const QString& startTime, const QString& endTime,
                                                 int pageSize)
{
    if (!m_database.isOpen() || recordId <= 0 || pageSize <= 0) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_record_page_in_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_record_page_in_range";
        return 0;
    }
    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(pageSize);
    query.addBindValue(pageSize);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(recordId);
    query.addBindValue(recordId);
    query.addBindValue(recordId);
    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

QList<QVariantMap> OperationLogSqlLogic::queryPageWithConditions(const QString& startTime, const QString& endTime,
                                                       int logType, const QString& keyword,
                                                       int pageSize, int pageNumber)
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

    int offset = calculateOffset(pageSize, pageNumber);

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(pageSize);
    query.addBindValue(offset);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);

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

int OperationLogSqlLogic::queryTotalCountWithConditions(const QString& startTime, const QString& endTime,
                                            int logType, const QString& keyword)
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
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);

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

int OperationLogSqlLogic::queryRecordPosition(int recordId, const QString& startTime, const QString& endTime,
                                              int logType, const QString& keyword)
{
    if (!m_database.isOpen()) {
        qWarning() << "Database is not open";
        return -1;
    }

    QString sql = m_sqlMapper->getSql("query_record_position");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_record_position";
        return -1;
    }

    qDebug() << "Executing query_record_position for recordId:" << recordId
             << "startTime:" << startTime << "endTime:" << endTime
             << "logType:" << logType << "keyword:" << keyword;
    qDebug() << "SQL:" << sql;

    QSqlQuery query(m_database);
    query.prepare(sql);
    // COUNT检查子查询的参数
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(recordId);
    // COUNT位置子查询的参数
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(recordId);

    qDebug() << "Bound values:";
    qDebug() << "  startTime:" << (startTime.isEmpty() ? "NULL" : startTime);
    qDebug() << "  endTime:" << (endTime.isEmpty() ? "NULL" : endTime);
    qDebug() << "  logType:" << (logType == -1 ? "NULL" : QString::number(logType));
    qDebug() << "  keyword:" << (keyword.isEmpty() ? "NULL" : keyword);
    qDebug() << "  recordId:" << recordId;

    // 调试：查询该记录的实际log_type
    QSqlQuery debugQuery(m_database);
    debugQuery.prepare("SELECT id, log_type FROM operation_log WHERE id = ?");
    debugQuery.addBindValue(recordId);
    if (debugQuery.exec() && debugQuery.next()) {
        qDebug() << "Debug: Record" << recordId << "exists with log_type:" << debugQuery.value(1).toInt();
    } else {
        qDebug() << "Debug: Record" << recordId << "does not exist or query failed";
    }

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return -1;
    }

    qDebug() << "Query executed, has rows:" << query.size();

    if (query.next()) {
        int pos = query.value(0).toInt();
        qDebug() << "Position returned:" << pos;
        return pos;
    }

    qWarning() << "No rows returned from query";
    return -1;
}

int OperationLogSqlLogic::queryFirstRecordPage(const QString& startTime, const QString& endTime,
                                               int logType, const QString& keyword, int pageSize)
{
    if (!m_database.isOpen()) {
        qWarning() << "Database is not open";
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_first_record_page");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_first_record_page";
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);
    // (first_id + ? - 1) / ? 中的两个 pageSize 占位符
    query.addBindValue(pageSize);
    query.addBindValue(pageSize);
    // 条件占位符
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int OperationLogSqlLogic::queryFirstMatchedId(const QString& startTime, const QString& endTime,
                                              int logType, const QString& keyword)
{
    if (!m_database.isOpen()) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_first_matched_id");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_first_matched_id";
        return 0;
    }
    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int OperationLogSqlLogic::queryPrevMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                                              int logType, const QString& keyword)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_prev_matching_id");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_prev_matching_id";
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(anchorId);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }

    if (query.next()) {
        QVariant v = query.value(0);
        return v.isNull() ? 0 : v.toInt();
    }
    return 0;
}

int OperationLogSqlLogic::queryNextMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                                              int logType, const QString& keyword)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_next_matching_id");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_next_matching_id";
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(anchorId);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(logType == -1 ? QVariant() : logType);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);
    query.addBindValue(keyword.isEmpty() ? QVariant() : keyword);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }

    if (query.next()) {
        QVariant v = query.value(0);
        return v.isNull() ? 0 : v.toInt();
    }
    return 0;
}

QVariantMap OperationLogSqlLogic::queryMonthRange()
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

    qDebug() << "Executing SQL:" << sql;

    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        qWarning() << "Query failed:" << query.lastError().text();
        return result;
    }

    qDebug() << "Query executed successfully, rows:" << query.size();

    if (query.next()) {
        result["earliest_time"] = query.value("earliest_time");
        result["latest_time"] = query.value("latest_time");
        result["earliest_date"] = query.value("earliest_date");

        qDebug() << "earliest_time:" << result["earliest_time"].toString()
                 << "latest_time:" << result["latest_time"].toString()
                 << "earliest_date:" << result["earliest_date"].toString();
    } else {
        qWarning() << "No rows returned from query";
    }

    return result;
}

bool OperationLogSqlLogic::deleteByTimeRange(const QString& startTime, const QString& endTime)
{
    QString sql = m_sqlMapper->getSql("delete_by_time_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: delete_by_time_range";
        return false;
    }

    // 构造WriteResult传递给WriteSqlDBCon
    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql; // 传递带占位符的SQL语句
    result.sqlId = "delete_by_time_range";
    result.result = ""; // 待WriteSqlDBCon执行后填充
    result.tableName = "operation_log";
    result.opType = static_cast<int>(WriteOp::Delete);

    // 构造参数列表
    result.params << startTime << endTime;

    // 发出信号，传递SQL语句和参数给WriteSqlDBCon
    emit writeExecuted(result);

    return true;
}

int OperationLogSqlLogic::calculateOffset(int pageSize, int pageNumber)
{
    if (pageNumber <= 0) {
        return 0;
    }
    return pageSize * (pageNumber - 1);
}

} // namespace LogDB
