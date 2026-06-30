#include "sh85_periodic_self_check_task.h"
#include "sh85_self_check_log_helper.h"

#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "usermanager/usermanager.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>

// ====================================================================
// 静态注册
// ====================================================================
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

struct SH85PeriodicMetaTypeRegister {
    SH85PeriodicMetaTypeRegister() {
        qRegisterMetaType<SH85PeriodicSelfCheckTask::TimeUnit>("SH85PeriodicSelfCheckTask::TimeUnit");
        qRegisterMetaType<SH85PeriodicSelfCheckTask::State>("SH85PeriodicSelfCheckTask::State");
        qRegisterMetaType<SH85PeriodicSelfCheckTask::DeviceResult>("SH85PeriodicSelfCheckTask::DeviceResult");
        qRegisterMetaType<SH85PeriodicSelfCheckTask::SelfCheckSummary>("SH85PeriodicSelfCheckTask::SelfCheckSummary");
    }
};
static SH85PeriodicMetaTypeRegister s_sh85PeriodicMetaRegister;
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

SH85PeriodicSelfCheckTask::SH85PeriodicSelfCheckTask(QObject *parent)
    : SchedulerTask(parent)
{
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &SH85PeriodicSelfCheckTask::onIntervalTick);

    // 启动延时控制器
    m_bootDelay = new BootDelayTimer(this);
    connect(m_bootDelay, &BootDelayTimer::countdown,
            this, &SH85PeriodicSelfCheckTask::bootDelayCountdown);
    connect(m_bootDelay, &BootDelayTimer::timeout,
            this, &SH85PeriodicSelfCheckTask::onBootDelayTimeout);

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 创建任务";
}

SH85PeriodicSelfCheckTask::~SH85PeriodicSelfCheckTask()
{
    if (m_tickTimer) m_tickTimer->stop();
    disconnectAllCheckers();
    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 任务销毁";
}
void SH85PeriodicSelfCheckTask::onBootDelayTimeout()
{
    if (m_enabled) {
        enterChecking();
    } else {
        enterStopped();
    }
}

// ============================================================
// 公开控制接口
// ============================================================

void SH85PeriodicSelfCheckTask::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] setEnabled:" << enabled;

    if (auto* op = SharedData::getOperationDispatchTask()) {
        op->log(OperationDispatchTask::MsgType::Message,
                QString("Periodic SH85 self-check %1").arg(enabled ? "enabled" : "disabled"), 0);
    }

    if (enabled) {
        if (m_state == State::Stopped) {
            // 启用时，若当前不在启动延时内，则立即进入首轮 Checking
            if (!m_bootDelay || !m_bootDelay->isActive()) {
                enterChecking();
            }
        }
    }else {
        // 停用：
        // - Checking → 不打断本轮，本轮结束后由 tryEndRound() 进入 Stopped
        // - WaitingNext → 立即停止
        // - Stopped → 无操作
        if (m_state == State::WaitingNext || m_state == State::Stopped) {
            enterStopped();
        } else {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] setEnabled(false) 当前正在 Checking, "
                        "等待本轮完成后再转 Stopped";
        }
    }
}

void SH85PeriodicSelfCheckTask::setPeriod(int value, TimeUnit unit)
{
    if (value <= 0) value = 1;

    int totalSec = value;
    switch (unit) {
    case TimeUnit::Second: totalSec = value;          break;
    case TimeUnit::Minute: totalSec = value * 60;     break;
    case TimeUnit::Hour:   totalSec = value * 3600;   break;
    }
    m_periodSec = totalSec;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] setPeriod:" << value
             << timeUnitToString(unit) << "(" << totalSec << "s)";

    // WaitingNext 状态下立即重置倒计时
    if (m_state == State::WaitingNext) {
        m_intervalRemaining = m_periodSec;
        emit intervalCountdown(m_intervalRemaining);
    }
}

QString SH85PeriodicSelfCheckTask::timeUnitToString(TimeUnit u)
{
    switch (u) {
    case TimeUnit::Second: return "s";
    case TimeUnit::Minute: return "min";
    case TimeUnit::Hour:   return "hour";
    }
    return "?";
}

QString SH85PeriodicSelfCheckTask::stateToString(State s)
{
    switch (s) {
    case State::Stopped:     return "Stopped";
    case State::Checking:    return "Checking";
    case State::WaitingNext: return "WaitingNext";
    }
    return "Unknown";
}

// ============================================================
// SchedulerTask 接口
// ============================================================

