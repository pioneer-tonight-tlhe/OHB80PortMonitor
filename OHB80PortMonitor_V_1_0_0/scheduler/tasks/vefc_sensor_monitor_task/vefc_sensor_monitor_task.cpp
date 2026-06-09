#include "vefc_sensor_monitor_task.h"

#include "data/logdatabases/databasemanager.h"
#include "data/logdatabases/vefcsensormonitordb/vefcsensormonitordbcon.h"
#include "data/modbustcpmastermanager/modbuscommand/commandpool.h"
#include "data/modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "data/modbustcpmastermanager/modbustcpmastermanager.h"

#include <QDateTime>
#include <QTimer>

// ====================================================================
// VEFCSensorMonitorTask - 调度任务实现
//
// 说明：
//   1. 本文件只保留调度壳职责：周期触发、轮次开始/结束、状态切换和对外信号转发。
//   2. 设备筛选、执行器、轮次上下文和日志格式化均已拆分到独立文件。
// ====================================================================
VEFCSensorMonitorTask::VEFCSensorMonitorTask(QObject* parent)
    : SchedulerTask(parent)
{
    qRegisterMetaType<VEFCSensorMonitorTask::State>("VEFCSensorMonitorTask::State");
    qRegisterMetaType<VEFCSensorMonitorRecord>("VEFCSensorMonitorRecord");
    qRegisterMetaType<VEFCSensorMonitor::RoundSummary>("VEFCSensorMonitor::RoundSummary");

    initPeriodTimer();
    initRoundRunner();
    m_logService.writeTaskConstructed();
}

VEFCSensorMonitorTask::~VEFCSensorMonitorTask()
{
    stop();
}

void VEFCSensorMonitorTask::start()
{
    // start() 负责重置任务级状态，并立即触发首轮 VEFC 监控。
    m_stopped = false;
    m_startedMs = QDateTime::currentMSecsSinceEpoch();
    m_nextTriggerDeadlineMs = m_startedMs + kIntervalMs;
    m_elapsedSeconds = 0;
    m_intervalRemainingSeconds = 0;
    m_roundIndex = 0;

    setState(Running);
    enterTaskState(State::Monitoring);

    m_roundContext.clear();
    m_roundRunner->connectAllSenders();
    m_roundRunner->clearPendingCommands();

    if (m_elapsedTimer) {
        m_elapsedTimer->start();
    }
    if (m_periodTimer) {
        m_periodTimer->start();
    }

    m_logService.writeTaskStarted(kIntervalMs, currentTimestamp());
    startMonitorRound();
}

