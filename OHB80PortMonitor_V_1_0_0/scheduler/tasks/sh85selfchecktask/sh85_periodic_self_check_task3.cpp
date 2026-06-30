#include "sh85_periodic_self_check_task3.h"

#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "sh85_self_check_round_runner.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaType>
#include <QTimer>

namespace {
void registerSH85PeriodicSelfCheckTask3MetaTypes()
{
    static const bool registered = []() {
        qRegisterMetaType<SH85PeriodicSelfCheckTask3::TimeUnit>("SH85PeriodicSelfCheckTask3::TimeUnit");
        qRegisterMetaType<SH85PeriodicSelfCheckTask3::State>("SH85PeriodicSelfCheckTask3::State");
        qRegisterMetaType<SH85PeriodicSelfCheckTask3::DeviceResult>("SH85PeriodicSelfCheckTask3::DeviceResult");
        qRegisterMetaType<SH85PeriodicSelfCheckTask3::SelfCheckSummary>("SH85PeriodicSelfCheckTask3::SelfCheckSummary");
        return true;
    }();
    Q_UNUSED(registered)
}
} // namespace

SH85PeriodicSelfCheckTask3::SH85PeriodicSelfCheckTask3(QObject* parent)
    : SchedulerTask(parent)
{
    registerSH85PeriodicSelfCheckTask3MetaTypes();
    initPeriodTimer();
    initRoundRunner();
    m_logService.writeTaskConstructed();
}

SH85PeriodicSelfCheckTask3::~SH85PeriodicSelfCheckTask3()
{
    if (m_roundRunner) {
        m_roundRunner->disconnectAllCheckers();
    }
}

void SH85PeriodicSelfCheckTask3::initPeriodTimer()
{
    m_periodTimer = new QTimer(this);
    m_periodTimer->setSingleShot(true);
    connect(m_periodTimer, &QTimer::timeout,
            this, &SH85PeriodicSelfCheckTask3::onPeriodTimeout);

    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    connect(m_elapsedTimer, &QTimer::timeout,
            this, &SH85PeriodicSelfCheckTask3::onElapsedTimerTick);

    m_intervalCountdownTimer = new QTimer(this);
    m_intervalCountdownTimer->setInterval(1000);
    connect(m_intervalCountdownTimer, &QTimer::timeout,
            this, &SH85PeriodicSelfCheckTask3::onIntervalCountdownTick);
}

void SH85PeriodicSelfCheckTask3::initRoundRunner()
{
    m_roundRunner = new SH85SelfCheckRoundRunner(this);

    // Task3 仍作为对外信号门面，RoundRunner 只负责 checker 信号连接与中继。
    connect(m_roundRunner, &SH85SelfCheckRoundRunner::countdownTick,
            this, &SH85PeriodicSelfCheckTask3::onCheckerCountdownTick,
            Qt::QueuedConnection);
    connect(m_roundRunner, &SH85SelfCheckRoundRunner::stateChanged,
            this, &SH85PeriodicSelfCheckTask3::onCheckerStateChanged,
            Qt::QueuedConnection);
    connect(m_roundRunner, &SH85SelfCheckRoundRunner::finished,
            this, &SH85PeriodicSelfCheckTask3::onCheckerFinished,
            Qt::QueuedConnection);
    connect(m_roundRunner, &SH85SelfCheckRoundRunner::commandCompleted,
            this, &SH85PeriodicSelfCheckTask3::onCheckerCommandCompleted,
            Qt::QueuedConnection);
    connect(m_roundRunner, &SH85SelfCheckRoundRunner::commandRetrying,
            this, &SH85PeriodicSelfCheckTask3::onCheckerCommandRetrying,
            Qt::QueuedConnection);
    connect(m_roundRunner, &SH85SelfCheckRoundRunner::errorOccurred,
            this, &SH85PeriodicSelfCheckTask3::onCheckerErrorOccurred,
            Qt::QueuedConnection);
}

QString SH85PeriodicSelfCheckTask3::currentStateText() const
{
    switch (state()) {
    case SchedulerTask::Pending:   return QStringLiteral("Pending");
    case SchedulerTask::Running:   return QStringLiteral("Running");
    case SchedulerTask::Paused:    return QStringLiteral("Paused");
    case SchedulerTask::Finished:  return QStringLiteral("Finished");
    case SchedulerTask::Failed:    return QStringLiteral("Failed");
    case SchedulerTask::Cancelled: return QStringLiteral("Cancelled");
    }
    return QStringLiteral("Unknown");
}

