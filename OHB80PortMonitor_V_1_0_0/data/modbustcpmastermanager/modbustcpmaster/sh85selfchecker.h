#ifndef SH85_SELF_CHECKER_H
#define SH85_SELF_CHECKER_H

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

class ModbusTcpMaster;

// ============================================================
// SH85SelfChecker - SH85 湿度传感器自检器（数据层 / 设备子控件）
//
// 设计说明：
//   - 作为 ModbusTcpMaster 的子控件存在，由 master 创建并持有；
//     与 connector / sender / periodicSender 等子控件保持一致的获取方式
//     （master->selfChecker()）。
//   - 通过状态机驱动整套自检流程；不依赖外部 SchedulerTask，可被
//     业务侧/调度任务/UI 任意线程访问（Qt 信号槽默认排队保证线程安全）。
//   - 完整流程（共约 70 秒）：
//       1. 下发 StartSelfCheck（不允许超时重发）
//          1.1 失败 → errorOccurred(StartCommandFailed)
//       2. 等待 5s，下发 ReadSelfCheckStatus
//          2.1 失败 → errorOccurred(ReadEarlyCommandFailed)
//          2.2 CH_1 == 0  → errorOccurred(DeviceNotEntered)
//          2.3 CH_1 != 0 && CH_1 != 1 → errorOccurred(FirmwareAbnormal)
//          2.4 CH_1 == 1  → 进入步骤 3
//       3. 等待 55s，开始轮询 ReadSelfCheckStatus（窗口 10s）
//          3.1 失败 → errorOccurred(ReadPollCommandFailed)
//          3.2 CH_1 == 0  → errorOccurred(FirmwareAbnormal)
//          3.3 CH_1 == 1  → 继续轮询
//          3.4 CH_1 == 2  → 自检成功 finished(Success)
//          3.5 CH_1 == 3  → errorOccurred(HumidityExceeded)
//          3.6 CH_1 == 4  → errorOccurred(SensorCommError)
//          3.7 CH_1 == 5  → errorOccurred(ThresholdParamError)
//       4. 10s 轮询窗口超时 → errorOccurred(Timeout)
//
// 信号说明：
//   - errorOccurred：发出错误（任何失败原因均同步发出此信号），携带 Result 与文字描述
//   - finished     ：自检流程结束（含成功 / 失败 / 取消），UI/业务可据此恢复按钮等
//   - stateChanged ：内部状态机状态变更，便于调试和日志追踪
// ============================================================
class SH85SelfChecker : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 状态机状态
     */
    enum class State {
        Idle,                  ///< 空闲，未启动
        StartingSelfCheck,     ///< 已下发 StartSelfCheck，等待响应
        WaitingPhase1,         ///< StartSelfCheck 成功，等待 5s 计时
        ReadingStatusEarly,    ///< 已下发阶段 1 的 ReadSelfCheckStatus，等待响应
        WaitingPhase2,         ///< 阶段 1 通过，等待 55s 计时
        PollingStatus,         ///< 进入 10s 轮询窗口
        Done                   ///< 流程结束（成功/失败/取消）
    };
    Q_ENUM(State)

    /**
     * @brief 自检结果
     */
    enum class Result {
        Success,                  ///< 自检成功（CH_1 == 2）
        StartCommandFailed,       ///< StartSelfCheck 指令下发失败
        ReadEarlyCommandFailed,   ///< 阶段 1 ReadSelfCheckStatus 下发失败
        DeviceNotEntered,         ///< 阶段 1 CH_1 == 0，设备未进入自检
        FirmwareAbnormal,         ///< 底层固件异常（CH_1 非预期值）
        ReadPollCommandFailed,    ///< 阶段 2 轮询 ReadSelfCheckStatus 下发失败
        HumidityExceeded,         ///< CH_1 == 3，湿度超标失败
        SensorCommError,          ///< CH_1 == 4，SH85 传感器通讯故障
        ThresholdParamError,      ///< CH_1 == 5，阈值参数错误（湿度下限阈值 <= 0）
        Timeout,                  ///< 10s 轮询窗口仍未拿到终态值
        Cancelled                 ///< 外部调用 stop() 主动取消
    };
    Q_ENUM(Result)

    /// 阶段 1 等待时长（毫秒）：StartSelfCheck 成功 → 下发 ReadSelfCheckStatus
    static constexpr int kPhase1WaitMs = 5000;
    /// 阶段 2 等待时长（毫秒）：阶段 1 通过 → 开始轮询
    static constexpr int kPhase2WaitMs = 55000;
    /// 阶段 2 轮询窗口时长（毫秒）
    static constexpr int kPollWindowMs = 10000;
    /// 阶段 2 轮询间隔（毫秒）：每次响应回来后再次下发的最小间隔
    static constexpr int kPollIntervalMs = 1000;
    /// 整体自检总时长（毫秒）= 阶段 1 + 阶段 2 + 轮询窗口
    static constexpr int kTotalDurationMs = kPhase1WaitMs + kPhase2WaitMs + kPollWindowMs; // 70000
    /// 倒计时信号发出间隔（1Hz）
    static constexpr int kCountdownTickMs = 1000;

    /**
     * @brief 构造函数
     * @param master 所属的 ModbusTcpMaster（必须非空，作为 parent）
     * @param parent 父对象
     */
    explicit SH85SelfChecker(ModbusTcpMaster* master, QObject* parent = nullptr);
    ~SH85SelfChecker() override;

    /**
     * @brief 启动自检流程
     * @return 启动是否成功（前置条件不满足时立即返回 false 且不发任何信号）
     * @details 前置条件：
     *           - master 已连接
     *           - 指令池中存在 StartSelfCheck / ReadSelfCheckStatus 模板
     *           - 当前状态为 Idle / Done
     */
    bool start();

    /**
     * @brief 主动取消自检流程
     * @details 取消后将发出 finished 信号（result = Cancelled），不发 errorOccurred
     */
    void stop();

    /**
     * @brief 当前是否正在自检
     */
    bool isRunning() const { return m_state != State::Idle && m_state != State::Done; }

    /**
     * @brief 获取当前状态
     */
    State currentState() const { return m_state; }

    /// 状态枚举转中文字符串（用于日志）
    static QString stateToString(State s);
    /// 结果枚举转字符串（用于日志和信号文案）
    static QString resultToString(Result r);

