/*******************************************************************************************
 * @file operation_dispatch_task.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class OperationDispatchTask
 * @brief 负责接收业务侧运行日志事件并统一写入运行日志数据库。
 *
 * 设计目标：
 *      1. 取代旧的运行日志采集与显示组合，统一运行日志调度入口。
 *      2. 为任意线程提供线程安全的 Message、Warn、Error 写入接口。
 *      3. 保留最近写入记录缓存，支持 UI 在订阅信号前补齐早期日志显示。
 *******************************************************************************************/
#ifndef OPERATION_DISPATCH_TASK_H
#define OPERATION_DISPATCH_TASK_H

#include <QList>
#include <QMutex>
#include <QString>

#include "../../scheduler_task.h"
#include "operationrecord.h"

class OperationDispatchTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 公共数据类型 ============================
    enum class MsgType
    {
        Message = 0,
        Warn = 1,
        Error = 2
    };

    // ============================ 构造函数 ============================
    explicit OperationDispatchTask(QObject* parent = nullptr);
    ~OperationDispatchTask() override = default;

    // ============================ 任务生命周期 ============================
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("OperationDispatchTask"); }
    bool isPersistent() const override { return true; }

    // ============================ 业务接口 ============================
    void log(MsgType type, const QString& message, int userPermission = 0);
    void logMessage(const QString& message);
    void logWarn(const QString& message);
    void logError(const QString& message);
    QList<OperationRecord> recentRecords() const;

signals:
    // ---- 实时结果 ----
    void operationLogInserted(const OperationRecord& record);

private:
    // ---- 缓存成员 ----
    static constexpr int kRecentBufferMax = 500;
    mutable QMutex m_recentMutex;
    QList<OperationRecord> m_recentRecords;
};

#endif // OPERATION_DISPATCH_TASK_H
