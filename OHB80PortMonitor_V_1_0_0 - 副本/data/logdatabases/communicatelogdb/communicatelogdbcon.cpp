#include "communicatelogdbcon.h"
#include <QDateTime>
#include <QDebug>

namespace LogDB {

CommunicateLogDBCon::CommunicateLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon, QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_sqlLogic(nullptr)
    , m_writeCon(externalWriteCon)
{
    Q_ASSERT_X(externalWriteCon != nullptr, "CommunicateLogDBCon",
               "WriteSqlDBCon must be provided");
    m_workerThread = new QThread(this);
    m_sqlLogic = new CommunicateLogSqlLogic(databasePath);

    connect(m_workerThread, &QThread::finished, m_sqlLogic, &QObject::deleteLater);
    m_sqlLogic->moveToThread(m_workerThread);

    connect(m_sqlLogic, &CommunicateLogSqlLogic::writeExecuted,
            this, [this](const WriteResult& result) {
                m_writeCon->addWriteTask(result);
            }, Qt::DirectConnection);

    connect(m_writeCon, &WriteSqlDBCon::taskCompleted,
            this, &CommunicateLogDBCon::onWriteTaskCompleted, Qt::QueuedConnection);

    m_workerThread->start();
}

CommunicateLogDBCon::~CommunicateLogDBCon()
{
    cleanup();
}

bool CommunicateLogDBCon::initialize()
{
    QMetaObject::invokeMethod(m_sqlLogic, "initializeDatabase",
                              Qt::BlockingQueuedConnection);

    // 外部传入的 WriteSqlDBCon 由调用方负责初始化与销毁
    return true;
}

void CommunicateLogDBCon::cleanup()
{
    // 不拥有 m_writeCon，仅由外部负责销毁
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    m_sqlLogic = nullptr;
}

void CommunicateLogDBCon::onWriteTaskCompleted(const WriteResult& result)
{
    qDebug() << "[CommunicateLog] Write task completed:" << result.connectionName
             << "Result:" << result.result
             << "SQL ID:" << result.sqlId
             << "Params:" << result.params;

    // WriteSqlDBCon 的 taskCompleted 会广播给所有 DBCon，此处按表名 + 操作类型过滤，
    // 避免把别的表的事件当成本表的实时事件
    if (result.tableName != QStringLiteral("communicate_log")) return;
    if (result.opType != static_cast<int>(WriteOp::Insert)) return;
    if (result.result != QStringLiteral("Success")) return;

    // params 顺序与 SQL ((send_time, response_time, command_id, qr_code,
    //                    exec_status, retry_count, send_frame, response_frame, description, user_permission)) 严格对齐
    if (result.params.size() < 10) return;

    CommunicateRecord record;
    record.sendTime        = result.params.at(0).toString();
    record.responseTime    = result.params.at(1).toString();
    record.commandId       = result.params.at(2).toString();
    record.qrCode          = result.params.at(3).toString();
    record.execStatus      = result.params.at(4).toInt();
    record.retryCount      = result.params.at(5).toInt();
    record.sendFrame       = result.params.at(6).toByteArray();
    record.responseFrame   = result.params.at(7).toByteArray();
    record.description     = result.params.at(8).toString();
    record.userPermission  = result.params.at(9).toInt();

    emit recordInserted(record);
}

QList<CommunicateRecord> CommunicateLogDBCon::queryPageWithConditions(const QString& commandId,
                                                                const QString& qrCode,
                                                                int execStatus,
                                                                int retryCount,
                                                                const QString& startTime,
                                                                const QString& endTime,
                                                                int pageSize,
                                                                int pageNumber,
                                                                SortOrder sortOrder)
{
    QList<QVariantMap> varResults;
    QMetaObject::invokeMethod(m_sqlLogic, "queryPageWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<QVariantMap>, varResults),
                              Q_ARG(QString, commandId),
                              Q_ARG(QString, qrCode),
                              Q_ARG(int, execStatus),
                              Q_ARG(int, retryCount),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, pageSize),
                              Q_ARG(int, pageNumber),
                              Q_ARG(int, static_cast<int>(sortOrder)));

    // 转换 QVariantMap 为 CommunicateRecord
    QList<CommunicateRecord> results;
    results.reserve(varResults.size());
    for (const QVariantMap& row : varResults) {
        CommunicateRecord rec;
        rec.id              = row.value(QStringLiteral("id")).toInt();
        rec.sendTime        = row.value(QStringLiteral("send_time")).toString();
        rec.responseTime    = row.value(QStringLiteral("response_time")).toString();
        rec.commandId       = row.value(QStringLiteral("command_id")).toString();
        rec.qrCode          = row.value(QStringLiteral("qr_code")).toString();
        rec.execStatus      = row.value(QStringLiteral("exec_status")).toInt();
        rec.retryCount      = row.value(QStringLiteral("retry_count")).toInt();
        rec.sendFrame       = row.value(QStringLiteral("send_frame")).toByteArray();
        rec.responseFrame   = row.value(QStringLiteral("response_frame")).toByteArray();
        rec.description     = row.value(QStringLiteral("description")).toString();
        rec.userPermission  = row.value(QStringLiteral("user_permission")).toInt();
        results.append(rec);
    }
    return results;
}

int CommunicateLogDBCon::queryTotalCount()
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCount",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count));
    return count;
}

int CommunicateLogDBCon::queryTotalCountWithConditions(const QString& commandId,
                                                       const QString& qrCode,
                                                       int execStatus,
                                                       int retryCount,
                                                       const QString& startTime,
                                                       const QString& endTime)
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCountWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(QString, commandId),
                              Q_ARG(QString, qrCode),
                              Q_ARG(int, execStatus),
                              Q_ARG(int, retryCount),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
    return count;
}

int CommunicateLogDBCon::queryMonthRange()
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

void CommunicateLogDBCon::queryTimeBounds(QString& earliestTime, QString& latestTime)
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

void CommunicateLogDBCon::insertRecord(const QString& sendTime,
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
    QMetaObject::invokeMethod(m_sqlLogic, "insertRecord",
                              Qt::QueuedConnection,
                              Q_ARG(QString, sendTime),
                              Q_ARG(QString, responseTime),
                              Q_ARG(QString, commandId),
                              Q_ARG(QString, qrCode),
                              Q_ARG(int, execStatus),
                              Q_ARG(int, retryCount),
                              Q_ARG(QByteArray, sendFrame),
                              Q_ARG(QByteArray, responseFrame),
                              Q_ARG(QString, description),
                              Q_ARG(int, userPermission));
}

void CommunicateLogDBCon::deleteByTimeRange(const QString& startTime, const QString& endTime)
{
    QMetaObject::invokeMethod(m_sqlLogic, "deleteByTimeRange",
                              Qt::QueuedConnection,
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
}

} // namespace LogDB
