#ifndef SH85_PERIODIC_SELF_CHECK_TASK_H
#define SH85_PERIODIC_SELF_CHECK_TASK_H

#include "../scheduler_task.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QSet>
#include <QMetaType>
#include <QString>
#include <QStringList>

class QTimer;
class ModbusTcpMaster;

// ====================================================================
// SH85PeriodicSelfCheckTask — SH85 周期自检调度任务（常驻任务）
//
//   设计说明：
//     - 由 SharedData::initScheduler() 创建并提交调度器（常驻任务）；
//     - UI（SH85PeriodicSelfCheckSettingWidget）通过
//       SharedData::getSH85PeriodicSelfCheckTask() 获取实例并订阅信号；
//     - 通过 setEnabled / setPeriod 控制启停与周期；
//     - 一轮自检中，所有可用设备的 SH85SelfChecker 同时启动（并行）；
//     - 等所有设备均结束后进入"等待下次自检"状态；
//     - 内部状态机：Checking / WaitingNext / Stopped；
//     - 内部 1Hz 定时器仅在 WaitingNext 状态启用。
//
//   执行逻辑：
//     1. 遍历所有 foupinfo，过滤 enable=false 的设备
//     2. 网络未连接的设备直接判定为失败，发出 oneFinished
//     3. 其余设备并行启动 SH85SelfChecker
//     4. 所有设备结束后发出 allFinished，进入 WaitingNext
// ====================================================================
class SH85PeriodicSelfCheckTask : public SchedulerTask
{
    Q_OBJECT

public:
    using Result = SH85SelfChecker::Result;

    /**
     * @brief 周期单位
     */
    enum class TimeUnit {
        Second,
        Minute,
        Hour
    };
    Q_ENUM(TimeUnit)

    /**
     * @brief 任务整体状态机
     */
    enum class State {
        Stopped,        // 停止（功能未启用 / 已停止）
        Checking,       // 自检进行中（一轮内并行执行）
        WaitingNext     // 等待下一次自检（倒计时中）
    };
    Q_ENUM(State)

    /**
     * @brief 单设备自检结果（用于 SelfCheckSummary）
     */
    struct DeviceResult {
        QString qrcode;            // 设备 QRCode
        bool    participated = false; // 是否参加（enable=true 才参加）
        bool    success      = false; // 是否自检成功
        QString description;       // 失败原因 / 成功描述
    };

    /**
     * @brief 一轮自检汇总
     */
    struct SelfCheckSummary {
        QString  startTime;        // 开始时间
        QString  endTime;          // 结束时间
        int      successCount = 0; // 成功数
        int      failureCount = 0; // 失败数
        QList<DeviceResult> details;  // 每个设备的明细（按 QRCode 顺序）
    };

    explicit SH85PeriodicSelfCheckTask(QObject *parent = nullptr);
    ~SH85PeriodicSelfCheckTask() override;

    // ---- 公开控制接口 ----

    /**
     * @brief 启用/停用自检功能
     * @details 启用：立即进入 Checking 状态执行一轮自检；
     *          停用：
     *            - 当前若处于 Checking，**不打断**当前轮次，本轮完成后转 Stopped；
     *            - 当前若处于 WaitingNext，立即转 Stopped；
     *            - 当前若处于 Stopped，无操作。
     * @note  Q_INVOKABLE：允许 UI 通过 QMetaObject::invokeMethod 跨线程调用。
     */
    Q_INVOKABLE void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }

    /**
     * @brief 设置自检周期
     * @param value  数值（>=1）
     * @param unit   时间单位（s / min / hour）
     * @note  Q_INVOKABLE：允许 UI 通过 QMetaObject::invokeMethod 跨线程调用。
     */
    Q_INVOKABLE void setPeriod(int value, SH85PeriodicSelfCheckTask::TimeUnit unit);

    /// 返回当前周期换算的总秒数
    int periodSeconds() const { return m_periodSec; }

    /// 当前状态
    State currentState() const { return m_state; }

    // ---- SchedulerTask 接口 ----
    void start() override;
    void stop()  override;
    QString taskType() const override { return "SH85PeriodicSelfCheckTask"; }

    /// TimeUnit / State 字符串化（日志用）
    static QString timeUnitToString(TimeUnit u);
    static QString stateToString(State s);

