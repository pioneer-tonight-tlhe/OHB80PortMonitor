#include "sh85_periodic_self_check_task2.h"

#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "app/shareddata.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaType>
#include <QTimer>
#include <QVariantMap>

namespace {
ModbusCommand startSelfCheckTemplateCommand()
{
    CommandPool* pool = ModbusTcpMasterManager::instance().commandPool();
    if (!pool || !pool->contains(QStringLiteral("StartSelfCheck"))) {
        return ModbusCommand();
    }

    ModbusCommand cmd = pool->templateCommand(QStringLiteral("StartSelfCheck"));
    cmd.maxRetryCount = 0;
    return cmd;
}

QStringList targetQrcodes(bool singleDeviceMode, const QString& singleDeviceQrcode)
{
    if (singleDeviceMode) {
        return singleDeviceQrcode.isEmpty() ? QStringList() : QStringList{singleDeviceQrcode};
    }

    return SharedData::getAllQrcodes();
}

void writeCommunicateLog(ModbusCommand cmd, const QString& masterId)
{
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("-");

    int execStatus = 3;
    if (cmd.received) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    } else if (cmd.sendCount > 1) {
        execStatus = 2;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);
    QString description;
    if (execStatus != 0) {
        description = cmd.errorMessage;
    } else {
        const QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (!parsedData.isEmpty()) {
            QStringList parts;
            for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                parts << QStringLiteral("%1=%2").arg(it.key(), it.value().toString());
            }
            description = parts.join(QStringLiteral(", "));
        }
    }

    if (description.isEmpty()) {
        description = QStringLiteral("OK");
    }

    if (LogDB::CommunicateLogDBCon* db = LogDB::DatabaseManager::instance().communicateLogCon()) {
        const QString respTimeStr = cmd.responseMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
            : QString();
        db->insertRecord(sentTimeStr, respTimeStr, cmd.id, masterId,
                         execStatus, retryCount,
                         cmd.request.rawBytes, cmd.response.rawBytes, description,
                         UserPermission::Engineer);
    }
}

struct SH85Periodic2MetaTypeRegister {
    SH85Periodic2MetaTypeRegister() {
        qRegisterMetaType<SH85PeriodicSelfCheckTask2::TimeUnit>("SH85PeriodicSelfCheckTask2::TimeUnit");
        qRegisterMetaType<SH85PeriodicSelfCheckTask2::State>("SH85PeriodicSelfCheckTask2::State");
        qRegisterMetaType<SH85PeriodicSelfCheckTask2::DeviceResult>("SH85PeriodicSelfCheckTask2::DeviceResult");
        qRegisterMetaType<SH85PeriodicSelfCheckTask2::SelfCheckSummary>("SH85PeriodicSelfCheckTask2::SelfCheckSummary");
    }
};
static SH85Periodic2MetaTypeRegister s_sh85Periodic2MetaRegister;
} // namespace

SH85PeriodicSelfCheckTask2::SH85PeriodicSelfCheckTask2(QObject* parent)
    : SchedulerTask(parent)
{
    initPeriodTimer();
}

SH85PeriodicSelfCheckTask2::~SH85PeriodicSelfCheckTask2()
{
    disconnectAllCheckers();
}

void SH85PeriodicSelfCheckTask2::initPeriodTimer()
{
    m_periodTimer = new QTimer(this);
    m_periodTimer->setInterval(1000);
    m_periodTimer->setSingleShot(false);
    connect(m_periodTimer, &QTimer::timeout, this, &SH85PeriodicSelfCheckTask2::onIntervalTick);

    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);
    m_elapsedTimer->setSingleShot(false);
    connect(m_elapsedTimer, &QTimer::timeout, this, &SH85PeriodicSelfCheckTask2::onIntervalTick);

    m_bootDelay = new BootDelayTimer(this);
    connect(m_bootDelay, &BootDelayTimer::countdown,
            this, &SH85PeriodicSelfCheckTask2::bootDelayCountdown);
    connect(m_bootDelay, &BootDelayTimer::timeout,
            this, &SH85PeriodicSelfCheckTask2::onBootDelayTimeout);
}

void SH85PeriodicSelfCheckTask2::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] setEnabled:" << enabled;

    if (enabled) {
        if (state() == SchedulerTask::Running && m_taskState == State::Stopped
            && (!m_bootDelay || !m_bootDelay->isActive())) {
            onPeriodTimeout();
        }
    } else if (m_taskState == State::WaitingNext || m_taskState == State::Stopped) {
        enterTaskState(State::Stopped);
    }
}

