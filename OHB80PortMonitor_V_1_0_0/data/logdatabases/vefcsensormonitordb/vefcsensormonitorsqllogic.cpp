#include "vefcsensormonitorsqllogic.h"
#include "dbconnectionhelper.h"
#include "logcleanupscheduler.h"
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QStringList>
#include <QDebug>

namespace LogDB {

namespace {

constexpr qint64 kOneSecondMs = 1000;

}

VEFCSensorMonitorSqlLogic::VEFCSensorMonitorSqlLogic(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_connectionName("VEFCSensorMonitorSqlLogicConnection")
    , m_sqlMapper(nullptr)
    , m_cleanupScheduler(nullptr)
{
    const QString sqlFilePath = QString("%1/vefc_sensor_monitor_queries.sql").arg(databasePath);
    m_sqlMapper = new SqlMapper(sqlFilePath);
}

VEFCSensorMonitorSqlLogic::~VEFCSensorMonitorSqlLogic()
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

bool VEFCSensorMonitorSqlLogic::initializeDatabase()
{
    const QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);
    m_database = DBConnectionHelper::openSqlite(dbFilePath, m_connectionName);
    if (!m_database.isOpen()) {
        return false;
    }

    initializeCleanupScheduler();
    return true;
}

bool VEFCSensorMonitorSqlLogic::insertRecord(const QString& qrCode,
                                             qint64 recordTimestamp,
                                             double gasPressure,
                                             double actualFlow,
                                             double sensorPressure,
                                             double sensorTemperature)
{
    const QString sql = m_sqlMapper->getSql("insert_record");
    if (sql.isEmpty()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] SQL not found: insert_record";
        return false;
    }

    QVariantList params;
    params << qrCode
           << recordTimestamp
           << gasPressure
           << actualFlow
           << sensorPressure
           << sensorTemperature;

    emit writeExecuted(makeWriteResult(QStringLiteral("insert_record"), sql, params));
    return true;
}

bool VEFCSensorMonitorSqlLogic::insertRecords(const QVector<VEFCSensorMonitorRecord>& records)
{
    if (records.isEmpty()) {
        return true;
    }

    QStringList valueGroups;
    QVariantList params;
    valueGroups.reserve(records.size());
    params.reserve(records.size() * 6);

    for (const VEFCSensorMonitorRecord& record : records) {
        valueGroups << QStringLiteral("(?, ?, ?, ?, ?, ?)");
        params << record.qrCode
               << record.recordTimestamp
               << record.gasPressure
               << record.actualFlow
               << record.sensorPressure
               << record.sensorTemperature;
    }

    const QString sql = QStringLiteral(
        "INSERT INTO vefc_sensor_monitor "
        "(qr_code, record_timestamp, gas_pressure, actual_flow, sensor_pressure, sensor_temperature) "
        "VALUES %1")
        .arg(valueGroups.join(QStringLiteral(", ")));

    emit writeExecuted(makeWriteResult(QStringLiteral("insert_records"), sql, params));
    return true;
}

bool VEFCSensorMonitorSqlLogic::deleteByTimeRange(qint64 startTimestamp, qint64 endTimestamp)
{
    const QString sql = m_sqlMapper->getSql("delete_by_time_range");
    if (sql.isEmpty()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] SQL not found: delete_by_time_range";
        return false;
    }

    QVariantList params;
    params << startTimestamp
           << endTimestamp;

    emit writeExecuted(makeWriteResult(QStringLiteral("delete_by_time_range"),
                                       sql,
                                       params,
                                       WriteOp::Delete));
    return true;
}

QVariantMap VEFCSensorMonitorSqlLogic::queryDailyAverage(qint64 dayStartTimestamp, qint64 nextDayStartTimestamp)
{
    QVariantMap result;
    if (!m_database.isOpen()) {
        return result;
    }

    const QString sql = m_sqlMapper->getSql("query_daily_average");
    if (sql.isEmpty()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] SQL not found: query_daily_average";
        return result;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(dayStartTimestamp);
    query.addBindValue(nextDayStartTimestamp);

    if (!query.exec()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] queryDailyAverage failed:"
                   << query.lastError().text();
        return result;
    }

    if (query.next()) {
        const QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            result[record.fieldName(i)] = record.value(i);
        }
    }

    return result;
}