QString SH85PeriodicSelfCheckTask3::timeUnitToString(TimeUnit unit)
{
    switch (unit) {
    case TimeUnit::Second: return QStringLiteral("s");
    case TimeUnit::Minute: return QStringLiteral("min");
    case TimeUnit::Hour:   return QStringLiteral("hour");
    }
    return QStringLiteral("min");
}

QString SH85PeriodicSelfCheckTask3::stateToString(State state)
{
    switch (state) {
    case State::Stopped:    return QStringLiteral("Stopped");
    case State::Checking:   return QStringLiteral("Checking");
    case State::WaitingNext:return QStringLiteral("WaitingNext");
    }
    return QStringLiteral("Unknown");
}

void SH85PeriodicSelfCheckTask3::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] setEnabled:" << enabled;
    m_logService.writeEnabledChanged(enabled);

    if (!m_enabled) {
        if (m_periodTimer) {
            m_periodTimer->stop();
        }
        stopIntervalCountdown();
        if (!m_roundContext.isActive()) {
            enterTaskState(State::Stopped);
        }
        return;
    }

    if (state() == SchedulerTask::Running
            && !m_roundContext.isActive()
            && (!m_periodTimer || !m_periodTimer->isActive())) {
        onPeriodTimeout();
    }
}

void SH85PeriodicSelfCheckTask3::setPeriod(int value, const QString& unit)
{
    if (value <= 0) {
        value = 1;
    }

    int totalSec = value;
    const QString unitLower = unit.toLower();
    if (unitLower == QStringLiteral("s")) {
        totalSec = value;
    } else if (unitLower == QStringLiteral("min")) {
        totalSec = value * 60;
    } else if (unitLower == QStringLiteral("hour")) {
        totalSec = value * 3600;
    }

    m_periodSec = totalSec;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] setPeriod:" << value
             << unit << "(" << totalSec << "s)";
    m_logService.writePeriodChanged(value, unit, totalSec);
}

void SH85PeriodicSelfCheckTask3::setPeriod(int value, TimeUnit unit)
{
    setPeriod(value, timeUnitToString(unit));
}

void SH85PeriodicSelfCheckTask3::setSingleDevice(const QString& qrcode)
{
    m_singleDeviceMode = !qrcode.isEmpty();
    m_singleDeviceQrcode = qrcode;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] setSingleDevice:" << qrcode
             << ", singleDeviceMode=" << m_singleDeviceMode;
    m_logService.writeSingleDeviceChanged(m_singleDeviceMode, m_singleDeviceQrcode);
}

void SH85PeriodicSelfCheckTask3::start()
{
    setState(SchedulerTask::Running);
    m_logService.writeTaskStarted(m_periodSec, m_singleDeviceMode, m_singleDeviceQrcode);

    if (!m_enabled) {
        qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] start ignored because task is disabled";
        m_logService.writeTaskStartIgnored(QStringLiteral("task disabled"));
        enterTaskState(State::Stopped);
        return;
    }

    // 应用刚启动时设备启用、FOUP、网络状态可能尚未稳定，延迟后再触发首轮。
    m_logService.writeBootDelayScheduled(10);
    QTimer::singleShot(10000, this, [this]() {
        if (state() != SchedulerTask::Running) {
            m_logService.writeBootDelaySkipped(QStringLiteral("task is not running"));
            return;
        }

        if (!m_enabled) {
            m_logService.writeBootDelaySkipped(QStringLiteral("task disabled"));
            return;
        }

        if (m_singleDeviceMode) {
            m_periodRemainingCalls = 1;
        }

        onPeriodTimeout();
    });
}

void SH85PeriodicSelfCheckTask3::stop()
{
    if (m_periodTimer) {
        m_periodTimer->stop();
    }
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }
    stopIntervalCountdown();

    // stop 时若仍有设备在执行，统一标记为 Cancelled 后再尝试结束整轮。
    const QStringList pending = m_roundContext.pendingQrcodes();
    m_logService.writeTaskStopping(pending.size());
    for (const QString& qrcode : pending) {
        finishDevice(qrcode,
                     false,
                     SH85SelfChecker::Result::Cancelled,
                     QStringLiteral("Cancelled by task stop"),
                     -1.0);
    }

    tryFinishRound();

    if (m_roundRunner) {
        m_roundRunner->disconnectAllCheckers();
    }

    setState(SchedulerTask::Finished);
    enterTaskState(State::Stopped);
    m_logService.writeTaskStopped();
    emit finished(true, QStringLiteral("SH85PeriodicSelfCheckTask3 stopped"));
}

