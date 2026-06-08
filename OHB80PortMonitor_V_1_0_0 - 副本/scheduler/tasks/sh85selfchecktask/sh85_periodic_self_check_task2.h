#ifndef SH85_PERIODIC_SELF_CHECK_TASK2_H
#define SH85_PERIODIC_SELF_CHECK_TASK2_H

#include "scheduler_task.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "sh85_self_check_log_helper.h"
#include "boot_delay_timer.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

// ====================================================================
// SH85PeriodicSelfCheckTask2 - SH85 周期自检调度任务（第二版）
//
// 设计目标：
//   1. 作为常驻调度任务，按周期筛选可执行 SH85 自检的设备。
//   2. 每轮自检并行启动各设备的 SH85SelfChecker，并统一转发 UI 所需信号。
//   3. 日志复用 SH85SelfCheckLogHelper：设备过程先聚合到内存 record，
//      设备结束时写一条完整明细，整轮结束时只写汇总/失败告警/分隔线。
// ====================================================================
class SH85PeriodicSelfCheckTask2 : public SchedulerTask
{
    Q_OBJECT

public:
    enum class TimeUnit {
        Second,
        Minute,
        Hour
    };
    Q_ENUM(TimeUnit)

    enum class State {
        Stopped,
        Checking,
        WaitingNext
    };
    Q_ENUM(State)

    struct DeviceResult {
        QString qrcode;
        bool participated = false;
        bool success = false;
        QString description;
    };

    struct SelfCheckSummary {
        QString startTime;
        QString endTime;
        int successCount = 0;
        int failureCount = 0;
        QList<DeviceResult> details;
    };

    explicit SH85PeriodicSelfCheckTask2(QObject *parent = nullptr);
    ~SH85PeriodicSelfCheckTask2() override;

    // ---- SchedulerTask 生命周期接口 ----
    void start() override;
    void stop() override;

    QString taskType() const override { return QStringLiteral("SH85PeriodicSelfCheckTask2"); }
    bool isPersistent() const override { return true; }

    // 当前调度任务状态，供 UI 或调试输出展示。
    State currentState() const { return m_taskState; }

    // 启用/停用周期自检；停用后不再触发新轮次。
    Q_INVOKABLE void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // 设置周期，unit 支持 s / min / hour。
    Q_INVOKABLE void setPeriod(int value, SH85PeriodicSelfCheckTask2::TimeUnit unit);
    Q_INVOKABLE void setPeriod(int value, const QString& unit);
    int periodSeconds() const { return m_periodSec; }
    static QString timeUnitToString(TimeUnit unit);

    // 单设备模式：非空时只执行指定二维码设备；空字符串表示恢复全量模式。
    Q_INVOKABLE void setSingleDevice(const QString& qrcode);
    Q_INVOKABLE void runSingleDeviceOnce(const QString& qrcode);

    // 筛选当前可执行自检的设备列表，不写日志，仅用于 UI/调试查询。
    QStringList filterAvailableDevices();

signals:
    // ---- 周期任务整体状态，供周期配置 UI 显示 ----
    void taskStateChanged(SH85PeriodicSelfCheckTask2::State state);
    void elapsedTick(int elapsedSeconds);
    void intervalCountdown(int remainingSeconds);
    void bootDelayCountdown(int remainingSeconds);

    // ---- 转发单设备自检信号，供 UI 显示倒计时/状态/单设备结果 ----
    void countdownTick(int remainingSeconds, const QString& masterId);
    void selfCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void oneFinished(const QString& masterId, bool success, const QString& message);
    void statusChanged(const QString& text, const QString& qrcode);
    void singleFinished(bool success, SH85SelfChecker::Result result, const QString& qrcode);

    // ---- 转发自检过程中的命令/错误信号，供外层需要时订阅 ----
    void errorOccurred(SH85SelfChecker::Result result, const QString& message, const QString& masterId);
    void commandCompleted(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);

    // 一轮自检全部结束后的轻量汇总信号。
    void allFinished(const SH85PeriodicSelfCheckTask2::SelfCheckSummary& summary);
    void deviceParticipated(const QString& qrcode, bool participated);
    void allDevicesFinished(int totalCount, int successCount, int failureCount);

private slots:
    void onIntervalTick();
    void onBootDelayTimeout();
    void onPeriodTimeout();
    void onCheckerCountdownTick(int remainingSeconds, const QString& masterId);
    void onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void onCheckerFinished(bool success,
                           SH85SelfChecker::Result result,
                           const QString& message,
                           const QString& masterId);
    void onCheckerCommandCompleted(ModbusCommand cmd, const QString& masterId);
    void onCheckerCommandRetrying(ModbusCommand cmd, const QString& masterId);
    void onCheckerErrorOccurred(SH85SelfChecker::Result result,
                                const QString& message,
                                const QString& masterId);

private:
    // ---- 周期与轮次控制 ----
    void initPeriodTimer();
    void startAvailableDeviceChecks(bool manualRound = false, const QString& manualQrcode = QString());
    void finishDevice(const QString& qrcode,
                      bool success,
                      SH85SelfChecker::Result result,
                      const QString& description);
    void tryFinishRound();
    void enterTaskState(State state);

    // ---- checker 信号连接与异常收口 ----
    void disconnectAllCheckers();
    void appendNotSubmittedAndFinish(const QString& qrcode,
                                     SH85SelfChecker::Result result,
                                     const QString& reason);
    static QString currentTimestamp();

    // ---- 配置 ----
    bool m_enabled = true;
    int m_periodSec = 1800;

    // ---- 单设备模式 ----
    bool m_singleDeviceMode = false;
    QString m_singleDeviceQrcode;
    QStringList m_availableDevices;

    // ---- 周期定时器 ----
    QTimer* m_periodTimer = nullptr;
    QTimer* m_elapsedTimer = nullptr;
    BootDelayTimer* m_bootDelay = nullptr;
    int m_periodRemainingCalls = 0;
    int m_intervalRemainingSec = 0;
    int m_elapsedSeconds = 0;

    // ---- 一轮自检上下文 ----
    QString m_roundId;
    QString m_roundStartTime;
    bool m_roundActive = false;
    bool m_currentRoundManual = false;
    SH85SelfChecker::Result m_lastResult = SH85SelfChecker::Result::Success;
    State m_taskState = State::Stopped;
    QStringList m_roundOrderedQrcodes;
    QSet<QString> m_pendingQrcodes;
    QHash<QString, DeviceResult> m_roundResults;
    QHash<QString, SH85SelfCheckTaskRecord> m_deviceRecords;

    // ---- checker 当前阶段与连接句柄 ----
    QHash<QString, SH85SelfChecker::State> m_checkerStates;
    QList<QMetaObject::Connection> m_checkerConnections;
};

Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask2::TimeUnit)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask2::State)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask2::DeviceResult)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask2::SelfCheckSummary)

#endif // SH85_PERIODIC_SELF_CHECK_TASK2_H