void SH85PeriodicSelfCheckTask::start()
{
    setState(Running);
    m_finishedEmitted = false;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] start() period=" << m_periodSec << "s";

    // 启动延时倒计时（单位：秒）
    if (m_bootDelay) m_bootDelay->startSeconds(10);
}

void SH85PeriodicSelfCheckTask::stop()
{
    if (m_bootDelay) m_bootDelay->stop();
    enterStopped();

    if (m_finishedEmitted) return;
    m_finishedEmitted = true;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] stop() 调用";

    enterStopped();

    setState(Cancelled);
    emit finished(true, QStringLiteral("SH85PeriodicSelfCheckTask stopped"));
}

// ============================================================
// 状态切换
// ============================================================

void SH85PeriodicSelfCheckTask::enterStopped()
{
    if (m_tickTimer) m_tickTimer->stop();
    if (m_bootDelay) m_bootDelay->stop();

    // 取消所有进行中的 checker
    disconnectAllCheckers();
    for (const QString& qrcode : qAsConst(m_pendingQrcodes)) {
        auto* master = ModbusTcpMasterManager::instance().getMaster(qrcode);
        if (master && master->selfChecker() && master->selfChecker()->isRunning()) {
            master->selfChecker()->stop();
        }
    }
    m_pendingQrcodes.clear();
    m_roundResults.clear();
    m_roundOrderedQrcodes.clear();
    m_deviceRecords.clear();
    m_checkerStates.clear();
    m_roundId.clear();

    m_state             = State::Stopped;
    m_intervalRemaining = 0;
    m_elapsedSeconds    = 0;
    emit taskStateChanged(m_state);
}

void SH85PeriodicSelfCheckTask::enterWaitingNext()
{
    if (!m_enabled) {
        enterStopped();
        return;
    }

    m_state             = State::WaitingNext;
    m_intervalRemaining = m_periodSec;
    m_elapsedSeconds    = 0;
    emit taskStateChanged(m_state);
    emit intervalCountdown(m_intervalRemaining);

    if (m_tickTimer) m_tickTimer->start();
}

void SH85PeriodicSelfCheckTask::enterChecking()
{
    if (m_bootDelay && m_bootDelay->isActive()) return;

    if (!m_enabled) {
        enterStopped();
        return;
    }

    m_state          = State::Checking;
    m_elapsedSeconds = 0;
    emit taskStateChanged(m_state);
    emit elapsedTick(m_elapsedSeconds);

    // Checking 期间也用 1Hz 计时显示已执行秒数
    if (m_tickTimer) m_tickTimer->start();

    beginRound();
}

// ============================================================
// 1Hz tick
// ============================================================

void SH85PeriodicSelfCheckTask::onIntervalTick()
{
    if (m_state == State::WaitingNext) {
        if (m_intervalRemaining > 0) {
            --m_intervalRemaining;
            emit intervalCountdown(m_intervalRemaining);
        }
        if (m_intervalRemaining <= 0) {
            m_tickTimer->stop();
            // 切换到 Checking 之前重新检查 enabled
            if (m_enabled) {
                enterChecking();
            } else {
                enterStopped();
            }
        }
    } else if (m_state == State::Checking) {
        ++m_elapsedSeconds;
        emit elapsedTick(m_elapsedSeconds);
    }
}

// ============================================================
// 一轮自检（并行）
// ============================================================

