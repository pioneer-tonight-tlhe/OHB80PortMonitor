#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "scheduler_task.h"

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QMap>
#include <QQueue>
#include <QSet>

#define TASK_LOG_PATH "scheduler/scheduler"

class Scheduler : public QObject
{
    Q_OBJECT

public:
    static Scheduler* instance();

    // 提交任务（返回 taskId）
    QString submitTask(SchedulerTask *task);

    // 取消任务
    bool cancelTask(const QString &taskId);

    // 启动/停止调度器
    void start();
    void stop();

signals:
    // 转发任务信号给 UI
    void taskStateChanged(const QString &taskId, SchedulerTask::State state);
    void taskProgress(const QString &taskId, int percent, const QString &msg);
    void taskFinished(const QString &taskId, bool success, const QString &msg);
    void taskDataResult(const QString &key, const QVariantMap &data);

private:
    explicit Scheduler(QObject *parent = nullptr);
    ~Scheduler();

    static Scheduler* s_instance;

    // 调度器线程
    QThread m_thread;

    // 任务管理
    QMap<QString, SchedulerTask*> m_tasks;       // 所有任务（含长驻）
    QQueue<SchedulerTask*> m_pendingQueue;       // 待执行队列（仅普通任务）
    QSet<SchedulerTask*> m_runningTasks;         // 正在运行的普通任务
    QSet<SchedulerTask*> m_persistentTasks;      // 长驻任务（不占并发槽位）
    mutable QMutex m_mutex;

    int m_maxConcurrent = 10;  // 最大并发任务数（仅计普通任务）

    // 连接任务信号
    void connectTaskSignals(SchedulerTask *task);
    void enqueueTask(SchedulerTask *task);

    // 调度下一个任务
    void scheduleNext();

private slots:
    void onTaskStateChanged(SchedulerTask::State state);
    void onTaskFinished(bool success, const QString &msg);
};

#endif // SCHEDULER_H
