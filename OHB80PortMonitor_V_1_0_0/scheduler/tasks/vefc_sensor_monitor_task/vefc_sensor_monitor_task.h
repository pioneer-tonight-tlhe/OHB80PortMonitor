#ifndef VEFC_SENSOR_MONITOR_TASK_H
#define VEFC_SENSOR_MONITOR_TASK_H

#include "../../scheduler_task.h"
#include "vefc_sensor_monitor_daily_stats.h"
#include "vefc_sensor_monitor_device_selector.h"
#include "vefc_sensor_monitor_log_service.h"
#include "vefc_sensor_monitor_round_context.h"
#include "vefc_sensor_monitor_round_runner.h"
#include "vefc_sensor_monitor_types.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QDate>
#include <QHash>
#include <QSet>
#include <QTime>

class QTimer;

// ====================================================================
// VEFCSensorMonitorTask - VEFC 传感器监控调度任务
//
// 设计目标：
//   1. 任务类只负责周期调度、轮次收口、状态切换和对外信号转发。
//   2. 设备筛选、轮次上下文、执行器、日志服务分别拆到独立模块，避免调度类继续膨胀。
//   3. 保持对外任务类型、周期触发方式和“单设备两条指令成功后立即落库”的行为稳定。
// ====================================================================
class VEFCSensorMonitorTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ==================================== 公共状态类型 ====================================
    enum class State {
        Stopped,
        Monitoring,
        WaitingNext
    };
    Q_ENUM(State)

    // ==================================== 构造函数 ====================================
    explicit VEFCSensorMonitorTask(QObject* parent = nullptr);
    ~VEFCSensorMonitorTask() override;

    // ============================ 基类 SchedulerTask 相关接口 ============================
    void start() override;
    void stop() override;

    QString taskType() const override { return QStringLiteral("VEFCSensorMonitorTask"); }
    bool isPersistent() const override { return true; }
    bool isRecurring() const override { return true; }
    int intervalMs() const override { return kIntervalMs; }

    State currentState() const { return m_taskState; }
    QString currentStateText() const;
    static QString stateToString(State state);

    // ==================================== 设备筛选辅助查询 ====================================
    // 供 UI 或调试场景查看当前可提交 VEFC 监控指令的设备列表。
    QStringList filterAvailableDevices() const;

signals:
    // ---- 轮次级信号：供日志界面、调试页面或后续报表模块订阅 ----
    void roundStarted(const QString& roundId, int totalCount);
    void roundFinished(const QString& roundId,
                       int totalCount,
                       int persistedCount,
                       int failedCount,
                       int skippedCount);

    // ---- 设备级信号：供后续设备明细视图或数据联动使用 ----
    void recordPersisted(const QString& qrCode, const VEFCSensorMonitorRecord& record);
    void commandCompleted(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);

    // ---- 任务状态信号：供 UI 显示运行状态、耗时和下轮倒计时 ----
    void taskStateChanged(VEFCSensorMonitorTask::State state);
    void elapsedTick(int elapsedSeconds);
    void intervalCountdown(int remainingSeconds);
    void allFinished(const VEFCSensorMonitor::RoundSummary& summary);

private slots:
    // 周期定时器与执行器回调统一在 Task 中收口，再决定业务动作。
    void onPeriodTimeout();
    void onElapsedTimerTick();
    void onIntervalCountdownTick();
    void onDailyStatsTimerTick();
    void onRunnerCommandFinished(ModbusCommand cmd, const QString& masterId);
    void onRunnerCommandRetrying(ModbusCommand cmd, const QString& masterId);

private:
    // ---- 固定调度参数：当前默认每 60 秒执行一轮 VEFC 监控 ----
    static constexpr int kIntervalMs = 60 * 1000;
    static constexpr int kDailyStatsCheckIntervalMs = 60 * 1000;
    static constexpr const char* kReadPressureCmdId = "ReadVEFCPressure";
    static constexpr const char* kReadTemperatureCmdId = "ReadVEFCTemperature";

    // 初始化任务级定时器与执行器。
    void initPeriodTimer();
    void initDailyStatsTimer();
    void initRoundRunner();

    // 初始化新一轮、筛选设备、提交两条业务指令，并在空轮次时立即收口。
    void startMonitorRound();
    void processDeviceInspection(const VEFCSensorMonitor::DeviceInspection& inspection);

    // 单设备结果统一入口：成功解析、失败收口和提交失败都走这些方法。
    void completeDeviceCommand(const QString& qrCode,
                               VEFCSensorMonitor::SensorCommandType type,
                               const ModbusCommand& cmd);
    void failDeviceCommand(const QString& qrCode,
                           VEFCSensorMonitor::SensorCommandType type,
                           const ModbusCommand& cmd,
                           const QString& reason);
    void markSubmitFailure(const QString& qrCode,
                           VEFCSensorMonitor::SensorCommandType type,
                           const QString& reason);
    void persistDeviceRecord(VEFCSensorMonitor::DeviceRoundState& state);

    // 当 pending command 清空后，统一生成整轮汇总并切换到等待状态。
    void tryFinishRound();
    void enterTaskState(State state);
    void startIntervalCountdown();
    void stopIntervalCountdown();
    void logSoftwareFirstOpenRecordIfNeeded(const VEFCSensorMonitor::DeviceRoundState& state);

    // 每天凌晨统计前一天原始记录，并写入同一个寿命日统计日志文件。
    void tryWriteDailyStats();
    void writeDailyStatsForDate(const QDate& statDate);

    // 轮次辅助方法：生成时间字符串、轮次 ID 与统一失败原因。
    static QString currentTimestamp();
    static QString roundIdFromTimestamp(qint64 timestamp);
    static QTime dailyStatsTriggerTime();
    static QString commandTypeName(VEFCSensorMonitor::SensorCommandType type);
    static QString commandFailureReason(const ModbusCommand& cmd);
    static void appendFailureReason(VEFCSensorMonitor::DeviceRoundState& state, const QString& reason);

private:
    // ---- 周期触发控制 ----
    QTimer* m_periodTimer = nullptr;
    QTimer* m_elapsedTimer = nullptr;
    QTimer* m_intervalCountdownTimer = nullptr;
    QTimer* m_dailyStatsTimer = nullptr;

    bool m_stopped = true;
    qint64 m_startedMs = 0;
    qint64 m_nextTriggerDeadlineMs = 0;
    QDate m_lastDailyStatsDate;
    int m_elapsedSeconds = 0;
    int m_intervalRemainingSeconds = 0;
    int m_roundIndex = 0;
    State m_taskState = State::Stopped;
    QSet<QString> m_softwareFirstOpenLoggedQrcodes;
    QHash<QString, VEFCSensorMonitorRecord> m_softwareFirstOpenRecords;

    // ---- 拆分后的功能模块 ----
    VEFCSensorMonitorRoundContext m_roundContext;
    VEFCSensorMonitorDeviceSelector m_deviceSelector;
    VEFCSensorMonitorLogService m_logService;
    VEFCSensorMonitorDailyStatsService m_dailyStatsService;
    VEFCSensorMonitorRoundRunner* m_roundRunner = nullptr;
};

Q_DECLARE_METATYPE(VEFCSensorMonitorTask::State)

#endif // VEFC_SENSOR_MONITOR_TASK_H
