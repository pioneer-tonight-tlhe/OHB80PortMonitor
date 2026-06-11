#include "vefcsensormonitordbcon.h"
#include <QDebug>

namespace LogDB {

VEFCSensorMonitorDBCon::VEFCSensorMonitorDBCon(const QString& databasePath,
                                               WriteSqlDBCon* externalWriteCon,
                                               QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_sqlLogic(nullptr)
    , m_writeCon(externalWriteCon)
{
    Q_ASSERT_X(externalWriteCon != nullptr, "VEFCSensorMonitorDBCon",
               "WriteSqlDBCon must be provided");

    m_workerThread = new QThread(this);
    m_sqlLogic = new VEFCSensorMonitorSqlLogic(databasePath);

    connect(m_workerThread, &QThread::finished, m_sqlLogic, &QObject::deleteLater);
    m_sqlLogic->moveToThread(m_workerThread);

    connect(m_sqlLogic, &VEFCSensorMonitorSqlLogic::writeExecuted,
            this, [this](const WriteResult& result) {
                m_writeCon->addWriteTask(result);
            }, Qt::DirectConnection);

    connect(m_writeCon, &WriteSqlDBCon::taskCompleted,
            this, &VEFCSensorMonitorDBCon::onWriteTaskCompleted, Qt::QueuedConnection);

    m_workerThread->start();
}

VEFCSensorMonitorDBCon::~VEFCSensorMonitorDBCon()
{
    cleanup();
}

bool VEFCSensorMonitorDBCon::initialize()
{
    bool initialized = false;
    QMetaObject::invokeMethod(m_sqlLogic, "initializeDatabase",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, initialized));
    return initialized;
}

void VEFCSensorMonitorDBCon::cleanup()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    m_sqlLogic = nullptr;
}

void VEFCSensorMonitorDBCon::insertRecord(const QString& qrCode,
                                          qint64 recordTimestamp,
                                          double gasPressure,
                                          double actualFlow,
                                          double sensorPressure,
                                          double sensorTemperature)
{
    QMetaObject::invokeMethod(m_sqlLogic, "insertRecord",
                              Qt::QueuedConnection,
                              Q_ARG(QString, qrCode),
                              Q_ARG(qint64, recordTimestamp),
                              Q_ARG(double, gasPressure),
                              Q_ARG(double, actualFlow),
                              Q_ARG(double, sensorPressure),
                              Q_ARG(double, sensorTemperature));
}

void VEFCSensorMonitorDBCon::insertRecord(const VEFCSensorMonitorRecord& record)
{
    insertRecord(record.qrCode,
                 record.recordTimestamp,
                 record.gasPressure,
                 record.actualFlow,
                 record.sensorPressure,
                 record.sensorTemperature);
}

void VEFCSensorMonitorDBCon::insertRecords(const QVector<VEFCSensorMonitorRecord>& records)
{
    QMetaObject::invokeMethod(m_sqlLogic, "insertRecords",
                              Qt::QueuedConnection,
                              Q_ARG(QVector<VEFCSensorMonitorRecord>, records));
}

void VEFCSensorMonitorDBCon::deleteByTimeRange(qint64 startTimestamp, qint64 endTimestamp)
{
    QMetaObject::invokeMethod(m_sqlLogic, "deleteByTimeRange",
                              Qt::QueuedConnection,
                              Q_ARG(qint64, startTimestamp),
                              Q_ARG(qint64, endTimestamp));
}

QVariantMap VEFCSensorMonitorDBCon::queryDailyAverage(qint64 dayStartTimestamp, qint64 nextDayStartTimestamp)
{
    QVariantMap result;
    QMetaObject::invokeMethod(m_sqlLogic, "queryDailyAverage",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVariantMap, result),
                              Q_ARG(qint64, dayStartTimestamp),
                              Q_ARG(qint64, nextDayStartTimestamp));
    return result;
}

QVector<VEFCSensorMonitorRecord> VEFCSensorMonitorDBCon::queryRecordsByTimeRange(qint64 startTimestamp,
                                                                                 qint64 endTimestamp)
{
    QVector<VEFCSensorMonitorRecord> records;
    QMetaObject::invokeMethod(m_sqlLogic, "queryRecordsByTimeRange",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVector<VEFCSensorMonitorRecord>, records),
                              Q_ARG(qint64, startTimestamp),
                              Q_ARG(qint64, endTimestamp));
    return records;
}

QVector<VEFCSensorMonitorRecord> VEFCSensorMonitorDBCon::queryOldestWeekRecords()
{
    QVector<VEFCSensorMonitorRecord> records;
    QMetaObject::invokeMethod(m_sqlLogic, "queryOldestWeekRecords",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QVector<VEFCSensorMonitorRecord>, records));
    return records;
}

void VEFCSensorMonitorDBCon::onWriteTaskCompleted(const WriteResult& result)
{
    if (result.tableName != QStringLiteral("vefc_sensor_monitor")) {
        return;
    }
    if (result.result != QStringLiteral("Success")) {
        qWarning() << "[VEFCSensorMonitorDBCon] Write task failed:"
                   << "sqlId=" << result.sqlId
                   << "result=" << result.result;
        return;
    }

    if (result.sqlId == QStringLiteral("insert_record")) {
        if (result.params.size() < 6) {
            return;
        }
        emit recordInserted(recordFromParams(result.params, 0));
        return;
    }

    if (result.sqlId == QStringLiteral("insert_records")) {
        const QVector<VEFCSensorMonitorRecord> records = recordsFromParams(result.params);
        if (!records.isEmpty()) {
            emit recordsInserted(records);
        }
    }
}

VEFCSensorMonitorRecord VEFCSensorMonitorDBCon::recordFromParams(const QVariantList& params, int offset)
{
    VEFCSensorMonitorRecord record;
    if (params.size() < offset + 6) {
        return record;
    }

    record.qrCode = params.at(offset).toString();
    record.recordTimestamp = params.at(offset + 1).toLongLong();
    record.gasPressure = params.at(offset + 2).toDouble();
    record.actualFlow = params.at(offset + 3).toDouble();
    record.sensorPressure = params.at(offset + 4).toDouble();
    record.sensorTemperature = params.at(offset + 5).toDouble();
    return record;
}

QVector<VEFCSensorMonitorRecord> VEFCSensorMonitorDBCon::recordsFromParams(const QVariantList& params)
{
    QVector<VEFCSensorMonitorRecord> records;
    if (params.size() < 6) {
        return records;
    }

    records.reserve(params.size() / 6);
    for (int offset = 0; offset + 5 < params.size(); offset += 6) {
        records.append(recordFromParams(params, offset));
    }
    return records;
}

} // namespace LogDB
