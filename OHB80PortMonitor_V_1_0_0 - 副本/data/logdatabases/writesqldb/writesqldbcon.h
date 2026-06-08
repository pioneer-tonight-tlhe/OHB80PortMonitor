#ifndef WRITESQLDBCON_H
#define WRITESQLDBCON_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QQueue>
#include <QSqlDatabase>
#include "dbtypes.h"

namespace LogDB {

class WriteSqlDBCon : public QObject
{
    Q_OBJECT

public:
    explicit WriteSqlDBCon(const QString& databasePath, QObject* parent = nullptr);
    ~WriteSqlDBCon();

    // 初始化
    bool initialize();
    void cleanup();

    // 添加写入操作到队列
    void addWriteTask(const WriteResult& result);

    // 获取当前队列大小
    int getQueueSize();

signals:
    void taskCompleted(const WriteResult& result);

private slots:
    void processQueue();

private:
    void executeTask(const WriteResult& result);

    QString m_databasePath;
    QSqlDatabase m_database;
    QThread* m_workerThread;
    QQueue<WriteResult> m_taskQueue;
    QMutex m_queueMutex;
    QWaitCondition m_queueCondition;
    bool m_running;
};

} // namespace LogDB

#endif // WRITESQLDBCON_H