void SH85PeriodicSelfCheckTask3::onPeriodTimeout()
{
    if (state() != SchedulerTask::Running || !m_enabled) {
        return;
    }

    // 防止上一轮未结束时叠加启动下一轮，避免 checker 信号串轮。
    if (m_roundContext.isActive() && m_roundContext.hasPendingDevices()) {
        qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask3] previous round is still running, skip trigger"
                   << "roundId=" << m_roundContext.roundId()
                   << "pending=" << m_roundContext.pendingCount();
        m_logService.writeTriggerSkipped(m_roundContext.roundId(), m_roundContext.pendingCount());
        return;
    }

    stopIntervalCountdown();
    m_elapsedSeconds = 0;
    if (m_elapsedTimer) {
        m_elapsedTimer->start();
    }
    enterTaskState(State::Checking);

    startAvailableDeviceChecks();

    if (m_periodRemainingCalls > 0) {
        --m_periodRemainingCalls;
        if (m_periodRemainingCalls == 0 && m_periodTimer) {
            m_periodTimer->stop();
        }
    }
}

void SH85PeriodicSelfCheckTask3::startAvailableDeviceChecks()
{
    if (m_roundRunner) {
        m_roundRunner->disconnectAllCheckers();
    }

    // 新轮次开始前清理执行器连接和本轮缓存，避免残留状态影响统计。
    m_availableDevices.clear();
    m_checkerStates.clear();

    const QList<SH85SelfCheckDeviceSelector::DeviceInspection> inspections =
        m_deviceSelector.inspectTargets(m_singleDeviceMode, m_singleDeviceQrcode);

    QStringList orderedQrcodes;
    orderedQrcodes.reserve(inspections.size());
    for (const SH85SelfCheckDeviceSelector::DeviceInspection& inspection : inspections) {
        orderedQrcodes.append(inspection.qrcode);
    }

    // RoundContext 负责创建设备默认结果，并固定后续汇总输出顺序。
    const QString roundId = currentTimestamp();
    m_roundContext.beginRound(roundId, roundId, orderedQrcodes);

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] round started"
             << "roundId=" << m_roundContext.roundId()
             << "total=" << m_roundContext.totalCount();
    m_logService.writeRoundStarted(m_roundContext.roundId(),
                                   m_roundContext.startTime(),
                                   m_roundContext.totalCount(),
                                   orderedQrcodes);
    emit roundStarted(m_roundContext.roundId(), m_roundContext.totalCount());

    for (const SH85SelfCheckDeviceSelector::DeviceInspection& inspection : inspections) {
        const QString& qrcode = inspection.qrcode;
        m_checkerStates.insert(qrcode, SH85SelfChecker::State::Idle);

        if (!inspection.enabled) {
            m_roundContext.markSkipped(qrcode, QStringLiteral("Device disabled"));
            emit deviceParticipated(qrcode, false);
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] skip disabled device qrcode=" << qrcode;
            m_logService.writeDeviceSkipped(m_roundContext.roundId(),
                                            qrcode,
                                            QStringLiteral("Device disabled"));
            continue;
        }

        if (inspection.foupIn) {
            m_roundContext.markSkipped(qrcode, QStringLiteral("FOUP in place"));
            emit deviceParticipated(qrcode, false);
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] skip FOUP-in device qrcode=" << qrcode;
            m_logService.writeDeviceSkipped(m_roundContext.roundId(),
                                            qrcode,
                                            QStringLiteral("FOUP in place"));
            continue;
        }

        // 从这里开始设备算作已参与本轮；后续启动失败也会按失败结果收口。
        m_roundContext.markParticipating(qrcode);
        emit deviceParticipated(qrcode, true);

        if (!inspection.canStartChecker()) {
            appendNotSubmittedAndFinish(qrcode,
                                        SH85SelfChecker::Result::StartCommandFailed,
                                        inspection.unavailableReason.isEmpty()
                                            ? QStringLiteral("Device not connected")
                                            : inspection.unavailableReason);
            continue;
        }

        SH85SelfChecker* checker = inspection.master->selfChecker();
        if (!checker) {
            appendNotSubmittedAndFinish(qrcode,
                                        SH85SelfChecker::Result::StartCommandFailed,
                                        QStringLiteral("Self-checker is null"));
            continue;
        }

        // 先加入 pending 再启动 checker，确保早到的 queued 信号不会被过滤。
        m_roundContext.markPending(qrcode);

        QString startError;
        if (!m_roundRunner || !m_roundRunner->startDevice(qrcode, checker, &startError)) {
            appendNotSubmittedAndFinish(qrcode,
                                        SH85SelfChecker::Result::StartCommandFailed,
                                        startError.isEmpty()
                                            ? QStringLiteral("Checker start failed")
                                            : startError);
            continue;
        }

        m_logService.writeDeviceStarted(m_roundContext.roundId(), qrcode);
        m_availableDevices.append(qrcode);
    }

    tryFinishRound();
}

