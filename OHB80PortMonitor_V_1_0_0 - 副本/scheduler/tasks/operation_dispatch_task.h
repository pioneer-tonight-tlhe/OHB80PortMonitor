#ifndef OPERATION_DISPATCH_TASK_H
#define OPERATION_DISPATCH_TASK_H

#include "../scheduler_task.h"
#include "operationrecord.h"

#include <QList>
#include <QMutex>
#include <QString>

// ====================================================================
// OperationDispatchTask —— 操作调度常驻任务
//
// 取代旧 RunningLoggerCollector + RunningLoggerWidget 的组合。
//
// 职责：
//   1. 业务侧（任意线程）通过 SharedData::getOperationDispatchTask() 拿到本任务，
//      调用 logMessage / logWarn / logError / log 提交一条操作日志。
//   2. 直接落入 LogDB::OperationLogDBCon（SQLite）。OperationLogDBCon 内部已用
//      QueuedConnection 异步派发，本类无需缓冲队列。
//
// UI 实时显示：
//   订阅本类的 operationLogInserted 信号即可获取插入完成的日志信息。
// ====================================================================
class OperationDispatchTask : public SchedulerTask
{
    Q_OBJECT

public:
    enum class MsgType {
        Message = 0,    // 普通运行信息（与 LogDB::OperationLogType::Message 对齐）
        Warn    = 1,    // 警告（与 LogDB::OperationLogType::Warn 对齐）
        Error   = 2     // 错误（与 LogDB::OperationLogType::Error 对齐）
    };

    explicit OperationDispatchTask(QObject* parent = nullptr);
    ~OperationDispatchTask() override = default;

    // SchedulerTask 接口 —— 常驻、无周期、无内部循环
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("OperationDispatchTask"); }
    bool isPersistent() const override { return true; }

    // 通用入口（线程安全）
    void log(MsgType type, const QString& message, int userPermission = 0);

    // 便捷入口（线程安全）
    void logMessage(const QString& message);
    void logWarn(const QString& message);
    void logError(const QString& message);

    // 取出最近的若干条已提交日志（线程安全）。
    // 用于 UI 订阅前已经派发的日志补播 —— 避免 UI 控件构造时机晚于早期日志而丢失显示。
    QList<OperationRecord> recentRecords() const;

signals:
    // 操作日志插入完成信号（携带 OperationRecord）
    void operationLogInserted(const OperationRecord& record);

private:
    // 最近日志环形缓存上限
    static constexpr int kRecentBufferMax = 500;
    mutable QMutex m_recentMutex;
    QList<OperationRecord> m_recentRecords;
};

#endif // OPERATION_DISPATCH_TASK_H