void SH85PeriodicSelfCheckTask2::setPeriod(int value, SH85PeriodicSelfCheckTask2::TimeUnit unit)
{
    if (value <= 0) {
        value = 1;
    }

    switch (unit) {
    case TimeUnit::Second:
        m_periodSec = value;
        break;
    case TimeUnit::Minute:
        m_periodSec = value * 60;
        break;
    case TimeUnit::Hour:
        m_periodSec = value * 3600;
        break;
    }

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] setPeriod:" << value
             << timeUnitToString(unit) << "(" << m_periodSec << "s)";

    if (m_taskState == State::WaitingNext) {
        m_intervalRemainingSec = m_periodSec;
        emit intervalCountdown(m_intervalRemainingSec);
    }
}

void SH85PeriodicSelfCheckTask2::setPeriod(int value, const QString& unit)
{
    const QString unitLower = unit.toLower();
    if (unitLower == QStringLiteral("s") || unitLower == QStringLiteral("sec") || unitLower == QStringLiteral("second")) {
        setPeriod(value, TimeUnit::Second);
    } else if (unitLower == QStringLiteral("hour") || unitLower == QStringLiteral("h")) {
        setPeriod(value, TimeUnit::Hour);
    } else {
        setPeriod(value, TimeUnit::Minute);
    }
}

QString SH85PeriodicSelfCheckTask2::timeUnitToString(TimeUnit unit)
{
    switch (unit) {
    case TimeUnit::Second:
        return QStringLiteral("s");
    case TimeUnit::Minute:
        return QStringLiteral("min");
    case TimeUnit::Hour:
        return QStringLiteral("hour");
    }
    return QStringLiteral("?");
}

void SH85PeriodicSelfCheckTask2::setSingleDevice(const QString& qrcode)
{
    m_singleDeviceMode = !qrcode.isEmpty();
    m_singleDeviceQrcode = qrcode;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] setSingleDevice:" << qrcode
             << ", singleDeviceMode=" << m_singleDeviceMode;
}

void SH85PeriodicSelfCheckTask2::runSingleDeviceOnce(const QString& qrcode)
{
    if (qrcode.isEmpty()) {
        emit statusChanged(QStringLiteral("Invalid device id"), qrcode);
        emit singleFinished(false, SH85SelfChecker::Result::StartCommandFailed, qrcode);
        return;
    }

    if (state() != SchedulerTask::Running) {
        setState(SchedulerTask::Running);
    }

    if (m_roundActive && !m_pendingQrcodes.isEmpty()) {
        emit statusChanged(QStringLiteral("Self-check is already running"), qrcode);
        emit singleFinished(false, SH85SelfChecker::Result::StartCommandFailed, qrcode);
        return;
    }

    startAvailableDeviceChecks(true, qrcode);
}

void SH85PeriodicSelfCheckTask2::start()
{
    setState(SchedulerTask::Running);

    if (!m_enabled) {
        qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] start ignored because task is disabled";
        enterTaskState(State::Stopped);
        return;
    }

    enterTaskState(State::Stopped);
    if (m_bootDelay) {
        m_bootDelay->startSeconds(10);
    } else {
        onPeriodTimeout();
    }
}

void SH85PeriodicSelfCheckTask2::stop()
{
    if (m_periodTimer) {
        m_periodTimer->stop();
    }
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }
    if (m_bootDelay) {
        m_bootDelay->stop();
    }

    const QStringList pending = m_pendingQrcodes.values();
    for (const QString& qrcode : pending) {
        if (m_deviceRecords.contains(qrcode)) {
            m_deviceRecords[qrcode].appendRecord(QStringLiteral("Task stopped before checker finished"));
        }
        finishDevice(qrcode,
                     false,
                     SH85SelfChecker::Result::Cancelled,
                     QStringLiteral("Cancelled by task stop"));
    }

    tryFinishRound();
    disconnectAllCheckers();

    enterTaskState(State::Stopped);
    setState(SchedulerTask::Finished);
    emit finished(true, QStringLiteral("SH85PeriodicSelfCheckTask2 stopped"));
}

void SH85PeriodicSelfCheckTask2::onBootDelayTimeout()
{
    if (state() != SchedulerTask::Running || !m_enabled) {
        enterTaskState(State::Stopped);
        return;
    }

    onPeriodTimeout();
}

