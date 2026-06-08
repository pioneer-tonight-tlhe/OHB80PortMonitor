#include "alarmlogdbcon.h"
#include <QDateTime>
#include <QDebug>

namespace LogDB {

AlarmLogDBCon::AlarmLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon, QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_sqlLogic(nullptr)
    , m_writeCon(externalWriteCon)
{
    Q_ASSERT_X(externalWriteCon != nullptr, "AlarmLogDBCon",
               "WriteSqlDBCon must be provided");
    m_workerThread = new QThread(this);
    m_sqlLogic = new AlarmLogSqlLogic(databasePath);

    connect(m_workerThread, &QThread::finished, m_sqlLogic, &QObject::deleteLater);
    m_sqlLogic->moveToThread(m_workerThread);

    connect(m_sqlLogic, &AlarmLogSqlLogic::writeExecuted,
            this, [this](const WriteResult& result) {
                m_writeCon->addWriteTask(result);
            }, Qt::DirectConnection);

    connect(m_writeCon, &WriteSqlDBCon::taskCompleted,
            this, &AlarmLogDBCon::onWriteTaskCompleted, Qt::QueuedConnection);

    m_workerThread->start();
}

AlarmLogDBCon::~AlarmLogDBCon()
{
    cleanup();
}

bool AlarmLogDBCon::initialize()
{
    QMetaObject::invokeMethod(m_sqlLogic, "initializeDatabase",
                              Qt::BlockingQueuedConnection);
    return true;
}

void AlarmLogDBCon::cleanup()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    m_sqlLogic = nullptr;
}

void AlarmLogDBCon::onWriteTaskCompleted(const WriteResult& result)
{
    qDebug() << "[AlarmLog] Write task completed:" << result.connectionName
             << "Result:" << result.result
             << "SQL ID:" << result.sqlId
             << "Params:" << result.params;

    // 仅处理本表的成功结果
    if (result.tableName != QStringLiteral("alarm_log")) return;
    if (result.result != QStringLiteral("Success")) return;

    // ---- INSERT 派发 ----
    if (result.opType == static_cast<int>(WriteOp::Insert)) {
        // params 顺序与 SQL ((alarm_level, occur_time, qr_code, alarm_type,
        //                    is_resolved, resolve_time, description, user_permission)) 严格对齐
        if (result.params.size() < 8) return;

        AlarmRecord record;
        record.alarmLevel      = result.params.at(0).toInt();
        record.occurTime       = result.params.at(1).toString();
        record.qrCode          = result.params.at(2).toString();
        record.alarmType       = result.params.at(3).toInt();  // alarm_type 列为 TEXT，但存储为数字字符串
        record.isResolved      = result.params.at(4).toInt();
        record.resolveTime     = result.params.at(5).toString();
        record.description     = result.params.at(6).toString();
        record.userPermission  = result.params.at(7).toInt();

        emit recordInserted(record);
        return;
    }

    // ---- UPDATE(update_resolve) 派发 ----
    if (result.sqlId == QStringLiteral("update_resolve")) {
        // params 顺序：resolve_time, qr_code, alarm_type
        if (result.params.size() < 3) return;
        const QString resolveTime = result.params.at(0).toString();
        const QString qrCode      = result.params.at(1).toString();
        const QString alarmType   = result.params.at(2).toString();
        emit recordResolved(qrCode, alarmType, resolveTime);
        return;
    }
}

