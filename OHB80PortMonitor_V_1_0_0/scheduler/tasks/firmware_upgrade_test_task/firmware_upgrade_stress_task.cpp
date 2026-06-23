#include "firmware_upgrade_stress_task.h"

#include "../firmware_upgrade_task/firmware_upgrade_task.h"
#include "loggermanager.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

#include <QDebug>
#include <QDir>
#include <QRegularExpression>

namespace {
const std::string kTaskLogPath = "scheduler/firmware_upgrade_stress_task";
const int kMinimumRoundIntervalMs = 2000;

int normalizedRoundIntervalMs(int intervalMs)
{
    return intervalMs < kMinimumRoundIntervalMs ? kMinimumRoundIntervalMs : intervalMs;
}
}

FirmwareUpgradeStressTask::FirmwareUpgradeStressTask(const QString &binFilePath,
                                                     int targetRounds,
                                                     int intervalMs,
                                                     const QStringList &deviceIds,
                                                     QObject *parent)
    : SchedulerTask(parent)
    , m_binFilePath(binFilePath)
    , m_targetRounds(targetRounds > 0 ? targetRounds : 1)
    , m_intervalMs(normalizedRoundIntervalMs(intervalMs))
    , m_configuredDeviceIds(deviceIds)
    , m_roundIntervalTimer(new QTimer(this))
{
    m_roundIntervalTimer->setSingleShot(true);
    connect(m_roundIntervalTimer, &QTimer::timeout,
            this, &FirmwareUpgradeStressTask::onRoundIntervalTimeout);

    registerMetaTypes();

    LoggerManager::getInstance()->log(kTaskLogPath, Level::INFO,
        "[FirmwareUpgradeStressTask] task object created");
    LoggerManager::getInstance()->flush(kTaskLogPath);
}

FirmwareUpgradeStressTask::~FirmwareUpgradeStressTask()
{
    teardownCurrentRoundTask();
}

void FirmwareUpgradeStressTask::start()
{
    if (state() == Paused) {
        resume();
        return;
    }

    if (state() == Running) {
        return;
    }

    m_stopRequested = false;

    if (m_targetRounds <= 0) {
        setState(Failed);
        emit finished(false, QStringLiteral("Target rounds must be greater than 0"));
        return;
    }

    if (m_binFilePath.trimmed().isEmpty()) {
        setState(Failed);
        emit finished(false, QStringLiteral("Firmware file path is empty"));
        return;
    }

    if (!m_sessionInitialized) {
        if (!beginSession()) {
            setState(Failed);
            emit finished(false, QStringLiteral("Failed to initialize firmware upgrade test session"));
            return;
        }
    }

    setState(Running);
    writeSessionSnapshot(QStringLiteral("Running"));

    if (m_completedRounds >= m_targetRounds) {
        finalizeSession(QStringLiteral("Finished"), true, QStringLiteral("All rounds are already completed"));
        return;
    }

    if (m_roundIntervalTimer->isActive()) {
        return;
    }

    if (m_currentRoundTask) {
        return;
    }

    startNextRound();
}

void FirmwareUpgradeStressTask::stop()
{
    m_stopRequested = true;
    m_pauseRequested = false;
    m_roundIntervalTimer->stop();

    if (m_currentRoundTask) {
        m_currentRoundTask->stop();
        return;
    }

    finalizeSession(QStringLiteral("Cancelled"), false, QStringLiteral("Firmware upgrade stress test cancelled"));
}

void FirmwareUpgradeStressTask::pause()
{
    if (state() == Paused) {
        return;
    }

    m_pauseRequested = true;

    if (m_roundIntervalTimer->isActive()) {
        m_roundIntervalTimer->stop();
        finalizePause();
    }
}

void FirmwareUpgradeStressTask::resume()
{
    if (state() != Paused) {
        return;
    }

    m_pauseRequested = false;
    setState(Running);
    writeSessionSnapshot(QStringLiteral("Running"));

    if (m_completedRounds >= m_targetRounds) {
        finalizeSession(QStringLiteral("Finished"),
                        true,
                        QStringLiteral("Target rounds are already completed"));
        return;
    }

    startNextRound();
}

void FirmwareUpgradeStressTask::setBinFilePath(const QString &binFilePath)
{
    m_binFilePath = binFilePath;
}

QString FirmwareUpgradeStressTask::binFilePath() const
{
    return m_binFilePath;
}

