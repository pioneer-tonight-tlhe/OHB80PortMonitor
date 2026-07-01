#include "alarmlogsqllogic.h"
#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QDebug>
#include <QStringList>

namespace LogDB {

namespace {

struct AlarmLogQueryParts
{
    QStringList whereConditions;
    QVariantList bindValues;
};

AlarmLogQueryParts buildAlarmLogQueryParts(int alarmLevel,
                                           const QString& qrCode,
                                           const QString& alarmType,
                                           int isResolved,
                                           const QString& startTime,
                                           const QString& endTime,
                                           const QString& resolveStartTime,
                                           const QString& resolveEndTime,
                                           int maxUserPermission)
{
    AlarmLogQueryParts parts;

    if (alarmLevel != -1) {
        parts.whereConditions << QStringLiteral("alarm_level = ?");
        parts.bindValues << alarmLevel;
    }
    if (!qrCode.isEmpty()) {
        parts.whereConditions << QStringLiteral("qr_code = ?");
        parts.bindValues << qrCode;
    }
    if (!alarmType.isEmpty()) {
        parts.whereConditions << QStringLiteral("alarm_type = ?");
        parts.bindValues << alarmType;
    }
    if (isResolved != -1) {
        parts.whereConditions << QStringLiteral("is_resolved = ?");
        parts.bindValues << isResolved;
    }
    if (!startTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("occur_time >= ?");
        parts.bindValues << startTime;
    }
    if (!endTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("occur_time <= ?");
        parts.bindValues << endTime;
    }
    if (!resolveStartTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("resolve_time >= ?");
        parts.bindValues << resolveStartTime;
    }
    if (!resolveEndTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("resolve_time <= ?");
        parts.bindValues << resolveEndTime;
    }
    if (maxUserPermission >= 0) {
        parts.whereConditions << QStringLiteral("user_permission <= ?");
        parts.bindValues << maxUserPermission;
    }

    return parts;
}

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

bool ensureAlarmLogQueryIndexes(QSqlDatabase& database)
{
    static const QStringList indexStatements = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_time_id ON alarm_log(occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_resolved_time_id ON alarm_log(is_resolved, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_qr_time_id ON alarm_log(qr_code, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_type_time_id ON alarm_log(alarm_type, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_level_time_id ON alarm_log(alarm_level, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_resolve_time_id ON alarm_log(resolve_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_resolved_resolve_time_id ON alarm_log(is_resolved, resolve_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_qr_type_resolved_time_id ON alarm_log(qr_code, alarm_type, is_resolved, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_permission_time_id ON alarm_log(user_permission, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_qr_permission_time_id ON alarm_log(qr_code, user_permission, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_type_permission_time_id ON alarm_log(alarm_type, user_permission, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_level_permission_time_id ON alarm_log(alarm_level, user_permission, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_resolved_permission_time_id ON alarm_log(is_resolved, user_permission, occur_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_log_qr_type_resolved_permission_time_id ON alarm_log(qr_code, alarm_type, is_resolved, user_permission, occur_time DESC, id DESC)")
    };

    QSqlQuery query(database);
    for (const QString& statement : indexStatements) {
        if (!query.exec(statement)) {
            qWarning() << "Failed to create alarm_log query index:"
                       << query.lastError().text()
                       << statement;
            return false;
        }
    }
    return true;
}

} // namespace

AlarmLogSqlLogic::AlarmLogSqlLogic(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_connectionName("AlarmLogSqlLogicConnection")
    , m_sqlMapper(nullptr)
    , m_cleanupScheduler(nullptr)
{
    QString sqlFilePath = QString("%1/alarm_log_queries.sql").arg(databasePath);
    m_sqlMapper = new SqlMapper(sqlFilePath);
}

AlarmLogSqlLogic::~AlarmLogSqlLogic()
{
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

bool AlarmLogSqlLogic::initializeDatabase()
{
    QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);
    m_database = DBConnectionHelper::openSqlite(dbFilePath, m_connectionName);
    if (!m_database.isOpen()) {
        return false;
    }
    if (!ensureAlarmLogQueryIndexes(m_database)) {
        return false;
    }

    initializeCleanupScheduler();
    return true;
}

void AlarmLogSqlLogic::initializeCleanupScheduler()
{
    LogCleanupScheduler::Config cfg;
    cfg.checkIntervalMs = 60000;
    cfg.retainMonths = 6;
    cfg.cleanupMonths = 1;
    cfg.databaseName = QStringLiteral("报警日志数据库");
    cfg.tableName = QStringLiteral("alarm_log");
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

bool AlarmLogSqlLogic::insertRecord(int alarmLevel,
                                    const QString& occurTime,
                                    const QString& qrCode,
                                    const QString& alarmType,
                                    int isResolved,
                                    const QString& resolveTime,
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
    result.tableName = "alarm_log";
    result.opType = static_cast<int>(WriteOp::Insert);

    // 顺序与 SQL ((alarm_level, occur_time, qr_code, alarm_type, is_resolved,
    //                resolve_time, description, user_permission)) 严格对齐
    result.params << alarmLevel
                  << occurTime
                  << qrCode
                  << alarmType
                  << isResolved
                  << (resolveTime.isEmpty() ? QVariant() : QVariant(resolveTime))
                  << description
                  << userPermission;

    emit writeExecuted(result);
    return true;
}

QList<QVariantMap> AlarmLogSqlLogic::queryPageWithConditions(int alarmLevel,
                                                             const QString& qrCode,
                                                             const QString& alarmType,
                                                             int isResolved,
                                                             const QString& startTime,
                                                             const QString& endTime,
                                                             int pageSize,
                                                             int pageNumber,
                                                             const QString& resolveStartTime,
                                                             const QString& resolveEndTime,
                                                             int maxUserPermission)
{
    QList<QVariantMap> results;

    if (!m_database.isOpen()) {
        return results;
    }

    const AlarmLogQueryParts queryParts = buildAlarmLogQueryParts(alarmLevel,
                                                                  qrCode,
                                                                  alarmType,
                                                                  isResolved,
                                                                  startTime,
                                                                  endTime,
                                                                  resolveStartTime,
                                                                  resolveEndTime,
                                                                  maxUserPermission);
    const QString baseSql = trimSqlTerminator(m_sqlMapper->getSql("query_page_with_conditions"));
    if (baseSql.isEmpty()) {
        qWarning() << "SQL not found: query_page_with_conditions";
        return results;
    }

    const QString whereSql = queryParts.whereConditions.isEmpty()
        ? QString()
        : QStringLiteral(" WHERE %1").arg(queryParts.whereConditions.join(QStringLiteral(" AND ")));
    const QString sql = QStringLiteral("%1%2 "
                                       "ORDER BY occur_time DESC, id DESC LIMIT ? OFFSET ?")
                            .arg(baseSql, whereSql);

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);
    query.addBindValue(pageSize);
    query.addBindValue(calculateOffset(pageSize, pageNumber));

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

int AlarmLogSqlLogic::queryTotalCount(int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    if (maxUserPermission >= 0) {
        QString sql = m_sqlMapper->getSql("query_total_count_with_permission");
        if (sql.isEmpty()) {
            qWarning() << "SQL not found: query_total_count_with_permission";
            return 0;
        }
        QSqlQuery permissionQuery(m_database);
        permissionQuery.prepare(sql);
        permissionQuery.addBindValue(maxUserPermission);
        if (!permissionQuery.exec()) {
            qWarning() << "Query failed:" << permissionQuery.lastError().text();
            return 0;
        }
        return permissionQuery.next() ? permissionQuery.value(0).toInt() : 0;
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

int AlarmLogSqlLogic::queryTotalCountWithConditions(int alarmLevel,
                                                    const QString& qrCode,
                                                    const QString& alarmType,
                                                    int isResolved,
                                                    const QString& startTime,
                                                    const QString& endTime,
                                                    const QString& resolveStartTime,
                                                    const QString& resolveEndTime,
                                                    int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    const AlarmLogQueryParts queryParts = buildAlarmLogQueryParts(alarmLevel,
                                                                  qrCode,
                                                                  alarmType,
                                                                  isResolved,
                                                                  startTime,
                                                                  endTime,
                                                                  resolveStartTime,
                                                                  resolveEndTime,
                                                                  maxUserPermission);
    const QString baseSql = trimSqlTerminator(m_sqlMapper->getSql("query_total_count_with_conditions"));
    if (baseSql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count_with_conditions";
        return 0;
    }

    const QString whereSql = queryParts.whereConditions.isEmpty()
        ? QString()
        : QStringLiteral(" WHERE %1").arg(queryParts.whereConditions.join(QStringLiteral(" AND ")));
    const QString sql = QStringLiteral("%1%2").arg(baseSql, whereSql);

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

QVariantMap AlarmLogSqlLogic::queryMonthRange()
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

bool AlarmLogSqlLogic::deleteByTimeRange(const QString& startTime, const QString& endTime)
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
    result.tableName = "alarm_log";
    result.opType = static_cast<int>(WriteOp::Delete);

    result.params << startTime << endTime;

    emit writeExecuted(result);
    return true;
}

bool AlarmLogSqlLogic::updateResolve(const QString& qrCode, const QString& alarmType, const QString& resolveTime)
{
    QString sql = m_sqlMapper->getSql("update_resolve");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: update_resolve";
        return false;
    }

    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql;
    result.sqlId = "update_resolve";
    result.result = "";
    result.tableName = "alarm_log";
    // UPDATE 不改变记录数，使用 Other 避免触发 log_record_count 增减
    result.opType = static_cast<int>(WriteOp::Other);

    // 顺序对应 SQL: resolve_time, qr_code, alarm_type
    result.params << resolveTime << qrCode << alarmType;

    emit writeExecuted(result);
    return true;
}

int AlarmLogSqlLogic::calculateOffset(int pageSize, int pageNumber)
{
    if (pageNumber <= 0) {
        return 0;
    }
    return pageSize * (pageNumber - 1);
}

} // namespace LogDB