void VEFCSensorMonitorTask::stop()
{
    // stop() 统一停止定时器、断开执行器并清空轮次上下文。
    const bool wasRunning = (state() == Running)
        || (m_periodTimer && m_periodTimer->isActive())
        || m_roundContext.isActive();

    m_stopped = true;

    if (m_periodTimer) {
        m_periodTimer->stop();
    }
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }
    stopIntervalCountdown();

    if (m_roundRunner) {
        m_roundRunner->clearPendingCommands();
        m_roundRunner->disconnectAllSenders();
    }
    m_roundContext.clear();

    enterTaskState(State::Stopped);

    if (wasRunning) {
        setState(Cancelled);
        emit finished(false, QStringLiteral("VEFC sensor monitor task stopped"));
        const qint64 stoppedMs = QDateTime::currentMSecsSinceEpoch();
        m_logService.writeTaskStopped(m_startedMs > 0 ? stoppedMs - m_startedMs : 0,
                                      QDateTime::fromMSecsSinceEpoch(stoppedMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
    }
}

QString VEFCSensorMonitorTask::currentStateText() const
{
    return stateToString(m_taskState);
}

QString VEFCSensorMonitorTask::stateToString(State state)
{
    switch (state) {
    case State::Stopped:
        return QStringLiteral("Stopped");
    case State::Monitoring:
        return QStringLiteral("Monitoring");
    case State::WaitingNext:
        return QStringLiteral("WaitingNext");
    }
    return QStringLiteral("Unknown");
}

QStringList VEFCSensorMonitorTask::filterAvailableDevices() const
{
    return m_deviceSelector.filterAvailableDevices();
}

void VEFCSensorMonitorTask::onPeriodTimeout()
{
    if (m_stopped) {
        return;
    }

    // 当前轮次未收口时，本次触发只记录日志并重新进入倒计时，不强行并发开启新轮次。
    m_nextTriggerDeadlineMs = QDateTime::currentMSecsSinceEpoch() + kIntervalMs;
    stopIntervalCountdown();

    if (m_roundContext.isActive() || m_roundRunner->hasPendingCommands()) {
        m_logService.writeTriggerSkipped(m_roundContext.roundId(), m_roundRunner->pendingCount());
        startIntervalCountdown();
        return;
    }

    startMonitorRound();
}

void VEFCSensorMonitorTask::onElapsedTimerTick()
{
    ++m_elapsedSeconds;
    emit elapsedTick(m_elapsedSeconds);
}

void VEFCSensorMonitorTask::onIntervalCountdownTick()
{
    if (m_intervalRemainingSeconds <= 0) {
        stopIntervalCountdown();
        emit intervalCountdown(0);
        return;
    }

    --m_intervalRemainingSeconds;
    emit intervalCountdown(m_intervalRemainingSeconds);
}

void VEFCSensorMonitorTask::onRunnerCommandFinished(ModbusCommand cmd, const QString& masterId)
{
    emit commandCompleted(cmd, masterId);

    if (m_stopped || !m_roundContext.isActive()) {
        return;
    }
    if (!m_roundRunner->containsPendingCommand(cmd.uuid)) {
        return;
    }

    const VEFCSensorMonitor::PendingCommand pending = m_roundRunner->takePendingCommand(cmd.uuid);
    const bool commandOk = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;
    if (!commandOk) {
        failDeviceCommand(pending.qrCode, pending.type, cmd, commandFailureReason(cmd));
        tryFinishRound();
        return;
    }

    completeDeviceCommand(pending.qrCode, pending.type, cmd);
    tryFinishRound();
}

void VEFCSensorMonitorTask::onRunnerCommandRetrying(ModbusCommand cmd, const QString& masterId)
{
    emit commandRetrying(cmd, masterId);
    Q_UNUSED(masterId)

    if (!m_roundRunner->containsPendingCommand(cmd.uuid)) {
        return;
    }

    const VEFCSensorMonitor::PendingCommand pending = m_roundRunner->pendingCommand(cmd.uuid);
    m_logService.writeCommandRetrying(m_roundContext.roundId(), pending.qrCode, cmd);
}

void VEFCSensorMonitorTask::initPeriodTimer()
{
    m_periodTimer = new QTimer(this);
    m_periodTimer->setInterval(kIntervalMs);
    connect(m_periodTimer, &QTimer::timeout, this, &VEFCSensorMonitorTask::onPeriodTimeout);

    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout, this, &VEFCSensorMonitorTask::onElapsedTimerTick);

    m_intervalCountdownTimer = new QTimer(this);
    m_intervalCountdownTimer->setInterval(1000);
    connect(m_intervalCountdownTimer, &QTimer::timeout, this, &VEFCSensorMonitorTask::onIntervalCountdownTick);
}

void VEFCSensorMonitorTask::initRoundRunner()
{
    m_roundRunner = new VEFCSensorMonitorRoundRunner(this);
    connect(m_roundRunner, &VEFCSensorMonitorRoundRunner::commandFinished,
            this, &VEFCSensorMonitorTask::onRunnerCommandFinished,
            Qt::QueuedConnection);
    connect(m_roundRunner, &VEFCSensorMonitorRoundRunner::commandRetrying,
            this, &VEFCSensorMonitorTask::onRunnerCommandRetrying,
            Qt::QueuedConnection);
}

void VEFCSensorMonitorTask::startMonitorRound()
{
    if (m_stopped) {
        return;
    }

    // 每轮统一生成 roundId / recordTimestamp，并由 RoundContext 建立默认状态。
    ++m_roundIndex;
    const qint64 roundTimestamp = QDateTime::currentMSecsSinceEpoch();
    const QString roundId = roundIdFromTimestamp(roundTimestamp);
    const QString startTime = currentTimestamp();
    const QStringList qrcodes = m_deviceSelector.targetQrcodes();

    enterTaskState(State::Monitoring);
    m_roundContext.beginRound(roundId, roundTimestamp, startTime, qrcodes);
    emit roundStarted(roundId, qrcodes.size());
    m_logService.writeRoundStarted(roundId, startTime, roundTimestamp, qrcodes.size());

    CommandPool* pool = ModbusTcpMasterManager::instance().commandPool();
    if (!pool || !pool->contains(kReadPressureCmdId) || !pool->contains(kReadTemperatureCmdId)) {
        m_logService.writeRoundCancelled(roundId, QStringLiteral("VEFC monitor commands are missing in CommandPool"));
        m_roundContext.clear();
        enterTaskState(State::WaitingNext);
        startIntervalCountdown();
        return;
    }

    // 设备前置条件检查统一交给 Selector；Task 只消费检查结果。
    const QList<VEFCSensorMonitor::DeviceInspection> inspections = m_deviceSelector.inspectTargets();
    for (const VEFCSensorMonitor::DeviceInspection& inspection : inspections) {
        processDeviceInspection(inspection);
    }

    tryFinishRound();
}

