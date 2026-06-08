#include "writesqldbcon.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace LogDB {

WriteSqlDBCon::WriteSqlDBCon(const QString& databasePath, QObject* parent)
    : QObject(parent)
    , m_databasePath(databasePath)
    , m_workerThread(nullptr)
    , m_running(false)
{
    m_workerThread = new QThread(this);
    this->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, this, &WriteSqlDBCon::processQueue);
}

WriteSqlDBCon::~WriteSqlDBCon()
{
    cleanup();
}

bool WriteSqlDBCon::initialize()
{
    // 创建数据库连接
    m_database = QSqlDatabase::addDatabase("QSQLITE", "WriteSqlDBConConnection");
    QString dbFilePath = QString("%1/logdb.db").arg(m_databasePath);
    m_database.setDatabaseName(dbFilePath);

    if (!m_database.open()) {
        qWarning() << "Failed to open database:" << m_database.lastError().text();
        return false;
    }

    // 启动工作线程
    m_workerThread->start();

    return true;
}

void WriteSqlDBCon::cleanup()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        // 设置停止标志
        m_running = false;

        // 唤醒消费者线程
        m_queueMutex.lock();
        m_queueCondition.wakeAll();
        m_queueMutex.unlock();

        // 等待线程结束（线程会先处理完队列中的所有任务）
        m_workerThread->quit();
        m_workerThread->wait();

        if (m_database.isOpen()) {
            m_database.close();
        }
    }
}

void WriteSqlDBCon::addWriteTask(const WriteResult& result)
{
    QMutexLocker locker(&m_queueMutex);
    m_taskQueue.enqueue(result);
    m_queueCondition.wakeOne();
}

int WriteSqlDBCon::getQueueSize()
{
    QMutexLocker locker(&m_queueMutex);
    return m_taskQueue.size();
}

void WriteSqlDBCon::processQueue()
{
    m_running = true;

    while (m_running) {
        WriteResult result;

        {
            QMutexLocker locker(&m_queueMutex);
            if (m_taskQueue.isEmpty()) {
                m_queueCondition.wait(&m_queueMutex);
                continue;
            }
            result = m_taskQueue.dequeue();
        }

        executeTask(result);
    }

    // 处理剩余任务
    m_queueMutex.lock();
    while (!m_taskQueue.isEmpty()) {
        WriteResult result = m_taskQueue.dequeue();
        m_queueMutex.unlock();
        executeTask(result);
        m_queueMutex.lock();
    }
    m_queueMutex.unlock();
}

void WriteSqlDBCon::executeTask(const WriteResult& result)
{
    WriteResult execResult = result;
    execResult.connectionName = "WriteSqlDBConConnection";

    const int opType = result.opType;
    const bool isInsert = (opType == static_cast<int>(WriteOp::Insert));
    const bool isDelete = (opType == static_cast<int>(WriteOp::Delete));
    const bool needsCountUpdate = (isInsert || isDelete) && !result.tableName.isEmpty();

    // 将插入/删除和计数表的更新封装在同一个事务中：任意一步失败都回滚。
    if (!m_database.transaction()) {
        execResult.result = "Failed to begin transaction: "
                            + m_database.lastError().text();
        qWarning() << "[WriteSqlDBCon]" << execResult.result;
        emit taskCompleted(execResult);
        return;
    }

    // 执行主语句（insert / delete / 其他）
    QSqlQuery query(m_database);
    if (!query.prepare(result.sqlStatement)) {
        execResult.result = "Prepare failed: " + query.lastError().text();
        qWarning() << "[WriteSqlDBCon]" << execResult.result;
        m_database.rollback();
        emit taskCompleted(execResult);
        return;
    }
    for (const QVariant& param : result.params) {
        query.addBindValue(param);
    }

    if (!query.exec()) {
        execResult.result = "Exec failed: " + query.lastError().text();
        qWarning() << "[WriteSqlDBCon]" << execResult.result
                   << " sqlId=" << result.sqlId;
        m_database.rollback();
        emit taskCompleted(execResult);
        return;
    }

    // 根据操作类型，在同一事务中更新 log_record_count
    if (needsCountUpdate) {
        int delta = 0;
        if (isInsert) {
            // 批量 INSERT 时 numRowsAffected 可准确反映行数；否则退化为 1
            int affected = query.numRowsAffected();
            delta = (affected > 0) ? affected : 1;
        } else {
            // Delete：负向变化
            int affected = query.numRowsAffected();
            delta = -(affected > 0 ? affected : 0);
        }

        if (delta != 0) {
            QSqlQuery countQuery(m_database);
            countQuery.prepare(
                "UPDATE log_record_count SET total_count = total_count + ? "
                "WHERE table_name = ?");
            countQuery.addBindValue(delta);
            countQuery.addBindValue(result.tableName);
            if (!countQuery.exec()) {
                execResult.result = "Count update failed: "
                                    + countQuery.lastError().text();
                qWarning() << "[WriteSqlDBCon]" << execResult.result
                           << " table=" << result.tableName
                           << " delta=" << delta;
                m_database.rollback();
                emit taskCompleted(execResult);
                return;
            }
        }
    }

    // 提交事务
    if (!m_database.commit()) {
        execResult.result = "Commit failed: " + m_database.lastError().text();
        qWarning() << "[WriteSqlDBCon]" << execResult.result;
        m_database.rollback();
        emit taskCompleted(execResult);
        return;
    }

    execResult.result = "Success";
    emit taskCompleted(execResult);
}

} // namespace LogDB