void FirmwareUpgradeStressTask::setTargetRounds(int targetRounds)
{
    const int minimumTargetRounds = m_completedRounds + 1;
    m_targetRounds = targetRounds < minimumTargetRounds
        ? minimumTargetRounds
        : targetRounds;
}

int FirmwareUpgradeStressTask::targetRounds() const
{
    return m_targetRounds;
}

void FirmwareUpgradeStressTask::setIntervalMs(int intervalMs)
{
    m_intervalMs = normalizedRoundIntervalMs(intervalMs);
}

int FirmwareUpgradeStressTask::intervalMs() const
{
    return m_intervalMs;
}

void FirmwareUpgradeStressTask::setDeviceIds(const QStringList &deviceIds)
{
    m_configuredDeviceIds = deviceIds;
}

QStringList FirmwareUpgradeStressTask::deviceIds() const
{
    return m_deviceIds;
}

QString FirmwareUpgradeStressTask::sessionId() const
{
    return m_sessionId;
}

bool FirmwareUpgradeStressTask::pauseRequested() const
{
    return m_pauseRequested;
}

void FirmwareUpgradeStressTask::onRoundTaskFinished(bool success, const QString &message)
{
    const bool wasStopRequested = m_stopRequested;
    const bool wasPauseRequested = m_pauseRequested;
    const bool roundSetupFailed = !success && m_currentRoundResults.isEmpty();

    if (wasStopRequested && roundSetupFailed) {
        teardownCurrentRoundTask();
        finalizeSession(QStringLiteral("Cancelled"), false, QStringLiteral("Firmware upgrade stress test cancelled"));
        return;
    }

    if (roundSetupFailed) {
        teardownCurrentRoundTask();
        finalizeSession(QStringLiteral("Failed"), false, message);
        return;
    }

    const bool roundSuccess = (m_currentRoundFailedDevices == 0
                            && m_currentRoundFinishedDevices == m_deviceIds.size());
    FirmwareUpgradeTestRoundSummaryRecord roundRecord = buildRoundSummaryRecord(roundSuccess);
    FirmwareUpgradeTestReportRepository::appendRoundSummary(roundRecord);
    emit roundSummaryReady(roundRecord);

    for (auto it = m_currentRoundResults.constBegin(); it != m_currentRoundResults.constEnd(); ++it) {
        if (it.value().success) {
            continue;
        }

        FirmwareUpgradeTestFailureDetailRecord failureRecord =
            buildFailureDetailRecord(it.key(), it.value());
        FirmwareUpgradeTestReportRepository::appendFailureDetail(failureRecord);
        emit failureDetailReady(failureRecord);
    }

    teardownCurrentRoundTask();

    ++m_completedRounds;
    if (roundSuccess) {
        ++m_successRounds;
    } else {
        ++m_failedRounds;
    }

    emit totalRoundProgressChanged(m_completedRounds, m_targetRounds);
    writeSessionSnapshot(QStringLiteral("Running"));

    if (wasStopRequested) {
        finalizeSession(QStringLiteral("Cancelled"), false, QStringLiteral("Firmware upgrade stress test cancelled"));
        return;
    }

    if (m_completedRounds >= m_targetRounds) {
        finalizeSession(QStringLiteral("Finished"), true, QStringLiteral("Firmware upgrade stress test finished"));
        return;
    }

    if (wasPauseRequested) {
        finalizePause();
        return;
    }

    scheduleNextRound();
}

void FirmwareUpgradeStressTask::onRoundDeviceProgress(const QString &deviceId, int percent)
{
    emit deviceProgress(deviceId, percent);
}

void FirmwareUpgradeStressTask::onRoundDeviceStateLog(const QString &deviceId,
                                                      FirmwareUpgrader::UpgradeState state,
                                                      const QString &logMessage,
                                                      const QByteArray &frame)
{
    DeviceRoundResult &result = m_currentRoundResults[deviceId];
    result.phase = phaseFromState(state);
    if (!logMessage.trimmed().isEmpty()) {
        result.message = logMessage;
    }

    emit deviceStateLog(deviceId, state, logMessage, frame);
}

