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
#include <QTimer>
#include <QVariantMap>

namespace {
// 获取 StartSelfCheck 模板，用于“未真正下发”场景的日志补全。
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

// 根据当前运行模式获取本轮目标设备列表。
QStringList targetQrcodes(bool singleDeviceMode, const QString& singleDeviceQrcode)
{
    if (singleDeviceMode) {
        return singleDeviceQrcode.isEmpty() ? QStringList() : QStringList{singleDeviceQrcode};
    }

    return SharedData::getAllQrcodes();
}

// SH85SelfChecker 只负责发信号，通讯日志数据库仍由 Task 层统一落库。
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
    // 周期定时器只负责触发新一轮自检；单轮内部时序由 SH85SelfChecker 管理。
    m_periodTimer = new QTimer(this);
    m_periodTimer->setSingleShot(false);
    connect(m_periodTimer, &QTimer::timeout, this, &SH85PeriodicSelfCheckTask2::onPeriodTimeout);
}

QString SH85PeriodicSelfCheckTask2::currentState() const
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

void SH85PeriodicSelfCheckTask2::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }

    m_enabled = enabled;
    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] setEnabled:" << enabled;

    if (!m_enabled && m_periodTimer) {
        m_periodTimer->stop();
    }
}

void SH85PeriodicSelfCheckTask2::setPeriod(int value, const QString& unit)
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

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] setPeriod:" << value
             << unit << "(" << totalSec << "s)";
}

void SH85PeriodicSelfCheckTask2::setSingleDevice(const QString& qrcode)
{
    m_singleDeviceMode = !qrcode.isEmpty();
    m_singleDeviceQrcode = qrcode;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] setSingleDevice:" << qrcode
             << ", singleDeviceMode=" << m_singleDeviceMode;
}

void SH85PeriodicSelfCheckTask2::start()
{
    setState(SchedulerTask::Running);

    if (!m_enabled) {
        qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] start ignored because task is disabled";
        return;
    }

    // 应用刚启动时设备启用状态、FOUP 状态、网络状态可能尚未稳定，先延时再跑首轮。
    QTimer::singleShot(10000, this, [this]() {
        if (state() != SchedulerTask::Running || !m_enabled) {
            return;
        }

        if (m_singleDeviceMode) {
            m_periodRemainingCalls = 1;
        }

        onPeriodTimeout();

        if (!m_singleDeviceMode && m_periodSec > 0 && m_periodTimer) {
            const int intervalMs = m_periodSec * 1000 + SH85SelfChecker::kTotalDurationMs;
            m_periodTimer->start(intervalMs);
        }
    });
}

void SH85PeriodicSelfCheckTask2::stop()
{
    if (m_periodTimer) {
        m_periodTimer->stop();
    }

    // 若 stop 时仍有设备在自检，统一收口为 Cancelled，并写出已聚合的设备记录。
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

    setState(SchedulerTask::Finished);
    emit finished(true, QStringLiteral("SH85PeriodicSelfCheckTask2 stopped"));
}

void SH85PeriodicSelfCheckTask2::onPeriodTimeout()
{
    if (state() != SchedulerTask::Running || !m_enabled) {
        return;
    }

    // 防止上一轮未结束时叠加启动下一轮，避免 checker 信号和日志上下文串轮次。
    if (m_roundActive && !m_pendingQrcodes.isEmpty()) {
        qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask2] previous round is still running, skip trigger"
                   << "roundId=" << m_roundId
                   << "pending=" << m_pendingQrcodes.size();
        return;
    }

    startAvailableDeviceChecks();

    if (m_periodRemainingCalls > 0) {
        --m_periodRemainingCalls;
        if (m_periodRemainingCalls == 0 && m_periodTimer) {
            m_periodTimer->stop();
        }
    }
}