void SH85PeriodicSelfCheckTask2::onIntervalTick()
{
    if (m_taskState == State::Checking) {
        ++m_elapsedSeconds;
        emit elapsedTick(m_elapsedSeconds);
        return;
    }

    if (m_taskState != State::WaitingNext) {
        return;
    }

    if (m_intervalRemainingSec > 0) {
        --m_intervalRemainingSec;
        emit intervalCountdown(m_intervalRemainingSec);
    }

    if (m_intervalRemainingSec <= 0) {
        onPeriodTimeout();
    }
}

void SH85PeriodicSelfCheckTask2::onPeriodTimeout()
{
    if (state() != SchedulerTask::Running || !m_enabled) {
        return;
    }

    if (m_roundActive && !m_pendingQrcodes.isEmpty()) {
        qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask2] previous round is still running, skip trigger"
                   << "roundId=" << m_roundId
                   << "pending=" << m_pendingQrcodes.size();
        return;
    }

    startAvailableDeviceChecks();

    if (m_periodRemainingCalls > 0) {
        --m_periodRemainingCalls;
        if (m_periodRemainingCalls == 0) {
            enterTaskState(State::Stopped);
        }
    }
}

void SH85PeriodicSelfCheckTask2::startAvailableDeviceChecks(bool manualRound, const QString& manualQrcode)
{
    m_roundId = SH85SelfCheckLogHelper::createRoundId();
    m_roundStartTime = currentTimestamp();
    m_roundOrderedQrcodes = manualRound
        ? (manualQrcode.isEmpty() ? QStringList() : QStringList{manualQrcode})
        : targetQrcodes(m_singleDeviceMode, m_singleDeviceQrcode);
    m_roundActive = true;
    m_currentRoundManual = manualRound;
    m_lastResult = SH85SelfChecker::Result::Success;

    m_availableDevices.clear();
    m_pendingQrcodes.clear();
    m_roundResults.clear();
    m_deviceRecords.clear();
    m_checkerStates.clear();
    disconnectAllCheckers();
    enterTaskState(State::Checking);

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] round started"
             << "roundId=" << m_roundId
             << "manual=" << manualRound
             << "total=" << m_roundOrderedQrcodes.size();

    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        DeviceResult result;
        result.qrcode = qrcode;
        result.participated = false;
        result.success = false;
        result.description = QStringLiteral("Not participated");
        m_roundResults.insert(qrcode, result);
        emit deviceParticipated(qrcode, false);

        SH85SelfCheckTaskRecord record;
        SH85SelfCheckLogHelper::initRecord(record,
                                           manualRound ? SH85SelfCheckLogHelper::Mode::Manual
                                                       : SH85SelfCheckLogHelper::Mode::Periodic,
                                           qrcode,
                                           m_roundId);
        SH85SelfCheckLogHelper::appendStart(record);
        m_deviceRecords.insert(qrcode, record);
        m_checkerStates.insert(qrcode, SH85SelfChecker::State::Idle);
    }

    auto& mgr = ModbusTcpMasterManager::instance();
    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        FoupOfOHBInfo* foupInfo = SharedData::getFoupByQRCode(qrcode);
        const bool enabled = (foupInfo && foupInfo->enable());

        if (!enabled) {
            DeviceResult& result = m_roundResults[qrcode];
            result.description = QStringLiteral("Device disabled");
            SH85SelfCheckLogHelper::appendSkip(m_deviceRecords[qrcode], result.description);
            SH85SelfCheckLogHelper::finishSkippedRecord(m_deviceRecords[qrcode], result.description);
            SH85SelfCheckLogHelper::writeRecord(m_deviceRecords[qrcode], false);
            if (manualRound) {
                emit statusChanged(result.description, qrcode);
                emit singleFinished(false, SH85SelfChecker::Result::StartCommandFailed, qrcode);
            }
            continue;
        }

        if (foupInfo && foupInfo->foupIn()) {
            DeviceResult& result = m_roundResults[qrcode];
            result.description = QStringLiteral("FOUP in place");
            SH85SelfCheckLogHelper::appendSkip(m_deviceRecords[qrcode], result.description);
            SH85SelfCheckLogHelper::finishSkippedRecord(m_deviceRecords[qrcode], result.description);
            SH85SelfCheckLogHelper::writeRecord(m_deviceRecords[qrcode], false);
            if (manualRound) {
                emit statusChanged(result.description, qrcode);
                emit singleFinished(false, SH85SelfChecker::Result::StartCommandFailed, qrcode);
            }
            continue;
        }

        DeviceResult& result = m_roundResults[qrcode];
        result.participated = true;
        result.description.clear();
        emit deviceParticipated(qrcode, true);
        emit statusChanged(QStringLiteral("Processing"), qrcode);

        ModbusTcpMaster* master = mgr.getMaster(qrcode);
        if (!master || !master->isConnected()) {
            appendNotSubmittedAndFinish(qrcode,
                                        SH85SelfChecker::Result::StartCommandFailed,
                                        QStringLiteral("Device not connected"));
            continue;
        }

        SH85SelfChecker* checker = master->selfChecker();
        if (!checker) {
            appendNotSubmittedAndFinish(qrcode,
                                        SH85SelfChecker::Result::StartCommandFailed,
                                        QStringLiteral("Self-checker is null"));
            continue;
        }

        m_pendingQrcodes.insert(qrcode);

        m_checkerConnections.append(
            connect(checker, &SH85SelfChecker::countdownTick,
                    this, &SH85PeriodicSelfCheckTask2::onCheckerCountdownTick, Qt::QueuedConnection));
        m_checkerConnections.append(
            connect(checker, &SH85SelfChecker::stateChanged,
                    this, &SH85PeriodicSelfCheckTask2::onCheckerStateChanged, Qt::QueuedConnection));
        m_checkerConnections.append(
            connect(checker, &SH85SelfChecker::finished,
                    this, &SH85PeriodicSelfCheckTask2::onCheckerFinished, Qt::QueuedConnection));
        m_checkerConnections.append(
            connect(checker, &SH85SelfChecker::commandCompleted,
                    this, &SH85PeriodicSelfCheckTask2::onCheckerCommandCompleted, Qt::QueuedConnection));
        m_checkerConnections.append(
            connect(checker, &SH85SelfChecker::commandRetrying,
                    this, &SH85PeriodicSelfCheckTask2::onCheckerCommandRetrying, Qt::QueuedConnection));
        m_checkerConnections.append(
            connect(checker, &SH85SelfChecker::errorOccurred,
                    this, &SH85PeriodicSelfCheckTask2::onCheckerErrorOccurred, Qt::QueuedConnection));

        if (!checker->start()) {
            appendNotSubmittedAndFinish(qrcode,
                                        SH85SelfChecker::Result::StartCommandFailed,
                                        QStringLiteral("Checker start failed"));
            continue;
        }

        m_availableDevices.append(qrcode);
        m_deviceRecords[qrcode].appendRecord(QStringLiteral("Self-checker started"));
    }

    tryFinishRound();
}

