#include "operationlogsqllogic.h"
#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QDebug>
#include <QStringList>
#include <algorithm>

namespace LogDB {

namespace {

QVariant buildKeywordLikeValue(const QString& keyword)
{
    if (keyword.isEmpty()) {
        return QVariant();
    }
    return QStringLiteral("%") + keyword + QStringLiteral("%");
}

struct OperationLogQueryParts
{
    QStringList whereConditions;
    QVariantList bindValues;
};

void bindValues(QSqlQuery& query, const QVariantList& values)
{
    for (const QVariant& value : values) {
        query.addBindValue(value);
    }
}

QString trimSqlTerminator(QString sql)
{
    sql = sql.trimmed();
    while (sql.endsWith(QLatin1Char(';'))) {
        sql.chop(1);
        sql = sql.trimmed();
    }
    return sql;
}

OperationLogQueryParts buildOperationLogQueryParts(const QString& startTime,
                                                   const QString& endTime,
                                                   int logType,
                                                   const QString& keyword,
                                                   int maxUserPermission,
                                                   bool useDescriptionSearchIndex = false)
{
    OperationLogQueryParts parts;

    parts.whereConditions << QStringLiteral("user_permission <= ?");
    parts.bindValues << maxUserPermission;

    if (!startTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("occur_time >= ?");
        parts.bindValues << startTime;
    }
    if (!endTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("occur_time <= ?");
        parts.bindValues << endTime;
    }
    if (logType != -1) {
        parts.whereConditions << QStringLiteral("log_type = ?");
        parts.bindValues << logType;
    }
    if (!keyword.isEmpty()) {
        parts.whereConditions << (useDescriptionSearchIndex
            ? QStringLiteral("id IN (SELECT rowid FROM operation_log_description_fts WHERE description LIKE ?)")
            : QStringLiteral("description LIKE ?"));
        parts.bindValues << buildKeywordLikeValue(keyword);
    }

    return parts;
}

QString appendWhere(const QString& baseSql, const OperationLogQueryParts& queryParts)
{
    if (queryParts.whereConditions.isEmpty()) {
        return trimSqlTerminator(baseSql);
    }

    return QStringLiteral("%1 WHERE %2")
        .arg(trimSqlTerminator(baseSql),
             queryParts.whereConditions.join(QStringLiteral(" AND ")));
}

bool ensureOperationLogQueryIndexes(QSqlDatabase& database)
{
    static const QStringList indexStatements = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_operation_log_time_id ON operation_log(occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_operation_log_type_time_id ON operation_log(log_type, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_operation_log_permission_time_id ON operation_log(user_permission, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_operation_log_type_permission_time_id ON operation_log(log_type, user_permission, occur_time DESC, id DESC)")
    };

    QSqlQuery query(database);
    for (const QString& statement : indexStatements) {
        if (!query.exec(statement)) {
            qWarning() << "Failed to create operation_log query index:"
                       << query.lastError().text()
                       << statement;
            return false;
        }
    }
    return true;
}

} // namespace

OperationLogSqlLogic::OperationLogSqlLogic(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_connectionName("OperationLogSqlLogicConnection")
    , m_sqlMapper(nullptr)
    , m_cleanupScheduler(nullptr)
    , m_useDescriptionSearchIndex(false)
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
    // 不手动移除数据库连接，SQLite 会自动处理。
    if (m_sqlMapper) {
        delete m_sqlMapper;
    }
}

bool OperationLogSqlLogic::initializeDatabase()
{
    // 使用通用模块创建并优化 SQLite 连接。
    QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);
    m_database = DBConnectionHelper::openSqlite(dbFilePath, m_connectionName);
    if (!m_database.isOpen()) {
        return false;
    }

    // 向后兼容迁移：若 operation_log 表缺少 user_permission 列则补加。
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
                qWarning() << "[OperationLogSqlLogic] 迁移 user_permission 列失败"
                           << migrate.lastError().text();
            }
        }
    }

    if (!ensureOperationLogQueryIndexes(m_database)) {
        return false;
    }
    m_useDescriptionSearchIndex = initializeDescriptionSearchIndex();

    // 初始化清理调度器。
    initializeCleanupScheduler();

    return true;
}