QStringList SH85PeriodicSelfCheckTask3::filterAvailableDevices()
{
    m_availableDevices.clear();

    const QList<SH85SelfCheckDeviceSelector::DeviceInspection> inspections =
        m_deviceSelector.inspectTargets(m_singleDeviceMode, m_singleDeviceQrcode);

    for (const SH85SelfCheckDeviceSelector::DeviceInspection& inspection : inspections) {
        if (!inspection.enabled) {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] skip disabled device qrcode="
                     << inspection.qrcode;
            continue;
        }

        if (inspection.foupIn) {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] skip FOUP-in device qrcode="
                     << inspection.qrcode;
            continue;
        }

        if (!inspection.connected) {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] skip disconnected device qrcode="
                     << inspection.qrcode;
            continue;
        }

        m_availableDevices.append(inspection.qrcode);
    }

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] available device count:"
             << m_availableDevices.size();
    return m_availableDevices;
}

void SH85PeriodicSelfCheckTask3::finishDevice(const QString& qrcode,
                                              bool success,
                                              SH85SelfChecker::Result result,
                                              const QString& description,
                                              double minimumHumidity)
{
    if (!m_roundContext.finishDevice(qrcode, success, description, minimumHumidity)) {
        return;
    }

    m_logService.writeDeviceFinished(m_roundContext.roundId(),
                                     qrcode,
                                     success,
                                     result,
                                     description);
    emit oneFinished(qrcode, success, description, minimumHumidity);
}

void SH85PeriodicSelfCheckTask3::tryFinishRound()
{
    if (!m_roundContext.isActive() || m_roundContext.hasPendingDevices()) {
        return;
    }

    const SH85SelfCheck::RoundSummary summary = m_roundContext.summary();
    const SelfCheckSummary selfCheckSummary = buildSelfCheckSummary(summary);

    QStringList failedDevices;
    QStringList skippedDevices;
    for (const DeviceResult& detail : selfCheckSummary.details) {
        if (!detail.participated) {
            skippedDevices.append(detail.qrcode);
        } else if (!detail.success) {
            failedDevices.append(detail.qrcode);
        }
    }

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask3] round finished"
             << "roundId=" << m_roundContext.roundId()
             << "success=" << summary.successCount
             << "failure=" << summary.failureCount
             << "skipped=" << summary.skippedCount;
    m_logService.writeRoundFinished(m_roundContext.roundId(),
                                    selfCheckSummary.startTime,
                                    selfCheckSummary.endTime,
                                    summary,
                                    failedDevices,
                                    skippedDevices);

    emit roundFinished(m_roundContext.roundId(),
                       summary.totalCount,
                       summary.successCount,
                       summary.failureCount,
                       summary.skippedCount);
    emit allDevicesFinished(summary.totalCount, summary.successCount, summary.failureCount);
    emit allFinished(selfCheckSummary);

    m_roundContext.completeRound();
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }

    if (m_roundRunner) {
        m_roundRunner->disconnectAllCheckers();
    }

    if (state() == SchedulerTask::Running && m_enabled && !m_singleDeviceMode) {
        startIntervalCountdown();
    } else {
        enterTaskState(State::Stopped);
    }
}

void SH85PeriodicSelfCheckTask3::appendNotSubmittedAndFinish(const QString& qrcode,
                                                             SH85SelfChecker::Result result,
                                                             const QString& reason)
{
    m_logService.writeDeviceStartFailed(m_roundContext.roundId(), qrcode, result, reason);
    finishDevice(qrcode, false, result, reason, -1.0);
}

void SH85PeriodicSelfCheckTask3::enterTaskState(State state)
{
    if (m_taskState == state) {
        return;
    }

    m_taskState = state;
    emit taskStateChanged(m_taskState);
    m_logService.writeTaskStateChanged(stateToString(m_taskState));
}

void SH85PeriodicSelfCheckTask3::startIntervalCountdown()
{
    m_intervalRemainingSeconds = m_periodSec;
    emit intervalCountdown(m_intervalRemainingSeconds);
    enterTaskState(State::WaitingNext);
    m_logService.writeNextRoundScheduled(m_periodSec);

    if (m_periodTimer && m_periodSec > 0) {
        m_periodTimer->start(m_periodSec * 1000);
    }
    if (m_intervalCountdownTimer && m_periodSec > 0) {
        m_intervalCountdownTimer->start();
    }
}