QStringList SH85PeriodicSelfCheckTask2::filterAvailableDevices()
{
    m_availableDevices.clear();

    auto& mgr = ModbusTcpMasterManager::instance();
    const QStringList allQrcodes = targetQrcodes(m_singleDeviceMode, m_singleDeviceQrcode);

    for (const QString& qrcode : allQrcodes) {
        FoupOfOHBInfo* foupInfo = SharedData::getFoupByQRCode(qrcode);
        const bool enabled = (foupInfo && foupInfo->enable());
        if (!enabled) {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] skip disabled device qrcode=" << qrcode;
            continue;
        }

        if (foupInfo && foupInfo->foupIn()) {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] skip FOUP-in device qrcode=" << qrcode;
            continue;
        }

        ModbusTcpMaster* master = mgr.getMaster(qrcode);
        if (!master || !master->isConnected()) {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] skip disconnected device qrcode=" << qrcode;
            continue;
        }

        m_availableDevices.append(qrcode);
    }

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] available device count:" << m_availableDevices.size();
    return m_availableDevices;
}

void SH85PeriodicSelfCheckTask2::finishDevice(const QString& qrcode,
                                              bool success,
                                              SH85SelfChecker::Result result,
                                              const QString& description)
{
    if (!m_roundResults.contains(qrcode)) {
        return;
    }

    if (m_deviceRecords.contains(qrcode) && m_deviceRecords[qrcode].taskEndTime().isValid()) {
        return;
    }

    DeviceResult& deviceResult = m_roundResults[qrcode];
    deviceResult.qrcode = qrcode;
    deviceResult.participated = true;
    deviceResult.success = success;
    m_lastResult = result;

    m_pendingQrcodes.remove(qrcode);

    QString normalizedDescription = description;
    if (m_deviceRecords.contains(qrcode)) {
        SH85SelfCheckLogHelper::finishRecord(m_deviceRecords[qrcode],
                                             success,
                                             result,
                                             description);
        normalizedDescription = m_deviceRecords[qrcode].description();
        const bool warn = !success && result != SH85SelfChecker::Result::Cancelled;
        SH85SelfCheckLogHelper::writeRecord(m_deviceRecords[qrcode], warn);
    }

    deviceResult.description = normalizedDescription;
    emit statusChanged(success ? QStringLiteral("Success") : normalizedDescription, qrcode);
    emit oneFinished(qrcode, success, normalizedDescription);
    if (m_currentRoundManual) {
        emit singleFinished(success, result, qrcode);
    }
}