void OperationLogSqlLogic::initializeCleanupScheduler()
{
    LogCleanupScheduler::Config cfg;
    cfg.checkIntervalMs = 60000;
    cfg.retainMonths = 6;
    cfg.cleanupMonths = 1;
    cfg.databaseName = QStringLiteral("运行日志数据库");
    cfg.tableName = QStringLiteral("operation_log");
    cfg.logPath = "log_db/database_cleanup/month_clean";
    m_cleanupScheduler = new LogCleanupScheduler(cfg, this);
    m_cleanupScheduler->setMonthRangeProvider([this]() {
        return queryMonthRange();
    });
    m_cleanupScheduler->setDeleteByRangeFn([this](const QString& s, const QString& e) {
        return deleteByTimeRange(s, e);
    });
    m_cleanupScheduler->start();
}

bool OperationLogSqlLogic::initializeDescriptionSearchIndex()
{
    if (!m_database.isOpen()) {
        return false;
    }

    QSqlQuery existsQuery(m_database);
    existsQuery.prepare(QStringLiteral(
        "SELECT 1 FROM sqlite_master "
        "WHERE type = 'table' AND name = 'operation_log_description_fts'"));
    const bool tableExisted = existsQuery.exec() && existsQuery.next();

    QSqlQuery query(m_database);
    const QString createFts = QStringLiteral(
        "CREATE VIRTUAL TABLE IF NOT EXISTS operation_log_description_fts "
        "USING fts5(description, content='operation_log', content_rowid='id', tokenize='trigram')");
    if (!query.exec(createFts)) {
        qWarning() << "[OperationLogSqlLogic] FTS5 trigram description index unavailable:"
                   << query.lastError().text();
        return false;
    }

    const QStringList triggerStatements = {
        QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS operation_log_description_fts_ai "
            "AFTER INSERT ON operation_log BEGIN "
            "INSERT INTO operation_log_description_fts(rowid, description) "
            "VALUES (new.id, new.description); "
            "END"),
        QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS operation_log_description_fts_ad "
            "AFTER DELETE ON operation_log BEGIN "
            "INSERT INTO operation_log_description_fts(operation_log_description_fts, rowid, description) "
            "VALUES ('delete', old.id, old.description); "
            "END"),
        QStringLiteral(
            "CREATE TRIGGER IF NOT EXISTS operation_log_description_fts_au "
            "AFTER UPDATE OF description ON operation_log BEGIN "
            "INSERT INTO operation_log_description_fts(operation_log_description_fts, rowid, description) "
            "VALUES ('delete', old.id, old.description); "
            "INSERT INTO operation_log_description_fts(rowid, description) "
            "VALUES (new.id, new.description); "
            "END")
    };

    for (const QString& statement : triggerStatements) {
        if (!query.exec(statement)) {
            qWarning() << "[OperationLogSqlLogic] Failed to create description FTS trigger:"
                       << query.lastError().text()
                       << statement;
            return false;
        }
    }

    if (!tableExisted) {
        if (!query.exec(QStringLiteral(
                "INSERT INTO operation_log_description_fts(operation_log_description_fts) "
                "VALUES ('rebuild')"))) {
            qWarning() << "[OperationLogSqlLogic] Failed to rebuild description FTS index:"
                       << query.lastError().text();
            return false;
        }
    }

    qDebug() << "[OperationLogSqlLogic] FTS5 trigram description index enabled";
    return true;
}