void FirmwareUpgradeStressTask::onRoundDeviceFinished(const QString &deviceId,
                                                      bool success,
                                                      const QString &message)
{
    DeviceRoundResult &result = m_currentRoundResults[deviceId];
    const bool firstFinished = !result.finished;
    if (!result.finished) {
        result.finished = true;
        ++m_currentRoundFinishedDevices;
    }

    result.success = success;
    result.message = message;
    result.occurredTime = QDateTime::currentDateTime();
    result.errorCode = success ? QStringLiteral("OK")
                               : classifyFailureCode(message, result.phase);

    if (!success) {
        if (result.phase.isEmpty()) {
            result.phase = QStringLiteral("Unknown");
        }
    }

    if (firstFinished) {
        if (success) {
            ++m_currentRoundSuccessDevices;
        } else {
            ++m_currentRoundFailedDevices;
        }
    }

    emit currentRoundProgressChanged(m_currentRoundFinishedDevices, m_deviceIds.size());
    emit deviceFinished(deviceId, success, message);
}

void FirmwareUpgradeStressTask::onRoundIntervalTimeout()
{
    if (m_pauseRequested) {
        finalizePause();
        return;
    }

    startNextRound();
}

void FirmwareUpgradeStressTask::registerMetaTypes()
{
    qRegisterMetaType<FirmwareUpgradeTestSessionSummaryRecord>(
        "FirmwareUpgradeTestSessionSummaryRecord");
    qRegisterMetaType<FirmwareUpgradeTestRoundSummaryRecord>(
        "FirmwareUpgradeTestRoundSummaryRecord");
    qRegisterMetaType<FirmwareUpgradeTestFailureDetailRecord>(
        "FirmwareUpgradeTestFailureDetailRecord");
    qRegisterMetaType<FirmwareUpgradeTestReportData>(
        "FirmwareUpgradeTestReportData");
}

bool FirmwareUpgradeStressTask::beginSession()
{
    resolveDeviceIds();
    if (m_deviceIds.isEmpty()) {
        return false;
    }

    m_sessionId = FirmwareUpgradeTestReportRepository::createSessionId();
    if (!FirmwareUpgradeTestReportRepository::initializeSession(m_sessionId)) {
        return false;
    }

    m_sessionStartTime = QDateTime::currentDateTime();
    m_sessionInitialized = true;
    m_completedRounds = 0;
    m_successRounds = 0;
    m_failedRounds = 0;
    emit totalRoundProgressChanged(0, m_targetRounds);
    emit currentRoundProgressChanged(0, m_deviceIds.size());
    writeSessionSnapshot(QStringLiteral("Running"));
    return true;
}

void FirmwareUpgradeStressTask::resolveDeviceIds()
{
    if (!m_configuredDeviceIds.isEmpty()) {
        m_deviceIds = sortedDeviceIds(m_configuredDeviceIds);
        return;
    }

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    m_deviceIds = sortedDeviceIds(manager.masterIds());
}

void FirmwareUpgradeStressTask::startNextRound()
{
    if (m_pauseRequested || m_stopRequested) {
        return;
    }

    if (m_currentRoundTask || m_completedRounds >= m_targetRounds) {
        return;
    }

    resetCurrentRoundState();
    m_currentRoundIndex = m_completedRounds + 1;
    m_currentRoundStartTime = QDateTime::currentDateTime();

    FirmwareUpgradeTask *roundTask =
        new FirmwareUpgradeTask(m_deviceIds, m_binFilePath, this);
    m_currentRoundTask = roundTask;

    connect(roundTask, &FirmwareUpgradeTask::deviceProgress,
            this, &FirmwareUpgradeStressTask::onRoundDeviceProgress);
    connect(roundTask, &FirmwareUpgradeTask::deviceStateLog,
            this, &FirmwareUpgradeStressTask::onRoundDeviceStateLog);
    connect(roundTask, &FirmwareUpgradeTask::deviceFinished,
            this, &FirmwareUpgradeStressTask::onRoundDeviceFinished);
    connect(roundTask, &FirmwareUpgradeTask::finished,
            this, &FirmwareUpgradeStressTask::onRoundTaskFinished);

    emit currentRoundProgressChanged(0, m_deviceIds.size());
    emit roundStarted(m_currentRoundIndex, m_targetRounds, m_deviceIds);

    roundTask->start();
}

void FirmwareUpgradeStressTask::scheduleNextRound()
{
    if (m_pauseRequested || m_stopRequested) {
        return;
    }

    const int safeIntervalMs = normalizedRoundIntervalMs(m_intervalMs);
    m_roundIntervalTimer->start(safeIntervalMs);
}