void SH85PeriodicSelfCheckTask2::tryFinishRound()
{
    if (!m_roundActive || !m_pendingQrcodes.isEmpty()) {
        return;
    }

    SH85SelfCheckLogHelper::RoundSummary summary;
    summary.roundId = m_roundId;
    summary.startTime = m_roundStartTime;
    summary.endTime = currentTimestamp();
    summary.totalDevices = m_roundOrderedQrcodes.size();

    SelfCheckSummary uiSummary;
    uiSummary.startTime = summary.startTime;
    uiSummary.endTime = summary.endTime;
    uiSummary.details.reserve(m_roundOrderedQrcodes.size());

    QList<SH85SelfCheckTaskRecord> orderedRecords;
    orderedRecords.reserve(m_roundOrderedQrcodes.size());

    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        const DeviceResult result = m_roundResults.value(qrcode);
        uiSummary.details.append(result);
        if (result.participated) {
            ++summary.participatedCount;
            if (result.success) {
                ++summary.successCount;
                ++uiSummary.successCount;
            } else {
                ++summary.failureCount;
                ++uiSummary.failureCount;
                summary.failedDevices.append(qrcode);
            }
        } else {
            ++summary.skippedCount;
            summary.skippedDevices.append(qrcode);
        }

        if (m_deviceRecords.contains(qrcode)) {
            orderedRecords.append(m_deviceRecords.value(qrcode));
        }
    }

    const QString roundResultText = summary.failureCount == 0
        ? QStringLiteral("Success")
        : QStringLiteral("Failed");

    SH85SelfCheckLogHelper::writeRoundSummary(summary);
    SH85SelfCheckLogHelper::writeRoundWarning(summary, orderedRecords);
    SH85SelfCheckLogHelper::writeTaskSeparator(m_currentRoundManual ? SH85SelfCheckLogHelper::Mode::Manual
                                                                    : SH85SelfCheckLogHelper::Mode::Periodic,
                                               summary.roundId,
                                               m_currentRoundManual && m_roundOrderedQrcodes.size() == 1
                                                   ? m_roundOrderedQrcodes.first()
                                                   : QString(),
                                               roundResultText,
                                               summary.startTime,
                                               summary.endTime,
                                               summary.totalDevices,
                                               summary.participatedCount,
                                               summary.successCount,
                                               summary.failureCount,
                                               summary.skippedCount);

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] round finished"
             << "roundId=" << summary.roundId
             << "success=" << summary.successCount
             << "failure=" << summary.failureCount
             << "skipped=" << summary.skippedCount;

    emit allFinished(uiSummary);
    emit allDevicesFinished(summary.totalDevices, summary.successCount, summary.failureCount);

    m_roundActive = false;
    const bool wasManual = m_currentRoundManual;
    disconnectAllCheckers();

    if (wasManual) {
        enterTaskState(m_enabled ? State::WaitingNext : State::Stopped);
    } else if (m_enabled) {
        enterTaskState(State::WaitingNext);
    } else {
        enterTaskState(State::Stopped);
    }
}

void SH85PeriodicSelfCheckTask2::enterTaskState(State state)
{
    if (m_periodTimer) {
        m_periodTimer->stop();
    }
    if (m_elapsedTimer) {
        m_elapsedTimer->stop();
    }

    m_taskState = state;
    switch (state) {
    case State::Stopped:
        m_intervalRemainingSec = 0;
        m_elapsedSeconds = 0;
        emit intervalCountdown(0);
        break;
    case State::Checking:
        m_intervalRemainingSec = 0;
        m_elapsedSeconds = 0;
        emit elapsedTick(m_elapsedSeconds);
        if (m_elapsedTimer) {
            m_elapsedTimer->start();
        }
        break;
    case State::WaitingNext:
        m_elapsedSeconds = 0;
        m_intervalRemainingSec = m_periodSec;
        emit intervalCountdown(m_intervalRemainingSec);
        if (m_periodTimer) {
            m_periodTimer->start();
        }
        break;
    }

    emit taskStateChanged(m_taskState);
}

