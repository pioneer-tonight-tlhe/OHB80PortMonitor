#ifndef RUNNINGLOGGERCOLLECTOR_H
#define RUNNINGLOGGERCOLLECTOR_H

#include "runningloggerwidget.h"

#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QMutex>
#include <QTimer>

// ====================================================================
// RunningLoggerCollector —— 运行日志采集器（单例）
//
// 作用：
//   跨线程全局日志采集入口。任意线程均可调用 log() / logMessage()
//   提交日志条目，内部通过互斥锁保护缓冲队列，再由主线程定时器
//   异步将条目依次提交给 RunningLoggerWidget。
//
// 使用方式：
//   1. 在主线程中调用 setTarget() 绑定目标控件
//   2. 任意线程调用 logMessage(msg) 或 log(type, qrCode, alarmId, msg)
// ====================================================================
class RunningLoggerCollector : public QObject
{
    Q_OBJECT

public:
    static RunningLoggerCollector* instance();

    // 绑定目标控件（须在主线程调用）
    void setTarget(RunningLoggerWidget* widget);

    // 线程安全：写入一条日志
    void log(RunningLoggerWidget::MsgType type,
             const QString& qrCode,
             const QString& alarmId,
             const QString& message);

    // 便捷方法：写入 Message 级别日志（qrCode / alarmId 为空）
    void logMessage(const QString& message);

    // 每帧最多提交条目数（防止队列积压时单帧 UI 卡顿）
    static constexpr int kMaxFlushPerTick = 20;
    // 队列最大容量（超过时丢弃最旧条目，防止生产速度远超消费速度时内存膨胀）
    static constexpr int kMaxQueueSize = 2000;

    explicit RunningLoggerCollector(QObject* parent = nullptr);

private:
    struct LogEntry {
        RunningLoggerWidget::MsgType type;
        QString qrCode;
        QString alarmId;
        QString message;
    };

private slots:
    void onFlushTick();

private:
    QPointer<RunningLoggerWidget> m_target;       // Widget 销毁时自动置 null
    QQueue<LogEntry>              m_queue;
    QMutex                        m_queueMutex;
    QTimer*                       m_flushTimer = nullptr;
};

#endif // RUNNINGLOGGERCOLLECTOR_H
