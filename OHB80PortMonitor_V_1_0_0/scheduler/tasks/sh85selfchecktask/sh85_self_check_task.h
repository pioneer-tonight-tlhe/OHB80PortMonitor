#ifndef SH85_SELF_CHECK_TASK_H
#define SH85_SELF_CHECK_TASK_H

#include "scheduler_task.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "tasks/record/sh85selfchecktaskrecord.h"

#include <QList>
#include <QMetaType>
#include <QString>


// ====================================================================
// SH85SelfCheckTask — SH85 湿度传感器自检调度任务
//
//   设计说明：
//     本任务作为 SH85SelfChecker（Data 层自检器）的薄包装层，
//     负责将 Scheduler 层的信号接口与 Data 层的自检器对接。
//     自检核心逻辑已下沉到 SH85SelfChecker，本任务仅负责：
//       1. 通过 qrcode 获取 ModbusTcpMaster
//       2. 获取 master->selfChecker() 并启动
//       3. 转发 checker 的 countdownTick 信号给 UI
//       4. 在 countdownTick 中频发 "Checking (N)" statusChanged（最后 10s）
//       5. 转换 SH85SelfChecker 的完成信号到 SchedulerTask 信号接口
//
//   信号：
//     countdownTick   — 整体倒计时（剩余秒数 70 → 0），UI 按钮文案
//     statusChanged   — 状态文本（"Checking (N)" / 终态文案）
//     allFinished     — 任务结束（携带成功标志和具体 Result）
// ====================================================================
class SH85SelfCheckTask : public SchedulerTask
{
    Q_OBJECT

public:
    // 使用 SH85SelfChecker 的 Result 枚举
    using Result = SH85SelfChecker::Result;

    explicit SH85SelfCheckTask(const QString &qrcode, QObject *parent = nullptr);
    ~SH85SelfCheckTask();

    // SchedulerTask 接口
    void start() override;
    void stop()  override;
    QString taskType() const override { return "SH85SelfCheckTask"; }

signals:
    // 整体倒计时（剩余秒数：70 → 0），驱动 UI 按钮文案
    void countdownTick(int remainingSeconds, const QString &qrcode);

    // 阶段性状态文本（"Checking (N)" / "Network Error" / 终态文案）
    void statusChanged(const QString &text, const QString &qrcode);

    // 任务结束
    void allFinished(bool success,
                     SH85SelfChecker::Result result,
                     const QString &qrcode);

private slots:
    void onCheckerStarted(const QString& masterId);
    void onCheckerCountdownTick(int remainingSeconds, const QString& masterId);
    void onCheckerErrorOccurred(SH85SelfChecker::Result result, const QString& message, const QString& masterId);
    void onCheckerFinished(bool success, SH85SelfChecker::Result result, const QString& message, const QString& masterId);
    void onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void onCommandCompleted(ModbusCommand cmd, const QString& masterId);
    void onCommandRetrying(ModbusCommand cmd, const QString& masterId);

private:
    // ---- 私有方法 ----
    bool ensureMaster();              // 校验 master
    void finishWith(bool success, SH85SelfChecker::Result result, const QString &uiText);
    void writeCompletionLog(bool success, SH85SelfChecker::Result result); // 写入运行/警报日志
    static QString resultToText(SH85SelfChecker::Result r);
    static QString resultToChineseText(SH85SelfChecker::Result r);
    static QString stateToChineseText(SH85SelfChecker::State s);

    // ---- 成员 ----
    QString  m_qrcode;
    ModbusTcpMaster *m_master = nullptr;
    SH85SelfChecker *m_checker = nullptr;

    QList<QMetaObject::Connection> m_checkerConnections;  // checker 信号连接

    // 标志位
    bool m_stopped         = false;
    bool m_finishedEmitted = false;
    SH85SelfChecker::State m_currentCheckerState = SH85SelfChecker::State::Idle;
    SH85SelfCheckTaskRecord m_record;
};

// 跨线程信号（QueuedConnection）需要将枚举注册为 Qt 元类型
Q_DECLARE_METATYPE(SH85SelfChecker::Result)

#endif // SH85_SELF_CHECK_TASK_H