signals:
    /**
     * @brief 自检流程已启动
     */
    void started(const QString& masterId);

    /**
     * @brief 整体倒计时（1s），自检启动后开始发出
     * @param remainingSeconds 剩余秒数（70 → 0）
     * @param masterId         设备 ID
     */
    void countdownTick(int remainingSeconds, const QString& masterId);

    /**
     * @brief 内部状态变更
     */
    void stateChanged(SH85SelfChecker::State state, const QString& masterId);

    /**
     * @brief 每条 Modbus 指令完成后发出（仅本 checker 下发的指令）
     *        供 Task 层订阅后写入 communicatelogdb，不在 Data 层直接操作数据库
     */
    void commandCompleted(ModbusCommand cmd, const QString& masterId);

    /**
     * @brief 指令超时且将要重试时发出（仅本 checker 下发的指令）
     *        供 Task 层记录每一次实际发送过的指令帧。
     */
    void commandRetrying(ModbusCommand cmd, const QString& masterId);

    /**
     * @brief 错误信号（任意阶段失败均会发出）
     */
    void errorOccurred(SH85SelfChecker::Result result,
                       const QString& message,
                       const QString& masterId);

    /**
     * @brief 自检流程结束（成功 / 失败 / 取消都会发出）
     */
    void finished(bool success,
                  SH85SelfChecker::Result result,
                  const QString& message,
                  const QString& masterId);

private slots:
    /// 接收来自 master->sender() 的指令完成信号
    void onCommandFinished(ModbusCommand cmd, const QString& masterId);
    /// 接收来自 master->sender() 的指令超时重试信号
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString& masterId);
    /// 阶段 1 等待 5s 到期
    void onPhase1WaitElapsed();
    /// 阶段 2 等待 55s 到期
    void onPhase2WaitElapsed();
    /// 轮询窗口 10s 到期
    void onPollWindowElapsed();
    /// 1Hz 倒计时 tick
    void onCountdownTick();
    /// 在对象所属线程上启动倒计时定时器（由 start() 通过 invokeMethod 派发）
    void startCountdownTimer();

private:
    /// 下发 StartSelfCheck（maxRetryCount = 0，不允许超时重发）
    bool submitStartSelfCheck();
    /// 下发 ReadSelfCheckStatus
    bool submitReadStatus();

    /// 从响应 registerValue 解析 CH_1（2 字节大端）
    quint16 parseStatusValue(const ModbusCommand& cmd) const;

    /// 进入新状态（仅在变化时发出 stateChanged 并写日志）
    void enterState(State s);

    /// 终结流程：发出 errorOccurred（result != Success）+ finished
    void emitErrorAndFinish(Result r, const QString& msg);
    /// 终结流程：仅发出 finished（用于 Success / Cancelled）
    void finishOnly(bool success, Result r, const QString& msg);

    /// 通用清理：停止所有定时器，断开 sender 信号槽
    void cleanup();

    ModbusTcpMaster* m_master = nullptr;       // 所属 master
    State            m_state  = State::Idle;
    qint64           m_pendingUuid = 0;        // 当前等待响应的指令 uuid（0 表示无）

    QTimer*          m_phase1Timer     = nullptr; // 5s
    QTimer*          m_phase2Timer     = nullptr; // 55s
    QTimer*          m_pollWindowTimer = nullptr; // 10s 轮询窗口
    QTimer*          m_countdownTimer  = nullptr; // 1Hz 倒计时
    QElapsedTimer    m_elapsed;                   // 流程经过时间

    QMetaObject::Connection m_senderConn;         // sender::commandFinished 连接句柄
    QMetaObject::Connection m_senderRetryConn;    // sender::commandTimeoutRetry 连接句柄

    bool             m_finished = false;        // 防止重复 emit finished
};

#endif // SH85_SELF_CHECKER_H