void SH85PeriodicSelfCheckTask2::disconnectAllCheckers()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_checkerConnections)) {
        QObject::disconnect(connection);
    }
    m_checkerConnections.clear();
}

void SH85PeriodicSelfCheckTask2::appendNotSubmittedAndFinish(const QString& qrcode,
                                                             SH85SelfChecker::Result result,
                                                             const QString& reason)
{
    if (!m_deviceRecords.contains(qrcode)) {
        return;
    }

    SH85SelfCheckLogHelper::appendCommandNotSubmitted(m_deviceRecords[qrcode],
                                                      SH85SelfChecker::State::StartingSelfCheck,
                                                      startSelfCheckTemplateCommand(),
                                                      reason);
    m_deviceRecords[qrcode].appendRecord(QStringLiteral("Start failed\nReason: %1").arg(reason));
    finishDevice(qrcode, false, result, reason);
}

QString SH85PeriodicSelfCheckTask2::currentTimestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void SH85PeriodicSelfCheckTask2::onCheckerCountdownTick(int remainingSeconds, const QString& masterId)
{
    if (!m_roundActive || !m_pendingQrcodes.contains(masterId)) {
        return;
    }

    emit countdownTick(remainingSeconds, masterId);
    if (remainingSeconds <= SH85SelfChecker::kPollWindowMs / 1000) {
        emit statusChanged(QStringLiteral("Checking (%1)").arg(remainingSeconds), masterId);
    }
}

void SH85PeriodicSelfCheckTask2::onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId)
{
    if (!m_roundActive || !m_pendingQrcodes.contains(masterId)) {
        return;
    }

    m_checkerStates[masterId] = state;
    if (state != SH85SelfChecker::State::Done && m_deviceRecords.contains(masterId)) {
        SH85SelfCheckLogHelper::appendStage(m_deviceRecords[masterId], state);
    }

    emit selfCheckerStateChanged(state, masterId);
}

void SH85PeriodicSelfCheckTask2::onCheckerFinished(bool success,
                                                   SH85SelfChecker::Result result,
                                                   const QString& message,
                                                   const QString& masterId)
{
    if (!m_roundActive || !m_pendingQrcodes.contains(masterId)) {
        return;
    }

    const QString description = success
        ? QStringLiteral("Success")
        : (message.isEmpty() ? SH85SelfCheckLogHelper::resultToChineseText(result) : message);

    finishDevice(masterId, success, result, description);
    tryFinishRound();
}

void SH85PeriodicSelfCheckTask2::onCheckerCommandCompleted(ModbusCommand cmd, const QString& masterId)
{
    if (m_roundActive && m_deviceRecords.contains(masterId)) {
        const auto checkerState = m_checkerStates.value(masterId, SH85SelfChecker::State::Idle);
        SH85SelfCheckLogHelper::appendCommand(m_deviceRecords[masterId], checkerState, cmd);
    }

    emit commandCompleted(cmd, masterId);
    writeCommunicateLog(cmd, masterId);
}

void SH85PeriodicSelfCheckTask2::onCheckerCommandRetrying(ModbusCommand cmd, const QString& masterId)
{
    if (m_roundActive && m_deviceRecords.contains(masterId)) {
        const auto checkerState = m_checkerStates.value(masterId, SH85SelfChecker::State::Idle);
        SH85SelfCheckLogHelper::appendCommand(m_deviceRecords[masterId], checkerState, cmd, true);
    }

    emit commandRetrying(cmd, masterId);
}

void SH85PeriodicSelfCheckTask2::onCheckerErrorOccurred(SH85SelfChecker::Result result,
                                                        const QString& message,
                                                        const QString& masterId)
{
    if (m_roundActive && m_deviceRecords.contains(masterId)) {
        const QString normalizedMessage = SH85SelfCheckLogHelper::descriptionToChinese(result, message);
        m_deviceRecords[masterId].appendRecord(
            QStringLiteral("Self-check error\nResult: %1\nMessage: %2")
                .arg(SH85SelfChecker::resultToString(result), normalizedMessage));
    }

    emit errorOccurred(result, message, masterId);
}
