#include "communicatelogsqllogic.h"

#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>

namespace LogDB {

namespace {

struct CommunicateLogQueryParts
{
    QStringList whereConditions;
    QVariantList bindValues;
};

CommunicateLogQueryParts buildCommunicateLogQueryParts(const QString& commandId,
                                                        const QString& qrCode,
                                                        int execStatus,
                                                        int retryCount,
                                                        const QString& startTime,
                                                        const QString& endTime,
                                                        int maxUserPermission)
{
    CommunicateLogQueryParts parts;
    parts.whereConditions << QStringLiteral("user_permission <= ?");
    parts.bindValues << maxUserPermission;

    if (!commandId.isEmpty()) {
        parts.whereConditions << QStringLiteral("command_id = ?");
        parts.bindValues << commandId;
    }
    if (!qrCode.isEmpty()) {
        parts.whereConditions << QStringLiteral("qr_code = ?");
        parts.bindValues << qrCode;
    }
    if (execStatus != -1) {
        parts.whereConditions << QStringLiteral("exec_status = ?");
        parts.bindValues << execStatus;
    }
    if (retryCount != -1) {
        parts.whereConditions << QStringLiteral("retry_count = ?");
        parts.bindValues << retryCount;
    }
    if (!startTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("send_time >= ?");
        parts.bindValues << startTime;
    }
    if (!endTime.isEmpty()) {
        parts.whereConditions << QStringLiteral("send_time <= ?");
        parts.bindValues << endTime;
    }

    return parts;
}

void bindValues(QSqlQuery& query, const QVariantList& values)
{
    for (const QVariant& value : values) {
        query.addBindValue(value);
    }
}

bool ensureCommunicateLogQueryIndexes(QSqlDatabase& database)
{
    static const QStringList indexStatements = {
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_communicate_log_time_id ON communicate_log(send_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_communicate_log_qr_time_id ON communicate_log(qr_code, send_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_communicate_log_command_time_id ON communicate_log(command_id, send_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_communicate_log_status_time_id ON communicate_log(exec_status, send_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_communicate_log_retry_time_id ON communicate_log(retry_count, send_time DESC, id DESC)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_communicate_log_qr_command_time_id ON communicate_log(qr_code, command_id, send_time DESC, id DESC)")
    };

    QSqlQuery query(database);
    for (const QString& statement : indexStatements) {
        if (!query.exec(statement)) {
            qWarning() << "Failed to create communicate_log query index:"
                       << query.lastError().text()
                       << statement;
            return false;
        }
    }
    return true;
}

} // namespace

CommunicateLogSqlLogic::CommunicateLogSqlLogic(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_connectionName("CommunicateLogSqlLogicConnection")
    , m_sqlMapper(nullptr)
    , m_cleanupScheduler(nullptr)
{
    QString sqlFilePath = QString("%1/communicate_log_queries.sql").arg(databasePath);
    m_sqlMapper = new SqlMapper(sqlFilePath);
}

CommunicateLogSqlLogic::~CommunicateLogSqlLogic()
{
    if (m_cleanupScheduler) {
        m_cleanupScheduler->stop();
        delete m_cleanupScheduler;
        m_cleanupScheduler = nullptr;
    }
    if (m_database.isOpen()) {
        m_database.close();
    }
    delete m_sqlMapper;
}

bool CommunicateLogSqlLogic::initializeDatabase()
{
    QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);
    m_database = DBConnectionHelper::openSqlite(dbFilePath, m_connectionName);
    if (!m_database.isOpen()) {
        return false;
    }
    if (!ensureCommunicateLogQueryIndexes(m_database)) {
        return false;
    }

    initializeCleanupScheduler();
    return true;
}

void CommunicateLogSqlLogic::initializeCleanupScheduler()
{
    LogCleanupScheduler::Config cfg;
    cfg.checkIntervalMs = 60000;
    cfg.retainMonths = 6;
    cfg.cleanupMonths = 1;
    cfg.databaseName = QStringLiteral("通信日志数据库");
    cfg.tableName = QStringLiteral("communicate_log");
    cfg.logPath = "log_db/database_cleanup/month_clean";

    m_cleanupScheduler = new LogCleanupScheduler(cfg, this);
    m_cleanupScheduler->setMonthRangeProvider([this]() {
        return queryMonthRange();
    });
    m_cleanupScheduler->setDeleteByRangeFn([this](const QString& startTime, const QString& endTime) {
        return deleteByTimeRange(startTime, endTime);
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
                                                                   int sortOrder,
                                                                   int maxUserPermission)
{
    QList<QVariantMap> results;
    if (!m_database.isOpen()) {
        return results;
    }

    const QString orderKeyword =
        (static_cast<SortOrder>(sortOrder) == SortOrder::Asc) ? QStringLiteral("ASC")
                                                              : QStringLiteral("DESC");
    const CommunicateLogQueryParts queryParts = buildCommunicateLogQueryParts(commandId,
                                                                              qrCode,
                                                                              execStatus,
                                                                              retryCount,
                                                                              startTime,
                                                                              endTime,
                                                                              maxUserPermission);
    const QString sql = QStringLiteral("SELECT * FROM communicate_log WHERE %1 "
                                       "ORDER BY send_time %2, id %2 LIMIT ? OFFSET ?")
                            .arg(queryParts.whereConditions.join(QStringLiteral(" AND ")),
                                 orderKeyword);

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

    return query.next() ? query.value(0).toInt() : 0;
}

int CommunicateLogSqlLogic::queryTotalCountWithConditions(const QString& commandId,
                                                          const QString& qrCode,
                                                          int execStatus,
                                                          int retryCount,
                                                          const QString& startTime,
                                                          const QString& endTime,
                                                          int maxUserPermission)
{
    if (!m_database.isOpen()) {
        return 0;
    }

    const CommunicateLogQueryParts queryParts = buildCommunicateLogQueryParts(commandId,
                                                                              qrCode,
                                                                              execStatus,
                                                                              retryCount,
                                                                              startTime,
                                                                              endTime,
                                                                              maxUserPermission);
    const QString sql = QStringLiteral("SELECT COUNT(*) AS total_count FROM communicate_log WHERE %1")
                            .arg(queryParts.whereConditions.join(QStringLiteral(" AND ")));

    QSqlQuery query(m_database);
    query.prepare(sql);
    bindValues(query, queryParts.bindValues);

    if (!query.exec()) {
        qWarning() << "Query failed:" << query.lastError().text();
        return 0;
    }

    return query.next() ? query.value(0).toInt() : 0;
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

int CommunicateLogSqlLogic::calculateOffset(int pageSize, int pageNumber)
{
    if (pageNumber <= 0) {
        return 0;
    }
    return pageSize * (pageNumber - 1);
}

} // namespace LogDB