bool OperationLogSqlLogic::insertRecord(const QString& occurTime, int logType, const QString& description,
                                       int userPermission)
{
    QString sql = m_sqlMapper->getSql("insert_record");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: insert_record";
        return false;
    }

    // 构造 WriteResult 传递给 WriteSqlDBCon。
    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql;
    result.sqlId = "insert_record";
    result.result = "";
    result.tableName = "operation_log";
    result.opType = static_cast<int>(WriteOp::Insert);

    // 参数顺序必须与 insert_record SQL 保持一致。
    result.params << occurTime << logType << description << userPermission;

    // 发出写入请求，由 WriteSqlDBCon 统一执行。
    emit writeExecuted(result);

    return true;
}

QList<QVariantMap> OperationLogSqlLogic::queryPagination(int pageSize, int pageNumber, int maxUserPermission)
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
    query.addBindValue(maxUserPermission);
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

int OperationLogSqlLogic::queryTotalCount(int maxUserPermission)
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
    query.addBindValue(maxUserPermission);

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
                                                                int pageSize, int pageNumber,
                                                                int maxUserPermission)
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
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, -1, QString(), maxUserPermission);
    sql = QStringLiteral("%1 ORDER BY occur_time DESC, id DESC LIMIT ? OFFSET ?")
              .arg(appendWhere(sql, queryParts));
    int offset = calculateOffset(pageSize, pageNumber);

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
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

QList<QVariantMap> OperationLogSqlLogic::queryPaginationWithBaseConditions(const QString& startTime, const QString& endTime,
                                                                           int logType, int pageSize, int pageNumber,
                                                                           int maxUserPermission)
{
    QList<QVariantMap> results;
    if (!m_database.isOpen()) {
        return results;
    }
    QString sql = m_sqlMapper->getSql("query_pagination_with_base_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_pagination_with_base_conditions";
        return results;
    }

    int offset = calculateOffset(pageSize, pageNumber);
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, QString(), maxUserPermission);
    sql = QStringLiteral("%1 ORDER BY occur_time DESC, id DESC LIMIT ? OFFSET ?")
              .arg(appendWhere(sql, queryParts));

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
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

QList<QVariantMap> OperationLogSqlLogic::queryPaginationAfterBaseConditions(int anchorRecordId,
                                                                            const QString& startTime,
                                                                            const QString& endTime,
                                                                            int logType,
                                                                            int pageSize,
                                                                            int maxUserPermission)
{
    QList<QVariantMap> results;
    if (!m_database.isOpen() || anchorRecordId <= 0 || pageSize <= 0) {
        return results;
    }

    QString baseSql = m_sqlMapper->getSql("query_pagination_with_base_conditions");
    if (baseSql.isEmpty()) {
        qWarning() << "SQL not found: query_pagination_with_base_conditions";
        return results;
    }

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, QString(), maxUserPermission);
    const QString sql = QStringLiteral("%1 AND ("
                                       "occur_time < (SELECT occur_time FROM operation_log WHERE id = ?) "
                                       "OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?) "
                                       "AND id < ?)) "
                                       "ORDER BY occur_time DESC, id DESC LIMIT ?")
                            .arg(appendWhere(baseSql, queryParts));

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(anchorRecordId);
    query.addBindValue(anchorRecordId);
    query.addBindValue(anchorRecordId);
    query.addBindValue(pageSize);

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

QList<QVariantMap> OperationLogSqlLogic::queryPaginationBeforeBaseConditions(int anchorRecordId,
                                                                             const QString& startTime,
                                                                             const QString& endTime,
                                                                             int logType,
                                                                             int pageSize,
                                                                             int maxUserPermission)
{
    QList<QVariantMap> results;
    if (!m_database.isOpen() || anchorRecordId <= 0 || pageSize <= 0) {
        return results;
    }

    QString baseSql = m_sqlMapper->getSql("query_pagination_with_base_conditions");
    if (baseSql.isEmpty()) {
        qWarning() << "SQL not found: query_pagination_with_base_conditions";
        return results;
    }

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, QString(), maxUserPermission);
    const QString sql = QStringLiteral("%1 AND ("
                                       "occur_time > (SELECT occur_time FROM operation_log WHERE id = ?) "
                                       "OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?) "
                                       "AND id > ?)) "
                                       "ORDER BY occur_time ASC, id ASC LIMIT ?")
                            .arg(appendWhere(baseSql, queryParts));

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(anchorRecordId);
    query.addBindValue(anchorRecordId);
    query.addBindValue(anchorRecordId);
    query.addBindValue(pageSize);

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
    std::reverse(results.begin(), results.end());
    return results;
}

