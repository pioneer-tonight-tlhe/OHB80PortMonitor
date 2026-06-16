#include "communicatelogsqllogic.h"

#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

namespace LogDB {

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

    QString sql = m_sqlMapper->getSql("query_page_with_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_page_with_conditions";
        return results;
    }

    const QString orderKeyword =
        (static_cast<SortOrder>(sortOrder) == SortOrder::Asc) ? QStringLiteral("ASC")
                                                              : QStringLiteral("DESC");
    sql.replace(QStringLiteral("{ORDER}"), orderKeyword);

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(maxUserPermission);
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
    query.addBindValue(endTime.isEmpty() ? QVariant() : endTime);
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

    QString sql = m_sqlMapper->getSql("query_total_count_with_conditions");
    if (sql.isEmpty()) {
        qWarning() << "SQL not found: query_total_count_with_conditions";
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(maxUserPermission);
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
    query.addBindValue(endTime.isEmpty() ? QVariant() : endTime);

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