void SH85PeriodicSelfCheckTask::beginRound()
{
    m_roundStartTime = currentTimestamp();
    m_roundId = SH85SelfCheckLogHelper::createRoundId();
    m_roundOrderedQrcodes = SharedData::getAllQrcodes();
    m_pendingQrcodes.clear();
    m_roundResults.clear();
    m_deviceRecords.clear();
    m_checkerStates.clear();
    disconnectAllCheckers();

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 一轮开始, 总设备数=" << m_roundOrderedQrcodes.size();

    // 1) 初始化每个设备的结果（默认未参加）
    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        DeviceResult dr;
        dr.qrcode       = qrcode;
        dr.participated = false;
        dr.success      = false;
        dr.description  = QString("Not enabled");
        m_roundResults.insert(qrcode, dr);

        SH85SelfCheckTaskRecord record;
        SH85SelfCheckLogHelper::initRecord(record,
                                           SH85SelfCheckLogHelper::Mode::Periodic,
                                           qrcode,
                                           m_roundId);
        SH85SelfCheckLogHelper::appendStart(record);
        m_deviceRecords.insert(qrcode, record);
        m_checkerStates.insert(qrcode, SH85SelfChecker::State::Idle);
    }

    // 2) 收集需要参加的设备 → 并行启动
    auto& mgr = ModbusTcpMasterManager::instance();
    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        FoupOfOHBInfo* foupInfo = SharedData::getFoupByQRCode(qrcode);
        const bool enabled = (foupInfo && foupInfo->enable());

        if (!enabled) {
            // 不参加：保持初始状态，不进入 pending
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 跳过未启用设备 qrcode=" << qrcode;
            SH85SelfCheckLogHelper::appendSkip(m_deviceRecords[qrcode], QStringLiteral("设备未启用"));
            SH85SelfCheckLogHelper::finishSkippedRecord(m_deviceRecords[qrcode], QStringLiteral("设备未启用"));
            continue;
        }

        // 过滤：如果设备处于 foup in 状态，不参加自检
        if (foupInfo && foupInfo->foupIn()) {
            DeviceResult& dr = m_roundResults[qrcode];
            dr.description = QStringLiteral("FOUP 到位，跳过");
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 跳过 FOUP in 状态设备 qrcode=" << qrcode;
            SH85SelfCheckLogHelper::appendSkip(m_deviceRecords[qrcode], QStringLiteral("FOUP 到位"));
            SH85SelfCheckLogHelper::finishSkippedRecord(m_deviceRecords[qrcode], QStringLiteral("FOUP 到位"));
            emit deviceParticipated(qrcode, false);
            continue;
        }

        DeviceResult& dr = m_roundResults[qrcode];
        dr.participated = true;
        dr.description  = QString();

        ModbusTcpMaster* master = mgr.getMaster(qrcode);
        if (!master || !master->isConnected()) {
            // 网络未连接 → 直接判为失败
            const QString msg = QStringLiteral("设备未连接");
            qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask] 设备未连接 qrcode=" << qrcode;
            SH85SelfCheckLogHelper::appendCommandNotSubmitted(
                m_deviceRecords[qrcode],
                SH85SelfChecker::State::StartingSelfCheck,
                startSelfCheckTemplateCommand(),
                msg);
            m_deviceRecords[qrcode].appendRecord(QStringLiteral("启动失败\n原因: 设备未连接"));
            finishDevice(qrcode, false, SH85SelfChecker::Result::StartCommandFailed, msg);
            continue;
        }

        SH85SelfChecker* checker = master->selfChecker();
        if (!checker) {
            const QString msg = QStringLiteral("SelfChecker 为空");
            qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask] checker 为 null qrcode=" << qrcode;
            SH85SelfCheckLogHelper::appendCommandNotSubmitted(
                m_deviceRecords[qrcode],
                SH85SelfChecker::State::StartingSelfCheck,
                startSelfCheckTemplateCommand(),
                msg);
            m_deviceRecords[qrcode].appendRecord(QStringLiteral("启动失败\n原因: SelfChecker 为空"));
            finishDevice(qrcode, false, SH85SelfChecker::Result::StartCommandFailed, msg);
            continue;
        }

        // 连接信号（每轮重新连接）
        connectChecker(checker);

        m_pendingQrcodes.insert(qrcode);

        if (!checker->start()) {
            const QString msg = QStringLiteral("自检器启动失败");
            qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask] checker->start() 返回 false qrcode=" << qrcode;
            SH85SelfCheckLogHelper::appendCommandNotSubmitted(
                m_deviceRecords[qrcode],
                SH85SelfChecker::State::StartingSelfCheck,
                startSelfCheckTemplateCommand(),
                msg);
            m_deviceRecords[qrcode].appendRecord(QStringLiteral("启动失败\n原因: checker 启动失败"));
            finishDevice(qrcode, false, SH85SelfChecker::Result::StartCommandFailed, msg);
            continue;
        }

        m_deviceRecords[qrcode].appendRecord(QStringLiteral("自检器启动成功"));
        qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 启动设备自检 qrcode=" << qrcode;
    }

    // 若所有设备都已直接判定结束（无人参加 / 全部未连接），立即结束本轮
    tryEndRound();
}

void SH85PeriodicSelfCheckTask::finishDevice(const QString& qrcode,
                                             bool success,
                                             SH85SelfChecker::Result result,
                                             const QString& description)
{
    DeviceResult& dr = m_roundResults[qrcode];
    dr.qrcode       = qrcode;
    dr.success      = success;
    // participated 已在 beginRound 中设置

    m_pendingQrcodes.remove(qrcode);
    QString normalizedDescription = description;
    if (m_deviceRecords.contains(qrcode)) {
        SH85SelfCheckLogHelper::finishRecord(
            m_deviceRecords[qrcode],
            success,
            result,
            description);
        normalizedDescription = m_deviceRecords[qrcode].description();
    }

    dr.description = normalizedDescription;
    emit oneFinished(qrcode, success, normalizedDescription);
}

