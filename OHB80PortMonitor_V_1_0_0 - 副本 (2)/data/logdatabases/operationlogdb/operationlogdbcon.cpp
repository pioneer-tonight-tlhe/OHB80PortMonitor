#include "operationlogdbcon.h"
#include <QDebug>

namespace LogDB {

OperationLogDBCon::OperationLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon, QObject* parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_sqlLogic(nullptr)
    , m_writeCon(externalWriteCon)
{
    Q_ASSERT_X(externalWriteCon != nullptr, "OperationLogDBCon",
               "WriteSqlDBCon must be provided");
    m_workerThread = new QThread(this);
    m_sqlLogic = new OperationLogSqlLogic(databasePath);

    // 线程结束时由工作线程自行销毁（计时器在正确线程中停止）
    connect(m_workerThread, &QThread::finished, m_sqlLogic, &QObject::deleteLater);

    // 将OperationLogSqlLogic移动到工作线程
    m_sqlLogic->moveToThread(m_workerThread);

    // 连接OperationLogSqlLogic的writeExecuted信号到WriteSqlDBCon
    connect(m_sqlLogic, &OperationLogSqlLogic::writeExecuted,
            this, [this](const WriteResult& result) {
                m_writeCon->addWriteTask(result);
            }, Qt::DirectConnection);

    // 连接WriteSqlDBCon的任务完成信号
    connect(m_writeCon, &WriteSqlDBCon::taskCompleted,
            this, &OperationLogDBCon::onWriteTaskCompleted, Qt::QueuedConnection);

    // 启动线程
    m_workerThread->start();
}

OperationLogDBCon::~OperationLogDBCon()
{
    cleanup();
}

bool OperationLogDBCon::initialize()
{
    // 在工作线程中初始化数据库（计时器也在此线程中启动）
    QMetaObject::invokeMethod(m_sqlLogic, "initializeDatabase",
                              Qt::BlockingQueuedConnection);

    // 外部传入的 WriteSqlDBCon 由调用方负责初始化与销毁
    return true;
}

void OperationLogDBCon::cleanup()
{
    // 不拥有 m_writeCon，仅由外部负责销毁
    if (m_workerThread && m_workerThread->isRunning()) {
        // 退出线程；finished 信号触发 m_sqlLogic->deleteLater，
        // 析构在工作线程内完成，计时器在正确线程中停止
        m_workerThread->quit();
        m_workerThread->wait();
    }
    // wait() 返回后 deleteLater 已处理，m_sqlLogic 已被销毁
    m_sqlLogic = nullptr;
}

void OperationLogDBCon::onWriteTaskCompleted(const WriteResult& result)
{
    // 处理写入任务完成的结果
    qDebug() << "Write task completed:" << result.connectionName
             << "Result:" << result.result
             << "SQL:" << result.sqlStatement
             << "SQL ID:" << result.sqlId
             << "Params:" << result.params;

    // 仅派发本表的成功 INSERT
    if (result.tableName != QStringLiteral("operation_log")) return;
    if (result.opType != static_cast<int>(WriteOp::Insert)) return;
    if (result.result != QStringLiteral("Success")) return;

    // params 顺序与 SQL ((occur_time, log_type, description, user_permission)) 严格对齐
    if (result.params.size() < 4) return;

    OperationRecord record;
    record.occurTime       = result.params.at(0).toString();
    record.logType         = result.params.at(1).toInt();
    record.description     = result.params.at(2).toString();
    record.userPermission  = result.params.at(3).toInt();

    emit recordInserted(record);
}

QList<OperationRecord> OperationLogDBCon::queryPagination(int pageSize, int pageNumber)
{
    QList<QVariantMap> varResults;
    QMetaObject::invokeMethod(m_sqlLogic, "queryPagination",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<QVariantMap>, varResults),
                              Q_ARG(int, pageSize),
                              Q_ARG(int, pageNumber));

    // 转换 QVariantMap 为 OperationRecord
    QList<OperationRecord> results;
    results.reserve(varResults.size());
    for (const QVariantMap& row : varResults) {
        OperationRecord rec;
        rec.id              = row.value(QStringLiteral("id")).toInt();
        rec.occurTime       = row.value(QStringLiteral("occur_time")).toString();
        rec.logType         = row.value(QStringLiteral("log_type")).toInt();
        rec.description     = row.value(QStringLiteral("description")).toString();
        rec.userPermission  = row.value(QStringLiteral("user_permission")).toInt();
        results.append(rec);
    }
    return results;
}

int OperationLogDBCon::queryTotalCount()
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCount",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count));
    return count;
}

QList<OperationRecord> OperationLogDBCon::queryPaginationInRange(const QString& startTime, const QString& endTime,
                                                              int pageSize, int pageNumber)
{
    QList<QVariantMap> varResults;
    QMetaObject::invokeMethod(m_sqlLogic, "queryPaginationInRange",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<QVariantMap>, varResults),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, pageSize),
                              Q_ARG(int, pageNumber));

    QList<OperationRecord> results;
    results.reserve(varResults.size());
    for (const QVariantMap& row : varResults) {
        OperationRecord rec;
        rec.id              = row.value(QStringLiteral("id")).toInt();
        rec.occurTime       = row.value(QStringLiteral("occur_time")).toString();
        rec.logType         = row.value(QStringLiteral("log_type")).toInt();
        rec.description     = row.value(QStringLiteral("description")).toString();
        rec.userPermission  = row.value(QStringLiteral("user_permission")).toInt();
        results.append(rec);
    }
    return results;
}