int OperationLogSqlLogic::queryTotalCountInRange(const QString& startTime, const QString& endTime,
                                                 int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }
    // 无范围时直接查询当前权限可见的总数。
    if (startTime.isEmpty() && endTime.isEmpty()) {
        return queryTotalCount(maxUserPermission);
    }
    QString sql = m_sqlMapper->getSql("query_total_count_in_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count_in_range";
        return 0;
    }
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, -1, QString(), maxUserPermission);
    sql = appendWhere(sql, queryParts);
    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
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

int OperationLogSqlLogic::queryTotalCountWithBaseConditions(const QString& startTime, const QString& endTime,
                                                            int logType, int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_total_count_with_base_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count_with_base_conditions";
        return 0;
    }

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, QString(), maxUserPermission);
    sql = appendWhere(sql, queryParts);

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
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
                                                 int pageSize, int maxUserPermission)
{
    if (!m_database.isOpen() || recordId <= 0 || pageSize <= 0) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_record_page_in_range");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_record_page_in_range";
        return 0;
    }
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, -1, QString(), maxUserPermission);
    sql = QStringLiteral("%1 AND ("
                         "occur_time > (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "AND id >= ?))"
                         ") AS sub")
              .arg(appendWhere(sql, queryParts));
    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(pageSize);
    query.addBindValue(pageSize);
    bindValues(query, queryParts.bindValues);
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

int OperationLogSqlLogic::queryRecordPageWithBaseConditions(int recordId, const QString& startTime, const QString& endTime,
                                                            int logType, int pageSize, int maxUserPermission)
{
    if (!m_database.isOpen() || recordId <= 0 || pageSize <= 0) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_record_page_with_base_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_record_page_with_base_conditions";
        return 0;
    }

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, QString(), maxUserPermission);
    sql = QStringLiteral("%1 AND ("
                         "occur_time > (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "AND id >= ?))"
                         ") AS sub")
              .arg(appendWhere(sql, queryParts));

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(pageSize);
    query.addBindValue(pageSize);
    bindValues(query, queryParts.bindValues);
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
                                                       int pageSize, int pageNumber,
                                                       int maxUserPermission)
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
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = QStringLiteral("%1 ORDER BY occur_time DESC, id DESC LIMIT ? OFFSET ?")
              .arg(appendWhere(sql, queryParts));

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
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

int OperationLogSqlLogic::queryTotalCountWithConditions(const QString& startTime, const QString& endTime,
                                            int logType, const QString& keyword, int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_total_count_with_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count_with_conditions";
        return 0;
    }

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = appendWhere(sql, queryParts);

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);

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
                                              int logType, const QString& keyword, int maxUserPermission)
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

    QSqlQuery query(m_database);
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    const QString whereSql = queryParts.whereConditions.join(QStringLiteral(" AND "));
    sql = QStringLiteral(
        "SELECT CASE "
        "WHEN (SELECT COUNT(*) FROM operation_log WHERE %1 AND id = ?) > 0 "
        "THEN (SELECT COUNT(*) + 1 FROM operation_log WHERE %1 "
        "AND (occur_time > (SELECT occur_time FROM operation_log WHERE id = ?) "
        "OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?) AND id > ?))) "
        "ELSE 0 END AS position")
              .arg(whereSql);

    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(recordId);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(recordId);
    query.addBindValue(recordId);
    query.addBindValue(recordId);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return -1;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}