void SH85PeriodicSelfCheckTask::tryEndRound()
{
    if (!m_pendingQrcodes.isEmpty()) {
        return;
    }

    // 汇总
    SelfCheckSummary summary;
    summary.startTime = m_roundStartTime;
    summary.endTime   = currentTimestamp();
    summary.successCount = 0;
    summary.failureCount = 0;
    summary.details.reserve(m_roundOrderedQrcodes.size());

    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        const DeviceResult& dr = m_roundResults.value(qrcode);
        summary.details.append(dr);
        if (dr.participated) {
            if (dr.success) ++summary.successCount;
            else            ++summary.failureCount;
        }
    }

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 一轮结束 success=" << summary.successCount
             << "failure=" << summary.failureCount;

    emit allFinished(summary);

    SH85SelfCheckLogHelper::RoundSummary logSummary;
    logSummary.roundId = m_roundId;
    logSummary.startTime = summary.startTime;
    logSummary.endTime = summary.endTime;
    logSummary.totalDevices = summary.details.size();
    logSummary.successCount = summary.successCount;
    logSummary.failureCount = summary.failureCount;

    QList<SH85SelfCheckTaskRecord> orderedRecords;
    orderedRecords.reserve(summary.details.size());

    for (const DeviceResult& dr : qAsConst(summary.details)) {
        if (dr.participated) {
            ++logSummary.participatedCount;
            if (!dr.success) {
                logSummary.failedDevices.append(dr.qrcode);
            }
        } else {
            ++logSummary.skippedCount;
            logSummary.skippedDevices.append(dr.qrcode);
        }

        if (m_deviceRecords.contains(dr.qrcode)) {
            orderedRecords.append(m_deviceRecords.value(dr.qrcode));
        }
    }

    SH85SelfCheckLogHelper::writeRoundReport(logSummary, orderedRecords);
    SH85SelfCheckLogHelper::writeRoundWarning(logSummary, orderedRecords);

    disconnectAllCheckers();

    // 进入下一次等待
    if (m_enabled) {
        enterWaitingNext();
    } else {
        enterStopped();
    }
}

// ============================================================
// SH85SelfChecker 信号回调
// ============================================================

void SH85PeriodicSelfCheckTask::onCheckerFinished(bool success,
                                                  SH85SelfChecker::Result result,
                                                  const QString& message,
                                                  const QString& masterId,
                                                  double minimumHumidity)
{
    Q_UNUSED(minimumHumidity)
    if (m_state != State::Checking) return;
    if (!m_pendingQrcodes.contains(masterId)) return;

    const QString desc = success
        ? QStringLiteral("成功")
        : (message.isEmpty() ? SH85SelfCheckLogHelper::resultToChineseText(result) : message);

    finishDevice(masterId, success, result, desc);
    tryEndRound();
}

void SH85PeriodicSelfCheckTask::onCheckerCountdown(int remainingSeconds, const QString& masterId)
{
    if (m_state != State::Checking) return;
    emit countdownTick(remainingSeconds, masterId);
}

void SH85PeriodicSelfCheckTask::onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId)
{
    if (m_state != State::Checking) return;

    m_checkerStates[masterId] = state;
    if (state != SH85SelfChecker::State::Done && m_deviceRecords.contains(masterId)) {
        SH85SelfCheckLogHelper::appendStage(m_deviceRecords[masterId], state);
    }

    emit selfCheckerStateChanged(state, masterId);
}

// ============================================================
// 辅助
// ============================================================

void SH85PeriodicSelfCheckTask::connectChecker(SH85SelfChecker* checker)
{
    if (!checker) return;

    auto c1 = connect(checker, &SH85SelfChecker::finished,
                      this, &SH85PeriodicSelfCheckTask::onCheckerFinished,
                      Qt::QueuedConnection);
    auto c2 = connect(checker, &SH85SelfChecker::countdownTick,
                      this, &SH85PeriodicSelfCheckTask::onCheckerCountdown,
                      Qt::QueuedConnection);
    auto c3 = connect(checker, &SH85SelfChecker::stateChanged,
                      this, &SH85PeriodicSelfCheckTask::onCheckerStateChanged,
                      Qt::QueuedConnection);

    auto c4 = connect(checker, &SH85SelfChecker::commandCompleted,
                      this, &SH85PeriodicSelfCheckTask::onCommandCompleted,
                      Qt::QueuedConnection);
    auto c5 = connect(checker, &SH85SelfChecker::commandRetrying,
                      this, &SH85PeriodicSelfCheckTask::onCommandRetrying,
                      Qt::QueuedConnection);

    m_checkerConnections.append(c1);
    m_checkerConnections.append(c2);
    m_checkerConnections.append(c3);
    m_checkerConnections.append(c4);
    m_checkerConnections.append(c5);
}