void FirmwareUpgradeStressTask::teardownCurrentRoundTask()
{
    if (!m_currentRoundTask) {
        return;
    }

    disconnect(m_currentRoundTask, nullptr, this, nullptr);
    m_currentRoundTask->deleteLater();
    m_currentRoundTask = nullptr;
}

void FirmwareUpgradeStressTask::resetCurrentRoundState()
{
    m_currentRoundFinishedDevices = 0;
    m_currentRoundSuccessDevices = 0;
    m_currentRoundFailedDevices = 0;
    m_currentRoundResults.clear();
}

void FirmwareUpgradeStressTask::finalizePause()
{
    setState(Paused);
    writeSessionSnapshot(QStringLiteral("Paused"));
}

void FirmwareUpgradeStressTask::finalizeSession(const QString &status,
                                                bool success,
                                                const QString &message)
{
    m_roundIntervalTimer->stop();
    teardownCurrentRoundTask();

    if (status == QStringLiteral("Finished")) {
        setState(Finished);
    } else if (status == QStringLiteral("Cancelled")) {
        setState(Cancelled);
    } else if (status == QStringLiteral("Paused")) {
        setState(Paused);
    } else {
        setState(Failed);
    }

    writeSessionSnapshot(status);

    FirmwareUpgradeTestSessionSummaryRecord record;
    if (FirmwareUpgradeTestReportRepository::loadSessionSummary(m_sessionId, &record)) {
        emit sessionSummaryUpdated(record);
        emit stressTaskFinished(record);
    }

    emit finished(success, message);
}

void FirmwareUpgradeStressTask::writeSessionSnapshot(const QString &status)
{
    if (!m_sessionInitialized) {
        return;
    }

    FirmwareUpgradeTestSessionSummaryRecord record;
    record.sessionId = m_sessionId;
    record.startTime = m_sessionStartTime;
    record.endTime = (status == QStringLiteral("Running")) ? QDateTime() : QDateTime::currentDateTime();
    record.status = status;
    record.targetRounds = m_targetRounds;
    record.completedRounds = m_completedRounds;
    record.intervalMs = m_intervalMs;
    record.deviceCount = m_deviceIds.size();
    record.successRounds = m_successRounds;
    record.failedRounds = m_failedRounds;
    record.binFilePath = m_binFilePath;

    FirmwareUpgradeTestReportRepository::writeSessionSummary(record);
    emit sessionSummaryUpdated(record);

    const int percent = (m_targetRounds > 0)
        ? (m_completedRounds * 100 / m_targetRounds)
        : 0;
    emit progress(percent,
                  QStringLiteral("Completed rounds %1/%2").arg(m_completedRounds).arg(m_targetRounds));
}

FirmwareUpgradeTestRoundSummaryRecord
FirmwareUpgradeStressTask::buildRoundSummaryRecord(bool roundSuccess) const
{
    FirmwareUpgradeTestRoundSummaryRecord record;
    record.sessionId = m_sessionId;
    record.roundIndex = m_currentRoundIndex;
    record.startTime = m_currentRoundStartTime;
    record.endTime = QDateTime::currentDateTime();
    record.totalDevices = m_deviceIds.size();
    record.successDevices = m_currentRoundSuccessDevices;
    record.failedDevices = m_currentRoundFailedDevices;
    record.result = roundSuccess ? QStringLiteral("Success") : QStringLiteral("Failed");
    record.screenshotPath = buildRoundScreenshotPath();
    return record;
}

FirmwareUpgradeTestFailureDetailRecord
FirmwareUpgradeStressTask::buildFailureDetailRecord(const QString &deviceId,
                                                    const DeviceRoundResult &result) const
{
    FirmwareUpgradeTestFailureDetailRecord record;
    record.sessionId = m_sessionId;
    record.roundIndex = m_currentRoundIndex;
    record.qrcode = deviceId;
    record.phase = result.phase.isEmpty() ? QStringLiteral("Unknown") : result.phase;
    record.errorCode = result.errorCode;
    record.errorMessage = result.message;
    record.occurredTime = result.occurredTime;
    record.screenshotPath = buildRoundScreenshotPath();
    return record;
}