int OperationLogDBCon::queryTotalCountInRange(const QString& startTime, const QString& endTime)
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCountInRange",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
    return count;
}

int OperationLogDBCon::queryRecordPageInRange(int recordId, const QString& startTime, const QString& endTime,
                                              int pageSize)
{
    int page = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryRecordPageInRange",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, page),
                              Q_ARG(int, recordId),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, pageSize));
    return page;
}

QList<OperationRecord> OperationLogDBCon::queryPageWithConditions(const QString& startTime, const QString& endTime,
                                                          int logType, const QString& keyword,
                                                          int pageSize, int pageNumber)
{
    QList<QVariantMap> varResults;
    QMetaObject::invokeMethod(m_sqlLogic, "queryPageWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QList<QVariantMap>, varResults),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, keyword),
                              Q_ARG(int, pageSize),
                              Q_ARG(int, pageNumber));

    QList<OperationRecord> results;
    results.reserve(varResults.size());
    for (const QVariantMap& row : varResults) {
        OperationRecord rec;
        rec.id              = row.value(QStringLiteral("id")).toInt();
        rec.occurTime       = row.value(QStringLiteral("occur_time")).toString();
        rec.logType         = row.value(QStringLiteral("log_type")).toInt();
        rec.description     = row.value(QStringLiteral("description")).toString();
        rec.userPermission  = row.value(QStringLiteral("user_permission")).toInt();
        results.append(rec);
    }
    return results;
}

int OperationLogDBCon::queryTotalCountWithConditions(const QString& startTime, const QString& endTime,
                                                     int logType, const QString& keyword)
{
    int count = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryTotalCountWithConditions",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, count),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, keyword));
    return count;
}

int OperationLogDBCon::queryRecordPosition(int recordId, const QString& startTime, const QString& endTime,
                                          int logType, const QString& keyword)
{
    int position = -1;
    QMetaObject::invokeMethod(m_sqlLogic, "queryRecordPosition",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, position),
                              Q_ARG(int, recordId),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, keyword));
    return position;
}

int OperationLogDBCon::queryFirstRecordPage(const QString& startTime, const QString& endTime,
                                            int logType, const QString& keyword, int pageSize)
{
    int page = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryFirstRecordPage",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, page),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, keyword),
                              Q_ARG(int, pageSize));
    return page;
}

int OperationLogDBCon::queryFirstMatchedId(const QString& startTime, const QString& endTime,
                                           int logType, const QString& keyword)
{
    int id = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryFirstMatchedId",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, id),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, keyword));
    return id;
}

int OperationLogDBCon::queryPrevMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                                           int logType, const QString& keyword)
{
    int id = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryPrevMatchingId",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, id),
                              Q_ARG(int, anchorId),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, keyword));
    return id;
}

int OperationLogDBCon::queryNextMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                                           int logType, const QString& keyword)
{
    int id = 0;
    QMetaObject::invokeMethod(m_sqlLogic, "queryNextMatchingId",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(int, id),
                              Q_ARG(int, anchorId),
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, keyword));
    return id;
}

int OperationLogDBCon::queryMonthRange()
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

    qDebug() << "earliest_time:" << earliestTime << "latest_time:" << latestTime;

    if (earliestTime.isEmpty() || latestTime.isEmpty() || earliestTime == "NULL" || latestTime == "NULL") {
        return 0;
    }

    QDateTime earliest = QDateTime::fromString(earliestTime, "yyyy-MM-dd HH:mm:ss");
    QDateTime latest = QDateTime::fromString(latestTime, "yyyy-MM-dd HH:mm:ss");

    if (!earliest.isValid() || !latest.isValid()) {
        return 0;
    }

    // 计算月份差
    int yearDiff = latest.date().year() - earliest.date().year();
    int monthDiff = latest.date().month() - earliest.date().month();
    int totalMonths = yearDiff * 12 + monthDiff;

    return totalMonths;
}

void OperationLogDBCon::queryTimeBounds(QString& earliestTime, QString& latestTime)
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

void OperationLogDBCon::insertRecord(const QString& occurTime, int logType, const QString& description,
                                     int userPermission)
{
    QMetaObject::invokeMethod(m_sqlLogic, "insertRecord",
                              Qt::QueuedConnection,
                              Q_ARG(QString, occurTime),
                              Q_ARG(int, logType),
                              Q_ARG(QString, description),
                              Q_ARG(int, userPermission));
}

void OperationLogDBCon::deleteByTimeRange(const QString& startTime, const QString& endTime)
{
    QMetaObject::invokeMethod(m_sqlLogic, "deleteByTimeRange",
                              Qt::QueuedConnection,
                              Q_ARG(QString, startTime),
                              Q_ARG(QString, endTime));
}

} // namespace LogDB