void SH85PeriodicSelfCheckTask::onCommandCompleted(ModbusCommand cmd, const QString& masterId)
{
    if (m_deviceRecords.contains(masterId)) {
        const auto state = m_checkerStates.value(masterId, SH85SelfChecker::State::Idle);
        SH85SelfCheckLogHelper::appendCommand(m_deviceRecords[masterId], state, cmd);
    }

    // 写入通讯日志数据库
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("-");
    int execStatus = 3;
    if (cmd.received)           execStatus = 0;
    else if (cmd.timedOut)      execStatus = 1;
    else if (cmd.sendCount > 1) execStatus = 2;
    const int retryCount = qMax(0, cmd.sendCount - 1);
    QString description;
    if (execStatus != 0) {
        description = cmd.errorMessage;
    } else {
        QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (!parsedData.isEmpty()) {
            QStringList parts;
            for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it)
                parts << QString("%1=%2").arg(it.key(), it.value().toString());
            description = parts.join(", ");
        }
    }
    if (description.isEmpty()) {
        description = QStringLiteral("OK");
    }
    if (LogDB::CommunicateLogDBCon *db = LogDB::DatabaseManager::instance().communicateLogCon()) {
        const QString respTimeStr = cmd.responseMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
            : QString();
        db->insertRecord(sentTimeStr, respTimeStr, cmd.id, masterId,
                         execStatus, retryCount,
                         cmd.request.rawBytes, cmd.response.rawBytes, description,
                         UserPermission::Engineer);
    }
}

void SH85PeriodicSelfCheckTask::onCommandRetrying(ModbusCommand cmd, const QString& masterId)
{
    if (m_deviceRecords.contains(masterId)) {
        const auto state = m_checkerStates.value(masterId, SH85SelfChecker::State::Idle);
        SH85SelfCheckLogHelper::appendCommand(m_deviceRecords[masterId], state, cmd, true);
    }
}

void SH85PeriodicSelfCheckTask::disconnectAllCheckers()
{
    for (const QMetaObject::Connection& c : qAsConst(m_checkerConnections)) {
        QObject::disconnect(c);
    }
    m_checkerConnections.clear();
}

QString SH85PeriodicSelfCheckTask::currentTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
}

QString SH85PeriodicSelfCheckTask::resultToChineseText(SH85SelfChecker::Result r)
{
    using Result = SH85SelfChecker::Result;
    switch (r) {
    case Result::Success:              return QStringLiteral("自检成功");
    case Result::StartCommandFailed:   return QStringLiteral("启动自检指令下发失败");
    case Result::ReadEarlyCommandFailed: return QStringLiteral("预检读取指令下发失败");
    case Result::DeviceNotEntered:     return QStringLiteral("设备未进入自检状态");
    case Result::FirmwareAbnormal:     return QStringLiteral("底层固件状态异常");
    case Result::ReadPollCommandFailed: return QStringLiteral("轮询读取指令下发失败");
    case Result::HumidityExceeded:     return QStringLiteral("湿度超标");
    case Result::SensorCommError:      return QStringLiteral("SH85传感器通讯故障");
    case Result::ThresholdParamError:  return QStringLiteral("阈值参数错误（湿度下限阈值≤0）");
    case Result::Timeout:              return QStringLiteral("轮询窗口超时，未获取到终态值");
    case Result::Cancelled:            return QStringLiteral("用户取消");
    }
    return QStringLiteral("未知结果");
}

QString SH85PeriodicSelfCheckTask::stateToChineseText(SH85SelfChecker::State s)
{
    switch (s) {
    case SH85SelfChecker::State::Idle:              return QStringLiteral("空闲");
    case SH85SelfChecker::State::StartingSelfCheck:  return QStringLiteral("阶段1 - 下发启动自检指令");
    case SH85SelfChecker::State::WaitingPhase1:      return QStringLiteral("阶段1 - 等待5秒");
    case SH85SelfChecker::State::ReadingStatusEarly:  return QStringLiteral("阶段2 - 预检读取自检状态");
    case SH85SelfChecker::State::WaitingPhase2:      return QStringLiteral("阶段2 - 等待55秒");
    case SH85SelfChecker::State::PollingStatus:      return QStringLiteral("阶段3 - 轮询读取自检结果（10秒窗口）");
    case SH85SelfChecker::State::Done:               return QStringLiteral("完成");
    }
    return QStringLiteral("未知状态");
}
