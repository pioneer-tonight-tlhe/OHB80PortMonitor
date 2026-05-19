#include "alarmlogsqllogic.h"
#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QDebug>

namespace LogDB {

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

    initializeCleanupScheduler();
    return true;
}

void AlarmLogSqlLogic::initializeCleanupScheduler()
{
    LogCleanupScheduler::Config cfg;
    cfg.checkIntervalMs = 60000;
    cfg.retainMonths = 7;
    cfg.cleanupMonths = 1;
    m_cleanupScheduler = new LogCleanupScheduler(cfg, this);
    m_cleanupScheduler->setMonthRangeProvider([this]() {
        return queryMonthRange();
    });
    m_cleanupScheduler->setDeleteByRangeFn([this](const QString& s, const QString& e) {
        deleteByTimeRange(s, e);
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
                                                             int pageNumber)
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
    // alarm_level（-1表示不应用）
    query.addBindValue(alarmLevel == -1 ? QVariant() : alarmLevel);
    query.addBindValue(alarmLevel == -1 ? QVariant() : alarmLevel);
    // qr_code
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    // alarm_type
    query.addBindValue(alarmType.isEmpty() ? QVariant() : alarmType);
    query.addBindValue(alarmType.isEmpty() ? QVariant() : alarmType);
    // is_resolved（-1表示不应用）
    query.addBindValue(isResolved == -1 ? QVariant() : isResolved);
    query.addBindValue(isResolved == -1 ? QVariant() : isResolved);
    // 时间区间（NULL检查 + start + end）
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);
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

int AlarmLogSqlLogic::queryTotalCount()
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

int AlarmLogSqlLogic::queryTotalCountWithConditions(int alarmLevel,
                                                    const QString& qrCode,
                                                    const QString& alarmType,
                                                    int isResolved,
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
    query.addBindValue(alarmLevel == -1 ? QVariant() : alarmLevel);
    query.addBindValue(alarmLevel == -1 ? QVariant() : alarmLevel);
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    query.addBindValue(qrCode.isEmpty() ? QVariant() : qrCode);
    query.addBindValue(alarmType.isEmpty() ? QVariant() : alarmType);
    query.addBindValue(alarmType.isEmpty() ? QVariant() : alarmType);
    query.addBindValue(isResolved == -1 ? QVariant() : isResolved);
    query.addBindValue(isResolved == -1 ? QVariant() : isResolved);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : startTime);
    query.addBindValue(startTime.isEmpty() ? QVariant() : endTime);

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
