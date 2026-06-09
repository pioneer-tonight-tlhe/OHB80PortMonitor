#ifndef VEFC_SENSOR_MONITOR_TASK_H
#define VEFC_SENSOR_MONITOR_TASK_H

#include "../../scheduler_task.h"
#include "vefc_sensor_monitor_device_selector.h"
#include "vefc_sensor_monitor_log_service.h"
#include "vefc_sensor_monitor_round_context.h"
#include "vefc_sensor_monitor_round_runner.h"
#include "vefc_sensor_monitor_types.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

class QTimer;

class VEFCSensorMonitorTask : public SchedulerTask
{
    Q_OBJECT

public:
    enum class State {
        Stopped,
        Monitoring,
        WaitingNext
    };
    Q_ENUM(State)

    explicit VEFCSensorMonitorTask(QObject* parent = nullptr);
    ~VEFCSensorMonitorTask() override;

    void start() override;
    void stop() override;

    QString taskType() const override { return QStringLiteral("VEFCSensorMonitorTask"); }
    bool isPersistent() const override { return true; }
    bool isRecurring() const override { return true; }
    int intervalMs() const override { return kIntervalMs; }

    State currentState() const { return m_taskState; }
    QString currentStateText() const;
    static QString stateToString(State state);

    QStringList filterAvailableDevices() const;

signals:
    void roundStarted(const QString& roundId, int totalCount);
    void roundFinished(const QString& roundId,
                       int totalCount,
                       int persistedCount,
                       int failedCount,
                       int skippedCount);
    void recordPersisted(const QString& qrCode, const VEFCSensorMonitorRecord& record);
    void commandCompleted(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);
    void taskStateChanged(VEFCSensorMonitorTask::State state);
    void elapsedTick(int elapsedSeconds);
    void intervalCountdown(int remainingSeconds);
    void allFinished(const VEFCSensorMonitor::RoundSummary& summary);

private slots:
    void onPeriodTimeout();
    void onElapsedTimerTick();
    void onIntervalCountdownTick();
    void onRunnerCommandFinished(ModbusCommand cmd, const QString& masterId);
    void onRunnerCommandRetrying(ModbusCommand cmd, const QString& masterId);

private:
    static constexpr int kIntervalMs = 60 * 1000;
    static constexpr const char* kReadPressureCmdId = "ReadVEFCPressure";
    static constexpr const char* kReadTemperatureCmdId = "ReadVEFCTemperature";

    void initPeriodTimer();
    void initRoundRunner();

    void startMonitorRound();
    void processDeviceInspection(const VEFCSensorMonitor::DeviceInspection& inspection);
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
    void tryFinishRound();
    void enterTaskState(State state);
    void startIntervalCountdown();
    void stopIntervalCountdown();

    static QString currentTimestamp();
    static QString roundIdFromTimestamp(qint64 timestamp);
    static QString commandTypeName(VEFCSensorMonitor::SensorCommandType type);
    static QString commandFailureReason(const ModbusCommand& cmd);
    static void appendFailureReason(VEFCSensorMonitor::DeviceRoundState& state, const QString& reason);

private:
    QTimer* m_periodTimer = nullptr;
    QTimer* m_elapsedTimer = nullptr;
    QTimer* m_intervalCountdownTimer = nullptr;

    bool m_stopped = true;
    qint64 m_startedMs = 0;
    qint64 m_nextTriggerDeadlineMs = 0;
    int m_elapsedSeconds = 0;
    int m_intervalRemainingSeconds = 0;
    int m_roundIndex = 0;
    State m_taskState = State::Stopped;

    VEFCSensorMonitorRoundContext m_roundContext;
    VEFCSensorMonitorDeviceSelector m_deviceSelector;
    VEFCSensorMonitorLogService m_logService;
    VEFCSensorMonitorRoundRunner* m_roundRunner = nullptr;
};

Q_DECLARE_METATYPE(VEFCSensorMonitorTask::State)

#endif // VEFC_SENSOR_MONITOR_TASK_H