void VEFCSensorMonitorTask::processDeviceInspection(const VEFCSensorMonitor::DeviceInspection& inspection)
{
    VEFCSensorMonitor::DeviceRoundState* state = m_roundContext.mutableState(inspection.qrCode);
    if (!state) {
        return;
    }

    // 本轮固定记录 FoupOfOHBInfo 中的进气压力和实际流量。
    state->record.qrCode = inspection.qrCode;
    state->record.recordTimestamp = m_roundContext.recordTimestamp();
    state->record.gasPressure = inspection.gasPressure;
    state->record.actualFlow = inspection.actualFlow;

    // 前置条件不满足时，直接将该设备标记为 skipped。
    if (!inspection.canSubmitCommands()) {
        state->skipped = true;
        state->pressureFinished = true;
        state->temperatureFinished = true;
        state->failReason = inspection.unavailableReason;
        m_logService.writeDeviceSkipped(m_roundContext.roundId(),
                                        inspection.qrCode,
                                        inspection.unavailableReason,
                                        state->record.recordTimeString());
        return;
    }

    // 正常情况下，每台设备提交两条业务指令：压力与温度。
    const bool pressureSubmitted = m_roundRunner->submitCommand(inspection.qrCode,
                                                                VEFCSensorMonitor::SensorCommandType::Pressure,
                                                                kReadPressureCmdId);
    if (!pressureSubmitted) {
        markSubmitFailure(inspection.qrCode,
                          VEFCSensorMonitor::SensorCommandType::Pressure,
                          QStringLiteral("ReadVEFCPressure submit failed"));
    }

    const bool temperatureSubmitted = m_roundRunner->submitCommand(inspection.qrCode,
                                                                   VEFCSensorMonitor::SensorCommandType::Temperature,
                                                                   kReadTemperatureCmdId);
    if (!temperatureSubmitted) {
        markSubmitFailure(inspection.qrCode,
                          VEFCSensorMonitor::SensorCommandType::Temperature,
                          QStringLiteral("ReadVEFCTemperature submit failed"));
    }
}

void VEFCSensorMonitorTask::completeDeviceCommand(const QString& qrCode,
                                                  VEFCSensorMonitor::SensorCommandType type,
                                                  const ModbusCommand& cmd)
{
    VEFCSensorMonitor::DeviceRoundState* state = m_roundContext.mutableState(qrCode);
    if (!state) {
        return;
    }

    // 成功收到响应后，统一通过 CommandResponseParser 提取传感器值。
    const QVariantMap data = CommandResponseParser::instance().parse(cmd);
    if (type == VEFCSensorMonitor::SensorCommandType::Pressure) {
        state->pressureFinished = true;
        if (data.contains(QStringLiteral("sensorPressure"))) {
            state->pressureOk = true;
            state->record.sensorPressure = data.value(QStringLiteral("sensorPressure")).toDouble();
            m_logService.writeCommandSucceeded(m_roundContext.roundId(), qrCode, cmd);
        } else {
            state->pressureOk = false;
            appendFailureReason(*state, QStringLiteral("ReadVEFCPressure parse failed"));
            m_logService.writeCommandFailed(m_roundContext.roundId(), qrCode, cmd,
                                            QStringLiteral("ReadVEFCPressure parse failed"));
        }
    } else {
        state->temperatureFinished = true;
        if (data.contains(QStringLiteral("sensorTemperature"))) {
            state->temperatureOk = true;
            state->record.sensorTemperature = data.value(QStringLiteral("sensorTemperature")).toDouble();
            m_logService.writeCommandSucceeded(m_roundContext.roundId(), qrCode, cmd);
        } else {
            state->temperatureOk = false;
            appendFailureReason(*state, QStringLiteral("ReadVEFCTemperature parse failed"));
            m_logService.writeCommandFailed(m_roundContext.roundId(), qrCode, cmd,
                                            QStringLiteral("ReadVEFCTemperature parse failed"));
        }
    }

    // 只有压力和温度两条业务指令都成功后，才允许立刻落库。
    if (state->pressureOk && state->temperatureOk && !state->persisted) {
        persistDeviceRecord(*state);
    }
}

void VEFCSensorMonitorTask::failDeviceCommand(const QString& qrCode,
                                              VEFCSensorMonitor::SensorCommandType type,
                                              const ModbusCommand& cmd,
                                              const QString& reason)
{
    VEFCSensorMonitor::DeviceRoundState* state = m_roundContext.mutableState(qrCode);
    if (!state) {
        return;
    }

    if (type == VEFCSensorMonitor::SensorCommandType::Pressure) {
        state->pressureFinished = true;
        state->pressureOk = false;
    } else {
        state->temperatureFinished = true;
        state->temperatureOk = false;
    }

    appendFailureReason(*state, QStringLiteral("%1: %2").arg(cmd.id, reason));
    m_logService.writeCommandFailed(m_roundContext.roundId(), qrCode, cmd, reason);
}

