#include "deviceparamlogdbcon.h"
#include <QDateTime>
#include <QDebug>

namespace LogDB {

DeviceParamLogDBCon::DeviceParamLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon, QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_sqlLogic(nullptr)
    , m_writeCon(externalWriteCon)
{
    Q_ASSERT_X(externalWriteCon != nullptr, "DeviceParamLogDBCon",
               "WriteSqlDBCon must be provided");
    m_workerThread = new QThread(this);
    m_sqlLogic = new DeviceParamLogSqlLogic(databasePath);

    connect(m_workerThread, &QThread::finished, m_sqlLogic, &QObject::deleteLater);
    m_sqlLogic->moveToThread(m_workerThread);

    connect(m_sqlLogic, &DeviceParamLogSqlLogic::writeExecuted,
            this, [this](const WriteResult& result) {
                m_writeCon->addWriteTask(result);
            }, Qt::DirectConnection);

    connect(m_writeCon, &WriteSqlDBCon::taskCompleted,
            this, &DeviceParamLogDBCon::onWriteTaskCompleted, Qt::QueuedConnection);

    m_workerThread->start();
}

DeviceParamLogDBCon::~DeviceParamLogDBCon()
{
    cleanup();
}

bool DeviceParamLogDBCon::initialize()
{
    QMetaObject::invokeMethod(m_sqlLogic, "initializeDatabase",
                              Qt::BlockingQueuedConnection);
    return true;
}

void DeviceParamLogDBCon::cleanup()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    m_sqlLogic = nullptr;
}

void DeviceParamLogDBCon::onWriteTaskCompleted(const WriteResult& result)
{
    qDebug() << "[DeviceParamLog] Write task completed:" << result.connectionName
             << "Result:" << result.result
             << "SQL ID:" << result.sqlId
             << "Params:" << result.params;

    // 仅派发本表的成功 INSERT
    if (result.tableName != QStringLiteral("device_param_log")) return;
    if (result.opType != static_cast<int>(WriteOp::Insert)) return;
    if (result.result != QStringLiteral("Success")) return;

    // params 顺序与 SQL ((qr_code, record_time, inlet_pressure, outlet_pressure,
    //                    inlet_flow, humidity, temperature, foup_status, user_permission)) 严格对齐
    if (result.params.size() < 9) return;

    DeviceParamRecord record;
    record.qrCode          = result.params.at(0).toString();
    record.recordTime      = result.params.at(1).toString();
    record.inletPressure   = result.params.at(2).toDouble();
    record.outletPressure  = result.params.at(3).toDouble();
    record.inletFlow       = result.params.at(4).toDouble();
    record.humidity        = result.params.at(5).toDouble();
    record.temperature     = result.params.at(6).toDouble();
    record.foupStatus      = result.params.at(7).toInt();
    record.userPermission  = result.params.at(8).toInt();

    emit recordInserted(record);
}

QList<DeviceParamRecord> DeviceParamLogDBCon::queryPageWithConditions(const QString& qrCode,
                                                                const QString& startTime,
                                                                const QString& endTime,
                                                                int pageSize,
                                                                int pageNumber)
{
    QList<QVariantMap> varResults;
    QMetaObject::invokeMethod(m_sqlLogic, "queryPageWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<QVariantMap>, varResults),
                              Q_ARG(QString, qrCode),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, pageSize),
                              Q_ARG(int, pageNumber));

    // 转换 QVariantMap 为 DeviceParamRecord
    QList<DeviceParamRecord> results;
    results.reserve(varResults.size());
    for (const QVariantMap& row : varResults) {
        DeviceParamRecord rec;
        rec.id              = row.value(QStringLiteral("id")).toInt();
        rec.qrCode          = row.value(QStringLiteral("qr_code")).toString();
        rec.recordTime      = row.value(QStringLiteral("record_time")).toString();
        rec.inletPressure   = row.value(QStringLiteral("inlet_pressure")).toDouble();
        rec.outletPressure  = row.value(QStringLiteral("outlet_pressure")).toDouble();
        rec.inletFlow       = row.value(QStringLiteral("inlet_flow")).toDouble();
        rec.humidity        = row.value(QStringLiteral("humidity")).toDouble();
        rec.temperature     = row.value(QStringLiteral("temperature")).toDouble();
        rec.foupStatus      = row.value(QStringLiteral("foup_status")).toInt();
        rec.userPermission  = row.value(QStringLiteral("user_permission")).toInt();
        results.append(rec);
    }
    return results;
}

int DeviceParamLogDBCon::queryTotalCount()
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCount",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count));
    return count;
}

int DeviceParamLogDBCon::queryTotalCountWithConditions(const QString& qrCode,
                                                       const QString& startTime,
                                                       const QString& endTime)
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCountWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(QString, qrCode),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
    return count;
}

int DeviceParamLogDBCon::queryMonthRange()
{
    QVariantMap result;
    bool success = QMetaObject::invokeMethod(m_sqlLogic, "queryMonthRange",
                                             Qt::BlockingQueuedConnection,
                                             Q_RETURN_ARG(QVariantMap, result));
    if (!success) {
        return -1;
    }

    QString earliestTime = result["earliest_time"].toString();
    QString latestTime = result["latest_time"].toString();

    if (earliestTime.isEmpty() || latestTime.isEmpty() || earliestTime == "NULL" || latestTime == "NULL") {
        return 0;
    }

    QDateTime earliest = QDateTime::fromString(earliestTime, "yyyy-MM-dd HH:mm:ss");
    QDateTime latest = QDateTime::fromString(latestTime, "yyyy-MM-dd HH:mm:ss");

    if (!earliest.isValid() || !latest.isValid()) {
        return 0;
    }

    int yearDiff = latest.date().year() - earliest.date().year();
    int monthDiff = latest.date().month() - earliest.date().month();
    return yearDiff * 12 + monthDiff;
}

void DeviceParamLogDBCon::insertRecord(const QString& qrCode,
                                       const QString& recordTime,
                                       double inletPressure,
                                       double outletPressure,
                                       double inletFlow,
                                       double humidity,
                                       double temperature,
                                       int foupStatus,
                                       int userPermission)
{
    QMetaObject::invokeMethod(m_sqlLogic, "insertRecord",
                              Qt::QueuedConnection,
                              Q_ARG(QString, qrCode),
                              Q_ARG(QString, recordTime),
                              Q_ARG(double, inletPressure),
                              Q_ARG(double, outletPressure),
                              Q_ARG(double, inletFlow),
                              Q_ARG(double, humidity),
                              Q_ARG(double, temperature),
                              Q_ARG(int, foupStatus),
                              Q_ARG(int, userPermission));
}

void DeviceParamLogDBCon::deleteByTimeRange(const QString& startTime, const QString& endTime)
{
    QMetaObject::invokeMethod(m_sqlLogic, "deleteByTimeRange",
                              Qt::QueuedConnection,
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
}

} // namespace LogDB
