#include "operation_dispatch_task.h"

#include <QDateTime>
#include <QDebug>
#include <QMutexLocker>

#include "logdatabases/databasemanager.h"
#include "logdatabases/operationlogdb/operationlogdbcon.h"


OperationDispatchTask::OperationDispatchTask(QObject* parent)
    : SchedulerTask(parent)
{
    qDebug() << "[OperationDispatchTask] constructed";
}

void OperationDispatchTask::start()
{
    setState(Running);
    qDebug() << "[OperationDispatchTask] started";
    emit progress(0, QStringLiteral("Operation log dispatcher ready"));
}

void OperationDispatchTask::stop()
{
    setState(Cancelled);
    emit finished(false, QStringLiteral("Operation log dispatcher stopped"));
    qDebug() << "[OperationDispatchTask] stopped";
}

void OperationDispatchTask::logMessage(const QString& message)
{
    log(MsgType::Message, message);
}

void OperationDispatchTask::logWarn(const QString& message)
{
    log(MsgType::Warn, message);
}

void OperationDispatchTask::logError(const QString& message)
{
    log(MsgType::Error, message);
}

void OperationDispatchTask::log(MsgType type, const QString& message, int userPermission)
{
    auto* db = LogDB::DatabaseManager::instance().operationLogCon();
    if (!db) {
        qWarning() << "[OperationDispatchTask] OperationLogDBCon unavailable, drop:"
                   << message;
        return;
    }

    // MsgType 数值与 LogDB::OperationLogType 对齐，可直接 cast
    const int logType = static_cast<int>(type);
    const QString occurTime =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    db->insertRecord(occurTime, logType, message, userPermission);

    // 发出插入完成信号，供 UI 接收显示
    OperationRecord record;
    record.occurTime      = occurTime;
    record.logType        = logType;
    record.description    = message;
    record.userPermission = userPermission;

    // 写入最近日志环形缓存，便于 UI 订阅前已派发的日志补播
    {
        QMutexLocker locker(&m_recentMutex);
        m_recentRecords.append(record);
        while (m_recentRecords.size() > kRecentBufferMax) {
            m_recentRecords.removeFirst();
        }
    }

    emit operationLogInserted(record);
}

QList<OperationRecord> OperationDispatchTask::recentRecords() const
{
    QMutexLocker locker(&m_recentMutex);
    return m_recentRecords;
}