void VEFCSensorMonitorTask::markSubmitFailure(const QString& qrCode,
                                              VEFCSensorMonitor::SensorCommandType type,
                                              const QString& reason)
{
    VEFCSensorMonitor::DeviceRoundState* state = m_roundContext.mutableState(qrCode);
    if (!state) {
        return;
    }

    if (type == VEFCSensorMonitor::SensorCommandType::Pressure) {
        state->pressureFinished = true;
        state->pressureOk = false;
    } else {
        state->temperatureFinished = true;
        state->temperatureOk = false;
    }

    appendFailureReason(*state, reason);
}

void VEFCSensorMonitorTask::persistDeviceRecord(VEFCSensorMonitor::DeviceRoundState& state)
{
    if (state.persisted) {
        return;
    }

    // 数据库连接不可用时只记录失败原因，不把该设备误判为成功。
    LogDB::VEFCSensorMonitorDBCon* db = LogDB::DatabaseManager::instance().vefcSensorMonitorCon();
    if (!db) {
        appendFailureReason(state, QStringLiteral("VEFC monitor database is unavailable"));
        m_logService.writePersistFailed(m_roundContext.roundId(), state, state.failReason);
        return;
    }

    db->insertRecord(state.record);
    state.persisted = true;
    emit recordPersisted(state.qrCode, state.record);
    m_logService.writeRecordPersisted(m_roundContext.roundId(), state);
}

void VEFCSensorMonitorTask::tryFinishRound()
{
    if (!m_roundContext.isActive() || m_roundRunner->hasPendingCommands()) {
        return;
    }

    // 只有当本轮 pending command 全部收口后，才生成整轮汇总并切换为等待下轮状态。
    const VEFCSensorMonitor::RoundSummary summary = m_roundContext.buildSummary(currentTimestamp());
    m_logService.writeRoundFinished(summary);

    emit roundFinished(summary.roundId,
                       summary.totalCount,
                       summary.persistedCount,
                       summary.failedCount,
                       summary.skippedCount);
    emit allFinished(summary);

    m_roundContext.completeRound();
    m_roundRunner->clearPendingCommands();
    enterTaskState(State::WaitingNext);
    startIntervalCountdown();
}

void VEFCSensorMonitorTask::enterTaskState(State state)
{
    if (m_taskState == state) {
        return;
    }

    m_taskState = state;
    emit taskStateChanged(m_taskState);
}

void VEFCSensorMonitorTask::startIntervalCountdown()
{
    if (!m_intervalCountdownTimer) {
        return;
    }

    // 倒计时基于“下一次理论触发时间”计算，而不是基于当前时间重新固定 60 秒。
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 remainingMs = qMax<qint64>(0, m_nextTriggerDeadlineMs - now);
    m_intervalRemainingSeconds = static_cast<int>((remainingMs + 999) / 1000);
    emit intervalCountdown(m_intervalRemainingSeconds);
    if (m_intervalRemainingSeconds > 0) {
        m_intervalCountdownTimer->start();
    }
}

void VEFCSensorMonitorTask::stopIntervalCountdown()
{
    if (m_intervalCountdownTimer) {
        m_intervalCountdownTimer->stop();
    }
    m_intervalRemainingSeconds = 0;
}

QString VEFCSensorMonitorTask::currentTimestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

QString VEFCSensorMonitorTask::roundIdFromTimestamp(qint64 timestamp)
{
    return QDateTime::fromMSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
}

QString VEFCSensorMonitorTask::commandTypeName(VEFCSensorMonitor::SensorCommandType type)
{
    return type == VEFCSensorMonitor::SensorCommandType::Pressure
        ? QStringLiteral("ReadVEFCPressure")
        : QStringLiteral("ReadVEFCTemperature");
}

QString VEFCSensorMonitorTask::commandFailureReason(const ModbusCommand& cmd)
{
    QStringList reasons;
    if (cmd.timedOut) {
        reasons << QStringLiteral("timed out");
    }
    if (cmd.checksumError) {
        reasons << QStringLiteral("checksum error");
    }
    if (cmd.deviceBusy) {
        reasons << QStringLiteral("device busy");
    }
    if (!cmd.errorMessage.trimmed().isEmpty()) {
        reasons << cmd.errorMessage.trimmed();
    }
    return reasons.isEmpty() ? QStringLiteral("no response") : reasons.join(QStringLiteral(", "));
}

void VEFCSensorMonitorTask::appendFailureReason(VEFCSensorMonitor::DeviceRoundState& state, const QString& reason)
{
    if (!state.failReason.isEmpty()) {
        state.failReason += QStringLiteral("; ");
    }
    state.failReason += reason;
}