void SH85PeriodicSelfCheckTask2::startAvailableDeviceChecks()
{
    // 初始化本轮上下文：所有目标设备都会先创建 record，后续只往内存追加过程。
    m_roundId = SH85SelfCheckLogHelper::createRoundId();
    m_roundStartTime = currentTimestamp();
    m_roundOrderedQrcodes = targetQrcodes(m_singleDeviceMode, m_singleDeviceQrcode);
    m_roundActive = true;

    m_availableDevices.clear();
    m_pendingQrcodes.clear();
    m_roundResults.clear();
    m_deviceRecords.clear();
    m_checkerStates.clear();
    disconnectAllCheckers();

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] round started"
             << "roundId=" << m_roundId
             << "total=" << m_roundOrderedQrcodes.size();

    // 先为每台设备创建默认“未参与”结果，确保轮次汇总的设备顺序稳定。
    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        DeviceResult result;
        result.qrcode = qrcode;
        result.participated = false;
        result.success = false;
        result.description = QStringLiteral("Not participated");
        m_roundResults.insert(qrcode, result);

        SH85SelfCheckTaskRecord record;
        SH85SelfCheckLogHelper::initRecord(record,
                                           SH85SelfCheckLogHelper::Mode::Periodic,
                                           qrcode,
                                           m_roundId);
        SH85SelfCheckLogHelper::appendStart(record);
        m_deviceRecords.insert(qrcode, record);
        m_checkerStates.insert(qrcode, SH85SelfChecker::State::Idle);
    }

    // 逐台判断是否参与本轮：未启用/FOUP 到位记为跳过，未连接/启动失败记为失败。
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
            continue;
        }

        if (foupInfo && foupInfo->foupIn()) {
            DeviceResult& result = m_roundResults[qrcode];
            result.description = QStringLiteral("FOUP in place");
            SH85SelfCheckLogHelper::appendSkip(m_deviceRecords[qrcode], result.description);
            SH85SelfCheckLogHelper::finishSkippedRecord(m_deviceRecords[qrcode], result.description);
            SH85SelfCheckLogHelper::writeRecord(m_deviceRecords[qrcode], false);
            continue;
        }

        DeviceResult& result = m_roundResults[qrcode];
        result.participated = true;
        result.description.clear();

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

        // 只有真正启动 checker 的设备进入 pending，后续由 finished 信号结束。
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
    // 只做筛选，不改变本轮日志上下文；供 UI 或外部查询当前可执行设备。
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

    // 防止 start() 返回 false 后又收到 queued finished 等重复回调导致重复写日志。
    if (m_deviceRecords.contains(qrcode) && m_deviceRecords[qrcode].taskEndTime().isValid()) {
        return;
    }

    DeviceResult& deviceResult = m_roundResults[qrcode];
    deviceResult.qrcode = qrcode;
    deviceResult.participated = true;
    deviceResult.success = success;

    m_pendingQrcodes.remove(qrcode);

    QString normalizedDescription = description;
    if (m_deviceRecords.contains(qrcode)) {
        SH85SelfCheckLogHelper::finishRecord(m_deviceRecords[qrcode],
                                             success,
                                             result,
                                             description);
        normalizedDescription = m_deviceRecords[qrcode].description();
        // 设备结束时写一条完整明细；失败写 warn，取消不作为告警。
        const bool warn = !success && result != SH85SelfChecker::Result::Cancelled;
        SH85SelfCheckLogHelper::writeRecord(m_deviceRecords[qrcode], warn);
    }

    deviceResult.description = normalizedDescription;
    emit oneFinished(qrcode, success, normalizedDescription);
}

void SH85PeriodicSelfCheckTask2::tryFinishRound()
{
    if (!m_roundActive || !m_pendingQrcodes.isEmpty()) {
        return;
    }

    // 整轮结束只写汇总/失败告警/分隔线，避免把所有设备明细重复灌进汇总日志。
    SH85SelfCheckLogHelper::RoundSummary summary;
    summary.roundId = m_roundId;
    summary.startTime = m_roundStartTime;
    summary.endTime = currentTimestamp();
    summary.totalDevices = m_roundOrderedQrcodes.size();

    QList<SH85SelfCheckTaskRecord> orderedRecords;
    orderedRecords.reserve(m_roundOrderedQrcodes.size());

    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        const DeviceResult result = m_roundResults.value(qrcode);
        if (result.participated) {
            ++summary.participatedCount;
            if (result.success) {
                ++summary.successCount;
            } else {
                ++summary.failureCount;
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
    SH85SelfCheckLogHelper::writeTaskSeparator(SH85SelfCheckLogHelper::Mode::Periodic,
                                               summary.roundId,
                                               QString(),
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

    emit allDevicesFinished(summary.totalDevices, summary.successCount, summary.failureCount);

    m_roundActive = false;
    disconnectAllCheckers();
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

    // 网络未连接、checker 缺失、checker 启动失败等场景没有真实命令回调，
    // 这里补一段“未下发”命令明细，保证设备日志仍然完整。
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

    // 倒计时属于 UI 状态，不写入文件日志，避免 1Hz 噪声。
    emit countdownTick(remainingSeconds, masterId);
}

void SH85PeriodicSelfCheckTask2::onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId)
{
    if (!m_roundActive || !m_pendingQrcodes.contains(masterId)) {
        return;
    }

    // 阶段变化用于后续命令明细归属到对应自检阶段。
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
        const auto state = m_checkerStates.value(masterId, SH85SelfChecker::State::Idle);
        SH85SelfCheckLogHelper::appendCommand(m_deviceRecords[masterId], state, cmd);
    }

    // 一方面追加到设备自检 record，一方面保留原有通讯日志数据库落库能力。
    emit commandCompleted(cmd, masterId);
    writeCommunicateLog(cmd, masterId);
}

void SH85PeriodicSelfCheckTask2::onCheckerCommandRetrying(ModbusCommand cmd, const QString& masterId)
{
    if (m_roundActive && m_deviceRecords.contains(masterId)) {
        const auto state = m_checkerStates.value(masterId, SH85SelfChecker::State::Idle);
        SH85SelfCheckLogHelper::appendCommand(m_deviceRecords[masterId], state, cmd, true);
    }

    emit commandRetrying(cmd, masterId);
}

void SH85PeriodicSelfCheckTask2::onCheckerErrorOccurred(SH85SelfChecker::Result result,
                                                        const QString& message,
                                                        const QString& masterId)
{
    // errorOccurred 通常会紧接 finished，这里只补充异常过程，最终结果由 finishDevice 写出。
    if (m_roundActive && m_deviceRecords.contains(masterId)) {
        const QString normalizedMessage = SH85SelfCheckLogHelper::descriptionToChinese(result, message);
        m_deviceRecords[masterId].appendRecord(
            QStringLiteral("Self-check error\nResult: %1\nMessage: %2")
                .arg(SH85SelfChecker::resultToString(result), normalizedMessage));
    }

    emit errorOccurred(result, message, masterId);
}