signals:
    /**
     * @brief 单设备自检倒计时（直接转发自 SH85SelfChecker::countdownTick）
     */
    void countdownTick(int remainingSeconds, const QString& masterId);

    /**
     * @brief 单设备自检阶段状态（直接转发自 SH85SelfChecker::stateChanged）
     */
    void selfCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);

    /**
     * @brief 单设备自检结束
     */
    void oneFinished(const QString& masterId, bool success, const QString& description);

    /**
     * @brief 一轮自检全部结束
     */
    void allFinished(const SH85PeriodicSelfCheckTask::SelfCheckSummary& summary);

    /**
     * @brief 设备参与状态变化（提前通知，不等到一轮结束）
     */
    void deviceParticipated(const QString& qrcode, bool participated);

    // ---- UI 辅助信号（任务整体状态） ----

    /**
     * @brief 任务整体状态切换
     */
    void taskStateChanged(SH85PeriodicSelfCheckTask::State state);

    /**
     * @brief Checking 状态下，已执行的秒数（1Hz）
     */
    void elapsedTick(int elapsedSeconds);

    /**
     * @brief WaitingNext 状态下，距离下次自检的剩余秒数（1Hz）
     */
    void intervalCountdown(int remainingSeconds);

private slots:
    void onIntervalTick();                            // 1Hz：等待下次倒计时 / 自检中已执行计时
    void onCheckerFinished(bool success,
                           SH85SelfChecker::Result result,
                           const QString& message,
                           const QString& masterId);
    void onCheckerCountdown(int remainingSeconds, const QString& masterId);
    void onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void onCommandCompleted(ModbusCommand cmd, const QString& masterId);

private:
    // ---- 状态切换 ----
    void enterStopped();
    void enterWaitingNext();
    void enterChecking();

    // ---- 一轮控制 ----
    void beginRound();                                // 启动一轮：并行启动所有可用 checker
    void finishDevice(const QString& qrcode,
                      bool success,
                      const QString& description);   // 单设备完成（含未连接设备的直接失败）
    void tryEndRound();                               // 检查是否所有设备已完成，若是则进入 WaitingNext

    // ---- 工具 ----
    void connectChecker(SH85SelfChecker* checker);
    void disconnectAllCheckers();
    static QString currentTimestamp();

private:
    // ---- 配置 ----
    bool      m_enabled    = false;     // 是否启用
    int       m_periodSec  = 5 * 60;    // 默认 5 分钟

    // ---- 状态 ----
    State     m_state              = State::Stopped;
    bool      m_finishedEmitted    = false;
    int       m_intervalRemaining  = 0; // WaitingNext 剩余秒数
    int       m_elapsedSeconds     = 0; // Checking 已执行秒数

    // ---- 1Hz 定时器（仅 WaitingNext 启用；Checking 时复用计时已执行秒数）----
    QTimer*   m_tickTimer = nullptr;

    // ---- 一轮上下文 ----
    QString                       m_roundStartTime;
    QStringList                   m_roundOrderedQrcodes;     // 项目所有 qrcode（按顺序，含未参加）
    QSet<QString>                 m_pendingQrcodes;          // 仍在执行的设备
    QHash<QString, DeviceResult>  m_roundResults;            // 每设备结果

    // ---- 信号连接（用于 stop/解除）----
    QList<QMetaObject::Connection> m_checkerConnections;
};

Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask::TimeUnit)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask::State)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask::DeviceResult)
Q_DECLARE_METATYPE(SH85PeriodicSelfCheckTask::SelfCheckSummary)

#endif // SH85_PERIODIC_SELF_CHECK_TASK_H
