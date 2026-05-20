#include "scheduler.h"
#include "app/applogger.h"
#include "loggermanager.h"
#include <QDebug>
#include <QTimer>

Scheduler* Scheduler::s_instance = nullptr;

Scheduler::Scheduler(QObject *parent)
    : QObject(parent)
{
    qDebug() << "=============================Scheduler 调度器开始=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================Scheduler 调度器开始=============================");
    // 将调度器移动到独立线程
    this->moveToThread(&m_thread);
    m_thread.setObjectName("SchedulerThread");
}

Scheduler::~Scheduler()
{
    stop();
    qDebug() << "=============================Scheduler 调度器结束=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================Scheduler 调度器结束=============================");
}

Scheduler* Scheduler::instance()
{
    if (!s_instance) {
        s_instance = new Scheduler();
    }
    return s_instance;
}

void Scheduler::start()
{
    if (!m_thread.isRunning()) {
        m_thread.start();
        qDebug() << "[Scheduler][start] 调度器线程已启动";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, "[Scheduler][start] 调度器线程已启动");
    }
}

void Scheduler::stop()
{
    if (!m_thread.isRunning()) return;

    // 收集需要停止的任务（在主线程侧加锁读取）
    QList<SchedulerTask*> tasksToStop;
    {
        QMutexLocker locker(&m_mutex);
        for (SchedulerTask *task : qAsConst(m_persistentTasks))
            tasksToStop.append(task);
        for (SchedulerTask *task : qAsConst(m_runningTasks))
            tasksToStop.append(task);
    }

    // 在 task 自己所在的线程（scheduler 线程）中调用 stop()，避免跨线程操作定时器
    for (SchedulerTask *task : tasksToStop) {
        QMetaObject::invokeMethod(task, [task]() { task->stop(); }, Qt::BlockingQueuedConnection);
    }

    {
        QMutexLocker locker(&m_mutex);
        m_persistentTasks.clear();
        m_runningTasks.clear();
        m_pendingQueue.clear();
        // 不能从主线程 delete 住在调度器线程上的 Task 对象，
        // 改用 deleteLater() 让调度器线程自行销毁
        for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
            if (it.value()) {
                it.value()->deleteLater();
            }
        }
        m_tasks.clear();
    }

    // 停止线程：deleteLater 事件会在 quit 退出事件循环前被处理
    m_thread.quit();
    m_thread.wait();
    qDebug() << "[Scheduler][stop] 调度器线程已停止";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, "[Scheduler][stop] 调度器线程已停止");
}

QString Scheduler::submitTask(SchedulerTask *task)
{
    if (!task) return QString();
    
    QMutexLocker locker(&m_mutex);
    
    QString taskId = task->taskId();
    
    // 先解除父对象，再移动到调度器线程（Qt 不允许移动有 parent 的对象）
    task->setParent(nullptr);
    task->moveToThread(&m_thread);
    
    // 连接信号
    connectTaskSignals(task);
    
    // 注册任务
    m_tasks[taskId] = task;
    
    if (task->isPersistent()) {
        // 长驻任务：立即启动，不入队，不占并发槽位
        m_persistentTasks.insert(task);
        qDebug() << "[Scheduler][submitTask] 提交长驻任务:" << task->taskType() << "taskId:" << taskId;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, 
            QString("[Scheduler][submitTask] 提交长驻任务: %1 taskId: %2").arg(task->taskType()).arg(taskId).toStdString());
        QMetaObject::invokeMethod(task, [task]() { task->start(); }, Qt::QueuedConnection);
    } else {
        // 普通任务：入队等待调度
        m_pendingQueue.enqueue(task);
        qDebug() << "[Scheduler][submitTask] 提交普通任务:" << task->taskType() << "taskId:" << taskId;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, 
            QString("[Scheduler][submitTask] 提交普通任务: %1 taskId: %2").arg(task->taskType()).arg(taskId).toStdString());
    }
    
    locker.unlock();
    
    // 尝试调度普通任务
    if (!task->isPersistent()) {
        scheduleNext();
    }
    
    return taskId;
}

bool Scheduler::cancelTask(const QString &taskId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_tasks.contains(taskId)) {
        return false;
    }
    
    SchedulerTask *task = m_tasks[taskId];
    
    // 停止任务
    if (m_persistentTasks.contains(task)) {
        task->stop();
        m_persistentTasks.remove(task);
    } else if (m_runningTasks.contains(task)) {
        task->stop();
        m_runningTasks.remove(task);
    }
    
    // 从待执行队列中移除
    m_pendingQueue.removeAll(task);
    
    // 从任务表中移除并删除
    m_tasks.remove(taskId);
    task->deleteLater();
    
    qDebug() << "[Scheduler][cancelTask] 取消任务:" << taskId;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, 
        QString("[Scheduler][cancelTask] 取消任务: %1").arg(taskId).toStdString());
    
    return true;
}

void Scheduler::connectTaskSignals(SchedulerTask *task)
{
    connect(task, &SchedulerTask::stateChanged,
            this, &Scheduler::onTaskStateChanged);
    
    connect(task, &SchedulerTask::finished,
            this, &Scheduler::onTaskFinished);
    
    connect(task, &SchedulerTask::progress,
            this, [this, task](int percent, const QString &msg) {
        emit taskProgress(task->taskId(), percent, msg);
    });
    
    connect(task, &SchedulerTask::dataResult,
            this, [this](const QString &key, const QVariantMap &data) {
        emit taskDataResult(key, data);
    });
}

void Scheduler::scheduleNext()
{
    QMutexLocker locker(&m_mutex);
    
    while (!m_pendingQueue.isEmpty() && m_runningTasks.size() < m_maxConcurrent) {
        SchedulerTask *task = m_pendingQueue.dequeue();
        m_runningTasks.insert(task);
        
        qDebug() << "[Scheduler][scheduleNext] 启动任务:" << task->taskType() << "taskId:" << task->taskId();
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, 
            QString("[Scheduler][scheduleNext] 启动任务: %1 taskId: %2").arg(task->taskType()).arg(task->taskId()).toStdString());
        
        // 在调度器线程中启动任务
        QMetaObject::invokeMethod(task, [task]() { task->start(); }, Qt::QueuedConnection);
    }
}

void Scheduler::onTaskStateChanged(SchedulerTask::State state)
{
    SchedulerTask *task = qobject_cast<SchedulerTask*>(sender());
    if (!task) return;
    
    emit taskStateChanged(task->taskId(), state);
}

void Scheduler::onTaskFinished(bool success, const QString &msg)
{
    SchedulerTask *task = qobject_cast<SchedulerTask*>(sender());
    if (!task) return;

    QString taskId = task->taskId();

    qDebug() << "[Scheduler][onTaskFinished] 任务完成:" << task->taskType()
             << "taskId:" << taskId
             << "成功:" << success << msg;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        success ? Level::INFO : Level::WARN,
        QString("[Scheduler][onTaskFinished] 任务完成: %1 taskId: %2 成功: %3 %4").arg(task->taskType()).arg(taskId).arg(success).arg(msg).toStdString());

    emit taskFinished(taskId, success, msg);

    // 清理：从运行列表和任务表中移除
    QMutexLocker locker(&m_mutex);
    m_runningTasks.remove(task);
    m_tasks.remove(taskId);
    locker.unlock();

    // 延迟删除 task，确保 UI 线程有时间处理已排队的信号
    // 避免因 task 被过早销毁导致跨线程信号丢失
    QTimer::singleShot(100, task, &QObject::deleteLater);

    // 调度下一个任务
    scheduleNext();
}