int OperationLogSqlLogic::queryFirstRecordPage(const QString& startTime, const QString& endTime,
                                               int logType, const QString& keyword, int pageSize,
                                               int maxUserPermission)
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

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = QStringLiteral("SELECT (first_id + ? - 1) / ? AS page_number FROM ("
                         "%1 ORDER BY occur_time DESC, id DESC LIMIT 1"
                         ") subquery")
              .arg(appendWhere(sql, queryParts).replace(QStringLiteral("SELECT *"),
                                                        QStringLiteral("SELECT id AS first_id")));

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(pageSize);
    query.addBindValue(pageSize);
    bindValues(query, queryParts.bindValues);

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
                                              int logType, const QString& keyword, int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_first_matched_id");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_first_matched_id";
        return 0;
    }
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = QStringLiteral("%1 ORDER BY occur_time DESC, id DESC LIMIT 1")
              .arg(appendWhere(sql, queryParts));
    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int OperationLogSqlLogic::queryLastMatchedId(const QString& startTime, const QString& endTime,
                                             int logType, const QString& keyword, int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_last_matched_id");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_last_matched_id";
        return 0;
    }
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = QStringLiteral("%1 ORDER BY occur_time ASC, id ASC LIMIT 1")
              .arg(appendWhere(sql, queryParts));
    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

int OperationLogSqlLogic::queryMatchedIdByPosition(int position, const QString& startTime, const QString& endTime,
                                                   int logType, const QString& keyword, int maxUserPermission)
{
    if (!m_database.isOpen() || position <= 0) {
        return 0;
    }
    QString sql = m_sqlMapper->getSql("query_matched_id_by_position");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_matched_id_by_position";
        return 0;
    }
    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = QStringLiteral("%1 ORDER BY occur_time DESC, id DESC LIMIT 1 OFFSET ?")
              .arg(appendWhere(sql, queryParts));
    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(position - 1);
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
                                              int logType, const QString& keyword, int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_prev_matching_id");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_prev_matching_id";
        return 0;
    }

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = QStringLiteral("%1 AND ("
                         "occur_time < (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "AND id < ?)) "
                         "ORDER BY occur_time DESC, id DESC LIMIT 1")
              .arg(appendWhere(sql, queryParts));

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(anchorId);
    query.addBindValue(anchorId);
    query.addBindValue(anchorId);

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
                                              int logType, const QString& keyword, int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    QString sql = m_sqlMapper->getSql("query_next_matching_id");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_next_matching_id";
        return 0;
    }

    const OperationLogQueryParts queryParts =
        buildOperationLogQueryParts(startTime, endTime, logType, keyword,
                                    maxUserPermission, m_useDescriptionSearchIndex);
    sql = QStringLiteral("%1 AND ("
                         "occur_time > (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "OR (occur_time = (SELECT occur_time FROM operation_log WHERE id = ?) "
                         "AND id > ?)) "
                         "ORDER BY occur_time ASC, id ASC LIMIT 1")
              .arg(appendWhere(sql, queryParts));

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(anchorId);
    query.addBindValue(anchorId);
    query.addBindValue(anchorId);

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

    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        qWarning() << "Query failed:" << query.lastError().text();
        return result;
    }

    if (query.next()) {
        result["earliest_time"] = query.value("earliest_time");
        result["latest_time"] = query.value("latest_time");
        result["earliest_date"] = query.value("earliest_date");
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

    // 构造 WriteResult 传递给 WriteSqlDBCon。
    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql;
    result.sqlId = "delete_by_time_range";
    result.result = "";
    result.tableName = "operation_log";
    result.opType = static_cast<int>(WriteOp::Delete);

    // 参数顺序必须与 delete_by_time_range SQL 保持一致。
    result.params << startTime << endTime;

    // 发出写入请求，由 WriteSqlDBCon 统一执行。
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