QVector<VEFCSensorMonitorRecord> VEFCSensorMonitorSqlLogic::queryRecordsByTimeRange(qint64 startTimestamp,
                                                                                     qint64 endTimestamp)
{
    QVector<VEFCSensorMonitorRecord> records;
    if (!m_database.isOpen()) {
        return records;
    }

    const QString sql = m_sqlMapper->getSql("query_records_by_time_range");
    if (sql.isEmpty()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] SQL not found: query_records_by_time_range";
        return records;
    }

    QSqlQuery query(m_database);
    query.prepare(sql);
    query.addBindValue(startTimestamp);
    query.addBindValue(endTimestamp);

    if (!query.exec()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] queryRecordsByTimeRange failed:"
                   << query.lastError().text();
        return records;
    }

    while (query.next()) {
        VEFCSensorMonitorRecord record;
        record.qrCode = query.value(0).toString();
        record.recordTimestamp = query.value(1).toLongLong();
        record.gasPressure = query.value(2).toDouble();
        record.actualFlow = query.value(3).toDouble();
        record.sensorPressure = query.value(4).toDouble();
        record.sensorTemperature = query.value(5).toDouble();
        records.append(record);
    }

    return records;
}

QVector<VEFCSensorMonitorRecord> VEFCSensorMonitorSqlLogic::queryOldestWeekRecords()
{
    QVector<VEFCSensorMonitorRecord> records;
    if (!m_database.isOpen()) {
        return records;
    }

    const QString sql = m_sqlMapper->getSql("query_oldest_week_records");
    if (sql.isEmpty()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] SQL not found: query_oldest_week_records";
        return records;
    }

    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] queryOldestWeekRecords failed:"
                   << query.lastError().text();
        return records;
    }

    while (query.next()) {
        VEFCSensorMonitorRecord record;
        record.qrCode = query.value(0).toString();
        record.recordTimestamp = query.value(1).toLongLong();
        record.gasPressure = query.value(2).toDouble();
        record.actualFlow = query.value(3).toDouble();
        record.sensorPressure = query.value(4).toDouble();
        record.sensorTemperature = query.value(5).toDouble();
        records.append(record);
    }

    return records;
}

QVariantMap VEFCSensorMonitorSqlLogic::queryMonthRange()
{
    QVariantMap result;
    if (!m_database.isOpen()) {
        return result;
    }

    const QString sql = m_sqlMapper->getSql("query_month_range");
    if (sql.isEmpty()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] SQL not found: query_month_range";
        return result;
    }

    QSqlQuery query(m_database);
    if (!query.exec(sql)) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] queryMonthRange failed:"
                   << query.lastError().text();
        return result;
    }

    if (query.next()) {
        result["earliest_time"] = query.value("earliest_time");
        result["latest_time"] = query.value("latest_time");
        result["earliest_date"] = query.value("earliest_date");
    }

    return result;
}

void VEFCSensorMonitorSqlLogic::initializeCleanupScheduler()
{
    LogCleanupScheduler::Config cleanupConfig;
    cleanupConfig.checkIntervalMs = 60000;
    cleanupConfig.retainMonths = 1;
    cleanupConfig.cleanupMonths = 1;
    cleanupConfig.logPath = "log_db/vefc_sensor_monitor_db/month_clean";

    m_cleanupScheduler = new LogCleanupScheduler(cleanupConfig, this);
    m_cleanupScheduler->setMonthRangeProvider([this]() {
        return queryMonthRange();
    });
    m_cleanupScheduler->setDeleteByRangeFn([this](const QString& startTime, const QString& endTime) {
        deleteByDateTimeRange(startTime, endTime);
    });
    m_cleanupScheduler->start();
}

bool VEFCSensorMonitorSqlLogic::deleteByDateTimeRange(const QString& startTime, const QString& endTime)
{
    const QDateTime startDateTime = QDateTime::fromString(startTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    const QDateTime endDateTime = QDateTime::fromString(endTime, QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    if (!startDateTime.isValid() || !endDateTime.isValid()) {
        qWarning() << "[VEFCSensorMonitorSqlLogic] Invalid cleanup range:"
                   << "startTime=" << startTime
                   << "endTime=" << endTime;
        return false;
    }

    const qint64 startTimestamp = startDateTime.toMSecsSinceEpoch();
    const qint64 endTimestamp = endDateTime.toMSecsSinceEpoch() + kOneSecondMs;
    return deleteByTimeRange(startTimestamp, endTimestamp);
}

WriteResult VEFCSensorMonitorSqlLogic::makeWriteResult(const QString& sqlId,
                                                       const QString& sql,
                                                       const QVariantList& params,
                                                       WriteOp opType) const
{
    WriteResult result;
    result.connectionName = m_connectionName;
    result.sqlStatement = sql;
    result.sqlId = sqlId;
    result.params = params;
    result.tableName = QStringLiteral("vefc_sensor_monitor");
    result.opType = static_cast<int>(opType);
    return result;
}

} // namespace LogDB
