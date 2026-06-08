#include "runningloggercollector.h"

#include <QGlobalStatic>

// Q_GLOBAL_STATIC: 线程安全的懒初始化，程序退出时自动析构
Q_GLOBAL_STATIC(RunningLoggerCollector, s_instance)

// =====================================================================
// 单例获取
// =====================================================================
RunningLoggerCollector* RunningLoggerCollector::instance()
{
    return s_instance;
}

// =====================================================================
// 构造
// =====================================================================
RunningLoggerCollector::RunningLoggerCollector(QObject* parent)
    : QObject(parent)
{
    // 定时器运行于主线程，每 50ms 将缓冲队列中的条目提交给 RunningLoggerWidget
    m_flushTimer = new QTimer(this);
    m_flushTimer->setInterval(50);
    connect(m_flushTimer, &QTimer::timeout, this, &RunningLoggerCollector::onFlushTick);
    m_flushTimer->start();
}

// =====================================================================
// 绑定目标控件（须在主线程调用）
// =====================================================================
void RunningLoggerCollector::setTarget(RunningLoggerWidget* widget)
{
    m_target = widget;
}

// =====================================================================
// 线程安全：写入一条日志（任意线程可调用）
// =====================================================================
void RunningLoggerCollector::log(RunningLoggerWidget::MsgType type,
                                 const QString& qrCode,
                                 const QString& alarmId,
                                 const QString& message)
{
    QMutexLocker locker(&m_queueMutex);
    // 队列上限保护，防止生产者速度远超消费者时内存无限增长
    if (m_queue.size() >= kMaxQueueSize) {
        m_queue.dequeue();
    }
    m_queue.enqueue({ type, qrCode, alarmId, message });
}

void RunningLoggerCollector::logMessage(const QString& message)
{
    log(RunningLoggerWidget::MsgType::Message, QString(), QString(), message);
}

// =====================================================================
// 定时刷新：在主线程将缓冲队列中的条目提交给 RunningLoggerWidget
// =====================================================================
void RunningLoggerCollector::onFlushTick()
{
    if (!m_target) return;

    // 只取出需要的条数，避免 swap 整个队列再归还剩余部分的 O(n²) 开销
    QVector<LogEntry> batch;
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_queue.isEmpty()) return;
        int count = qMin(m_queue.size(), kMaxFlushPerTick);
        batch.reserve(count);
        for (int i = 0; i < count; ++i)
            batch.append(m_queue.dequeue());
    }

    // 批量写入模式：仅在结束时统一刷新一次按钮文本，避免每条 setText 触发 repaint
    m_target->beginBatchWrite();
    for (const LogEntry &entry : batch)
        m_target->writeRecord(entry.type, entry.qrCode, entry.alarmId, entry.message);
    m_target->endBatchWrite();
}
