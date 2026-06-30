#ifndef SH85_PERIODIC_SELF_CHECK_TASK3_H
#define SH85_PERIODIC_SELF_CHECK_TASK3_H

#include "scheduler_task.h"
#include "sh85_self_check_device_selector.h"
#include "sh85_self_check_log_service.h"
#include "sh85_self_check_round_context.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

class QTimer;
class SH85SelfCheckRoundRunner;

// ====================================================================
// SH85PeriodicSelfCheckTask3 - SH85 周期自检调度任务（第三版）
//
// 设计目标：
//   1. 保持 Task2 风格的外部接口和 UI 信号，方便后续平滑替换。
//   2. Task3 只做调度编排；轮次状态、设备筛选、checker 执行、日志落库分别拆到独立模块。
//   3. 后续扩展报告弹窗、每设备日志、单次自检复用时，尽量不再膨胀调度类。
// ====================================================================
class SH85PeriodicSelfCheckTask3 : public SchedulerTask
{
    Q_OBJECT

public:
    // ==================================== 公共数据类型 ====================================
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
        double minimumHumidity = -1.0;
    };

    struct SelfCheckSummary {
        QString roundId;
        QString startTime;
        QString endTime;
        int totalCount = 0;
        int participatedCount = 0;
        int successCount = 0;
        int failureCount = 0;
        int skippedCount = 0;
        QList<DeviceResult> details;
    };

    // ==================================== 构造函数 ====================================
    explicit SH85PeriodicSelfCheckTask3(QObject* parent = nullptr);
    ~SH85PeriodicSelfCheckTask3() override;

    // ============================ 基类 SchedulerTask 相关接口 ============================
    void start() override;
    void stop() override;

    QString taskType() const override { return QStringLiteral("SH85PeriodicSelfCheckTask3"); }
    bool isPersistent() const override { return true; }

    State currentState() const { return m_taskState; }
    QString currentStateText() const;
    static QString timeUnitToString(TimeUnit unit);
    static QString stateToString(State state);

    // ==================================== 自检开启功能 ====================================
    // 启用状态只影响后续触发；正在执行的轮次仍由 stop() 统一收口。
    Q_INVOKABLE void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    // ==================================== 设置自检周期 ====================================
    // unit 支持 s / min / hour，与 Task2 行为保持一致。
    Q_INVOKABLE void setPeriod(int value, const QString& unit);
    Q_INVOKABLE void setPeriod(int value, TimeUnit unit);
    int periodSeconds() const { return m_periodSec; }

    // ================================== 单个设备自检功能 ==================================
    // 非空二维码表示进入单设备模式；空字符串表示恢复全量模式。
    void setSingleDevice(const QString& qrcode);

    // ==================================== 筛选自检的设备 ====================================
    // 供 UI 查询当前可执行设备；不修改当前轮次上下文。
    QStringList filterAvailableDevices();

signals:
    // ---- 转发单设备自检信号，供 UI 显示倒计时/状态/单设备结果 ----
    void countdownTick(int remainingSeconds, const QString& masterId);
    void selfCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void oneFinished(const QString& masterId,
                     bool success,
                     const QString& message,
                     double minimumHumidity);

    // ---- 转发自检过程中的命令/错误信号，供外层需要时订阅 ----
    void errorOccurred(SH85SelfChecker::Result result, const QString& message, const QString& masterId);
    void commandCompleted(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);

    // ---- 更清晰的轮次级信号，供新版报告弹窗或日志模块订阅 ----
    void roundStarted(const QString& roundId, int totalCount);
    void roundFinished(const QString& roundId,
                       int totalCount,
                       int successCount,
                       int failureCount,
                       int skippedCount);
    void allDevicesFinished(int totalCount, int successCount, int failureCount);

    // ---- 兼容现有周期自检 UI 的状态/汇总信号 ----
    void taskStateChanged(SH85PeriodicSelfCheckTask3::State state);
    void elapsedTick(int elapsedSeconds);
    void intervalCountdown(int remainingSeconds);
    void bootDelayCountdown(int remainingSeconds);
    void allFinished(const SH85PeriodicSelfCheckTask3::SelfCheckSummary& summary);
    void deviceParticipated(const QString& qrcode, bool participated);

private slots:
    void onPeriodTimeout();
    void onElapsedTimerTick();
    void onIntervalCountdownTick();

    void onCheckerCountdownTick(int remainingSeconds, const QString& masterId);
    void onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void onCheckerFinished(bool success,
                           SH85SelfChecker::Result result,
                           const QString& message,
                           const QString& masterId,
                           double minimumHumidity);
    void onCheckerCommandCompleted(ModbusCommand cmd, const QString& masterId);
    void onCheckerCommandRetrying(ModbusCommand cmd, const QString& masterId);
    void onCheckerErrorOccurred(SH85SelfChecker::Result result,
                                const QString& message,
                                const QString& masterId);

private:
    void initPeriodTimer();
    void initRoundRunner();

    // 初始化新一轮、筛选设备、提交 checker，并在空轮次时立即收口。
    void startAvailableDeviceChecks();

    // 单设备结束统一入口：正常完成、启动失败、stop 取消都走这里。
    void finishDevice(const QString& qrcode,
                      bool success,
                      SH85SelfChecker::Result result,
                      const QString& description,
                      double minimumHumidity);

    // 当 pending 清空时发出整轮汇总信号。
    void tryFinishRound();
    void appendNotSubmittedAndFinish(const QString& qrcode,
                                     SH85SelfChecker::Result result,
                                     const QString& reason);
    void enterTaskState(State state);
    void startIntervalCountdown();
    void stopIntervalCountdown();
    SelfCheckSummary buildSelfCheckSummary(const SH85SelfCheck::RoundSummary& summary) const;
    static QString currentTimestamp();

private:
    // ---- 自检开启功能 ----
    bool m_enabled = true;

    // ---- 设置自检周期 ----
    int m_periodSec = 1800;

    // ---- 单个设备自检功能 ----
    bool m_singleDeviceMode = false;
    QString m_singleDeviceQrcode;
    QStringList m_availableDevices;

    // ---- 周期触发控制 ----
    QTimer* m_periodTimer = nullptr;
    QTimer* m_elapsedTimer = nullptr;
    QTimer* m_intervalCountdownTimer = nullptr;
    int m_periodRemainingCalls = 0;
    int m_elapsedSeconds = 0;
    int m_intervalRemainingSeconds = 0;
    State m_taskState = State::Stopped;

    // ---- 拆分后的功能模块 ----
    SH85SelfCheckRoundContext m_roundContext;
    SH85SelfCheckDeviceSelector m_deviceSelector;
    SH85SelfCheckLogService m_logService;
    SH85SelfCheckRoundRunner* m_roundRunner = nullptr;

    // ---- checker 当前阶段缓存，预留给后续日志/报告归属使用 ----
    QHash<QString, SH85SelfChecker::State> m_checkerStates;
};

Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask3::TimeUnit)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask3::State)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask3::DeviceResult)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask3::SelfCheckSummary)

#endif // SH85_PERIODIC_SELF_CHECK_TASK3_H