QString FirmwareUpgradeStressTask::buildRoundScreenshotPath() const
{
    if (m_sessionId.trimmed().isEmpty() || m_currentRoundIndex <= 0) {
        return QString();
    }

    const QString fileName = QStringLiteral("fw_test_%1_round_%2.png")
        .arg(m_sessionId)
        .arg(m_currentRoundIndex, 4, 10, QLatin1Char('0'));
    return QDir(FirmwareUpgradeTestReportRepository::captureDirectoryPath(m_sessionId))
        .filePath(fileName);
}

QString FirmwareUpgradeStressTask::classifyFailureCode(const QString &message,
                                                       const QString &phase) const
{
    const QString lowerMessage = message.toLower();
    if (lowerMessage.contains(QStringLiteral("tcp"))) {
        return QStringLiteral("TCP_DISCONNECTED");
    }
    if (lowerMessage.contains(QStringLiteral("areyouthere"))) {
        return QStringLiteral("VERSION_TIMEOUT");
    }
    if (lowerMessage.contains(QStringLiteral("bin")) && lowerMessage.contains(QStringLiteral("crc"))) {
        return QStringLiteral("TRANSFER_CRC_ERROR");
    }
    if (lowerMessage.contains(QStringLiteral("bin")) && lowerMessage.contains(QStringLiteral("size"))) {
        return QStringLiteral("TRANSFER_SIZE_ERROR");
    }
    if (lowerMessage.contains(QStringLiteral("bin")) && lowerMessage.contains(QStringLiteral("frame"))) {
        return QStringLiteral("TRANSFER_FRAME_ERROR");
    }
    if (lowerMessage.contains(QStringLiteral("bin"))) {
        return QStringLiteral("TRANSFER_DATA_ERROR");
    }
    if (lowerMessage.contains(QStringLiteral("mismatch"))) {
        return QStringLiteral("VERSION_MISMATCH");
    }
    if (lowerMessage.contains(QStringLiteral("cancel"))) {
        return QStringLiteral("CANCELLED");
    }

    if (phase == QStringLiteral("Preparing")) {
        return QStringLiteral("PREPARING_FAILURE");
    }
    if (phase == QStringLiteral("WaitingDevice")) {
        return QStringLiteral("WAITING_DEVICE_FAILURE");
    }
    if (phase == QStringLiteral("DataTransfer")) {
        return QStringLiteral("DATA_TRANSFER_FAILURE");
    }
    if (phase == QStringLiteral("VersionCheck")) {
        return QStringLiteral("VERSION_CHECK_FAILURE");
    }
    if (phase == QStringLiteral("Finished")) {
        return QStringLiteral("FINALIZATION_FAILURE");
    }

    return QStringLiteral("UNKNOWN_FAILURE");
}

QString FirmwareUpgradeStressTask::phaseFromState(FirmwareUpgrader::UpgradeState state) const
{
    using UpgradeState = FirmwareUpgrader::UpgradeState;

    switch (state) {
    case UpgradeState::Preparing:
    case UpgradeState::PrepareCmdSent:
    case UpgradeState::PrepareCmdFinished:
        return QStringLiteral("Preparing");
    case UpgradeState::WaitingDevice:
    case UpgradeState::WaitingDeviceFinished:
        return QStringLiteral("WaitingDevice");
    case UpgradeState::DataTransferStarted:
    case UpgradeState::SendingDataFrame:
    case UpgradeState::SendingLastFrame:
    case UpgradeState::DataTransferFinished:
        return QStringLiteral("DataTransfer");
    case UpgradeState::VersionCmdSent:
    case UpgradeState::VersionCmdFinished:
        return QStringLiteral("VersionCheck");
    case UpgradeState::Finished:
        return QStringLiteral("Finished");
    }

    return QStringLiteral("Unknown");
}

QStringList FirmwareUpgradeStressTask::sortedDeviceIds(const QStringList &deviceIds)
{
    QStringList sortedIds = deviceIds;
    std::sort(sortedIds.begin(), sortedIds.end(),
              [](const QString &lhs, const QString &rhs) {
        static const QRegularExpression re(QStringLiteral("\\d+"));
        const QRegularExpressionMatch lhsMatch = re.match(lhs);
        const QRegularExpressionMatch rhsMatch = re.match(rhs);
        if (lhsMatch.hasMatch() && rhsMatch.hasMatch()) {
            return lhsMatch.captured().toInt() < rhsMatch.captured().toInt();
        }
        return lhs < rhs;
    });
    return sortedIds;
}