void SH85PeriodicSelfCheckTask3::stopIntervalCountdown()
{
    if (m_intervalCountdownTimer) {
        m_intervalCountdownTimer->stop();
    }
    m_intervalRemainingSeconds = 0;
}

SH85PeriodicSelfCheckTask3::SelfCheckSummary
SH85PeriodicSelfCheckTask3::buildSelfCheckSummary(const SH85SelfCheck::RoundSummary& summary) const
{
    SelfCheckSummary selfCheckSummary;
    selfCheckSummary.roundId = m_roundContext.roundId();
    selfCheckSummary.startTime = m_roundContext.startTime();
    selfCheckSummary.endTime = currentTimestamp();
    selfCheckSummary.totalCount = summary.totalCount;
    selfCheckSummary.participatedCount = summary.participatedCount;
    selfCheckSummary.successCount = summary.successCount;
    selfCheckSummary.failureCount = summary.failureCount;
    selfCheckSummary.skippedCount = summary.skippedCount;
    selfCheckSummary.details.reserve(m_roundContext.orderedQrcodes().size());

    for (const QString& qrcode : m_roundContext.orderedQrcodes()) {
        const SH85SelfCheck::DeviceResult result = m_roundContext.result(qrcode);
        DeviceResult detail;
        detail.qrcode = result.qrcode;
        detail.participated = result.participated;
        detail.success = result.success;
        detail.description = result.description;
        detail.minimumHumidity = result.minimumHumidity;
        selfCheckSummary.details.append(detail);
    }

    return selfCheckSummary;
}

QString SH85PeriodicSelfCheckTask3::currentTimestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void SH85PeriodicSelfCheckTask3::onElapsedTimerTick()
{
    ++m_elapsedSeconds;
    emit elapsedTick(m_elapsedSeconds);
}

void SH85PeriodicSelfCheckTask3::onIntervalCountdownTick()
{
    if (m_intervalRemainingSeconds > 0) {
        --m_intervalRemainingSeconds;
    }

    emit intervalCountdown(m_intervalRemainingSeconds);

    if (m_intervalRemainingSeconds <= 0 && m_intervalCountdownTimer) {
        m_intervalCountdownTimer->stop();
    }
}

void SH85PeriodicSelfCheckTask3::onCheckerCountdownTick(int remainingSeconds,
                                                       const QString& masterId)
{
    if (!m_roundContext.isActive() || !m_roundContext.isPending(masterId)) {
        return;
    }

    // 只有 active 且 pending 的设备才允许更新 UI 倒计时。
    emit countdownTick(remainingSeconds, masterId);
}

void SH85PeriodicSelfCheckTask3::onCheckerStateChanged(SH85SelfChecker::State state,
                                                       const QString& masterId)
{
    if (!m_roundContext.isActive() || !m_roundContext.isPending(masterId)) {
        return;
    }

    // 当前阶段先缓存下来，后续可用于命令日志或报告归属。
    m_checkerStates[masterId] = state;
    m_logService.writeCheckerStateChanged(m_roundContext.roundId(), masterId, state);
    emit selfCheckerStateChanged(state, masterId);
}

void SH85PeriodicSelfCheckTask3::onCheckerFinished(bool success,
                                                   SH85SelfChecker::Result result,
                                                   const QString& message,
                                                   const QString& masterId,
                                                   double minimumHumidity)
{
    if (!m_roundContext.isActive() || !m_roundContext.isPending(masterId)) {
        return;
    }

    const QString description = success
        ? QStringLiteral("Success")
        : (message.isEmpty() ? SH85SelfChecker::resultToString(result) : message);

    finishDevice(masterId, success, result, description, minimumHumidity);
    tryFinishRound();
}

void SH85PeriodicSelfCheckTask3::onCheckerCommandCompleted(ModbusCommand cmd,
                                                           const QString& masterId)
{
    emit commandCompleted(cmd, masterId);
    m_logService.writeCommunicateLog(cmd, masterId);
}

void SH85PeriodicSelfCheckTask3::onCheckerCommandRetrying(ModbusCommand cmd,
                                                          const QString& masterId)
{
    emit commandRetrying(cmd, masterId);
    m_logService.writeCheckerCommandRetrying(m_roundContext.roundId(), masterId, cmd);
}

void SH85PeriodicSelfCheckTask3::onCheckerErrorOccurred(SH85SelfChecker::Result result,
                                                        const QString& message,
                                                        const QString& masterId)
{
    emit errorOccurred(result, message, masterId);
    m_logService.writeCheckerError(m_roundContext.roundId(), masterId, result, message);
}