QList<AlarmRecord> AlarmLogDBCon::queryPageWithConditions(int alarmLevel,
                                                          const QString& qrCode,
                                                          const QString& alarmType,
                                                          int isResolved,
                                                          const QString& startTime,
                                                          const QString& endTime,
                                                          int pageSize,
                                                          int pageNumber)
{
    QList<QVariantMap> varResults;
    QMetaObject::invokeMethod(m_sqlLogic, "queryPageWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<QVariantMap>, varResults),
                              Q_ARG(int, alarmLevel),
                              Q_ARG(QString, qrCode),
                              Q_ARG(QString, alarmType),
                              Q_ARG(int, isResolved),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, pageSize),
                              Q_ARG(int, pageNumber));

    // 转换 QVariantMap 为 AlarmRecord
    QList<AlarmRecord> results;
    results.reserve(varResults.size());
    for (const QVariantMap& row : varResults) {
        AlarmRecord rec;
        rec.id              = row.value(QStringLiteral("id")).toInt();
        rec.alarmLevel      = row.value(QStringLiteral("alarm_level")).toInt();
        rec.occurTime       = row.value(QStringLiteral("occur_time")).toString();
        rec.qrCode          = row.value(QStringLiteral("qr_code")).toString();
        rec.alarmType       = row.value(QStringLiteral("alarm_type")).toInt();
        rec.isResolved      = row.value(QStringLiteral("is_resolved")).toInt();
        rec.resolveTime     = row.value(QStringLiteral("resolve_time")).toString();
        rec.description     = row.value(QStringLiteral("description")).toString();
        rec.userPermission  = row.value(QStringLiteral("user_permission")).toInt();
        results.append(rec);
    }
    return results;
}

int AlarmLogDBCon::queryTotalCount()
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCount",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count));
    return count;
}

int AlarmLogDBCon::queryTotalCountWithConditions(int alarmLevel,
                                                 const QString& qrCode,
                                                 const QString& alarmType,
                                                 int isResolved,
                                                 const QString& startTime,
                                                 const QString& endTime)
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCountWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(int, alarmLevel),
                              Q_ARG(QString, qrCode),
                              Q_ARG(QString, alarmType),
                              Q_ARG(int, isResolved),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
    return count;
}

int AlarmLogDBCon::queryMonthRange()
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

void AlarmLogDBCon::queryTimeBounds(QString& earliestTime, QString& latestTime)
{
    earliestTime.clear();
    latestTime.clear();

    QVariantMap result;
    bool success = QMetaObject::invokeMethod(m_sqlLogic, "queryMonthRange",
                                             Qt::BlockingQueuedConnection,
                                             Q_RETURN_ARG(QVariantMap, result));
    if (!success) {
        return;
    }

    const QString earliest = result.value("earliest_time").toString();
    const QString latest   = result.value("latest_time").toString();
    if (earliest.isEmpty() || latest.isEmpty()
        || earliest == "NULL" || latest == "NULL") {
        return;
    }
    earliestTime = earliest;
    latestTime   = latest;
}

void AlarmLogDBCon::insertRecord(int alarmLevel,
                                 const QString& occurTime,
                                 const QString& qrCode,
                                 const QString& alarmType,
                                 int isResolved,
                                 const QString& resolveTime,
                                 const QString& description,
                                 int userPermission)
{
    QMetaObject::invokeMethod(m_sqlLogic, "insertRecord",
                              Qt::QueuedConnection,
                              Q_ARG(int, alarmLevel),
                              Q_ARG(QString, occurTime),
                              Q_ARG(QString, qrCode),
                              Q_ARG(QString, alarmType),
                              Q_ARG(int, isResolved),
                              Q_ARG(QString, resolveTime),
                              Q_ARG(QString, description),
                              Q_ARG(int, userPermission));
}

void AlarmLogDBCon::deleteByTimeRange(const QString& startTime, const QString& endTime)
{
    QMetaObject::invokeMethod(m_sqlLogic, "deleteByTimeRange",
                              Qt::QueuedConnection,
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
}

void AlarmLogDBCon::updateResolve(const QString& qrCode, const QString& alarmType, const QString& resolveTime)
{
    QMetaObject::invokeMethod(m_sqlLogic, "updateResolve",
                              Qt::QueuedConnection,
                              Q_ARG(QString, qrCode),
                              Q_ARG(QString, alarmType),
                              Q_ARG(QString, resolveTime));
}

} // namespace LogDB
