#include "sh85_periodic_self_check_task.h"

#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "loggermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QTimer>

// ====================================================================
// 静态注册
// ====================================================================
namespace {
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

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 创建任务";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] 创建任务").toStdString());
}

SH85PeriodicSelfCheckTask::~SH85PeriodicSelfCheckTask()
{
    if (m_tickTimer) m_tickTimer->stop();
    disconnectAllCheckers();
    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 任务销毁";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] 任务销毁").toStdString());
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
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] setEnabled=%1").arg(enabled).toStdString());

    if (auto* op = SharedData::getOperationDispatchTask()) {
        op->log(OperationDispatchTask::MsgType::Message,
                QString("Periodic SH85 self-check %1").arg(enabled ? "enabled" : "disabled"), 0);
    }

    if (enabled) {
        // 启用：立即执行一轮自检
        // - Stopped → 直接进入 Checking
        // - Checking / WaitingNext → 已在运行；让其自然推进即可
        if (m_state == State::Stopped) {
            enterChecking();
        }
    } else {
        // 停用：
        // - Checking → 不打断本轮，本轮结束后由 tryEndRound() 进入 Stopped
        // - WaitingNext → 立即停止
        // - Stopped → 无操作
        if (m_state == State::WaitingNext || m_state == State::Stopped) {
            enterStopped();
        } else {
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] setEnabled(false) 当前正在 Checking, "
                        "等待本轮完成后再转 Stopped";
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
                QString("[Scheduler][SH85PeriodicSelfCheckTask] disable pending: wait current round to finish")
                    .toStdString());
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
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] setPeriod=%1 %2 (=%3s)")
            .arg(value).arg(timeUnitToString(unit)).arg(totalSec).toStdString());

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
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] 任务启动 period=%1s").arg(m_periodSec).toStdString());

    // 默认未启用；等待 UI 调用 setEnabled(true)
    if (m_enabled) {
        enterChecking();
    } else {
        enterStopped();
    }
}

void SH85PeriodicSelfCheckTask::stop()
{
    if (m_finishedEmitted) return;
    m_finishedEmitted = true;

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] stop() 调用";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] 任务停止").toStdString());

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
    m_roundOrderedQrcodes = SharedData::getAllQrcodes();
    m_pendingQrcodes.clear();
    m_roundResults.clear();
    disconnectAllCheckers();

    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 一轮开始, 总设备数=" << m_roundOrderedQrcodes.size();
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] 一轮开始 总设备数=%1")
            .arg(m_roundOrderedQrcodes.size()).toStdString());

    // 1) 初始化每个设备的结果（默认未参加）
    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        DeviceResult dr;
        dr.qrcode       = qrcode;
        dr.participated = false;
        dr.success      = false;
        dr.description  = QString("Not enabled");
        m_roundResults.insert(qrcode, dr);
    }

    // 2) 收集需要参加的设备 → 并行启动
    auto& mgr = ModbusTcpMasterManager::instance();
    for (const QString& qrcode : qAsConst(m_roundOrderedQrcodes)) {
        FoupOfOHBInfo* foupInfo = SharedData::getFoupByQRCode(qrcode);
        const bool enabled = (foupInfo && foupInfo->enable());

        if (!enabled) {
            // 不参加：保持初始状态，不进入 pending
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 跳过未启用设备 qrcode=" << qrcode;
            continue;
        }

        // 过滤：如果设备处于 foup in 状态，不参加自检
        if (foupInfo && foupInfo->foupIn()) {
            DeviceResult& dr = m_roundResults[qrcode];
            dr.description = QStringLiteral("FOUP in place, skipped");
            qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 跳过 FOUP in 状态设备 qrcode=" << qrcode;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
                QString("[Scheduler][SH85PeriodicSelfCheckTask] 跳过 FOUP in 状态设备 qrcode=%1").arg(qrcode).toStdString());
            emit deviceParticipated(qrcode, false);
            continue;
        }

        DeviceResult& dr = m_roundResults[qrcode];
        dr.participated = true;
        dr.description  = QString();

        ModbusTcpMaster* master = mgr.getMaster(qrcode);
        if (!master || !master->isConnected()) {
            // 网络未连接 → 直接判为失败
            const QString msg = QStringLiteral("Device not connected");
            qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask] 设备未连接 qrcode=" << qrcode;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][SH85PeriodicSelfCheckTask] 设备未连接 qrcode=%1").arg(qrcode).toStdString());
            if (auto* op = SharedData::getOperationDispatchTask()) {
                op->log(OperationDispatchTask::MsgType::Warn,
                        QString("Periodic SH85 self-check: device not connected, qrcode=%1").arg(qrcode), 0);
            }
            finishDevice(qrcode, false, msg);
            continue;
        }

        SH85SelfChecker* checker = master->selfChecker();
        if (!checker) {
            const QString msg = QStringLiteral("Self-checker is null");
            qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask] checker 为 null qrcode=" << qrcode;
            finishDevice(qrcode, false, msg);
            continue;
        }

        // 连接信号（每轮重新连接）
        connectChecker(checker);

        m_pendingQrcodes.insert(qrcode);

        if (!checker->start()) {
            const QString msg = QStringLiteral("Checker start failed");
            qWarning() << "[Scheduler][SH85PeriodicSelfCheckTask] checker->start() 返回 false qrcode=" << qrcode;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][SH85PeriodicSelfCheckTask] checker->start() 失败 qrcode=%1").arg(qrcode).toStdString());
            if (auto* op = SharedData::getOperationDispatchTask()) {
                op->log(OperationDispatchTask::MsgType::Warn,
                        QString("Periodic SH85 self-check: checker start failed, qrcode=%1").arg(qrcode), 0);
            }
            finishDevice(qrcode, false, msg);
            continue;
        }

        qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask] 启动设备自检 qrcode=" << qrcode;
    }

    // 若所有设备都已直接判定结束（无人参加 / 全部未连接），立即结束本轮
    tryEndRound();
}

void SH85PeriodicSelfCheckTask::finishDevice(const QString& qrcode,
                                             bool success,
                                             const QString& description)
{
    DeviceResult& dr = m_roundResults[qrcode];
    dr.qrcode       = qrcode;
    dr.success      = success;
    dr.description  = description;
    // participated 已在 beginRound 中设置

    m_pendingQrcodes.remove(qrcode);

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        success ? Level::INFO : Level::WARN,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] oneFinished qrcode=%1 success=%2 desc=%3")
            .arg(qrcode).arg(success).arg(description).toStdString());

    emit oneFinished(qrcode, success, description);
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
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85PeriodicSelfCheckTask] 一轮结束 success=%1 failure=%2")
            .arg(summary.successCount).arg(summary.failureCount).toStdString());

    emit allFinished(summary);

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
                                                  const QString& masterId)
{
    if (m_state != State::Checking) return;
    if (!m_pendingQrcodes.contains(masterId)) return;

    const QString desc = success
        ? QString("OK")
        : QString("%1: %2").arg(SH85SelfChecker::resultToString(result), message);

    finishDevice(masterId, success, desc);
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

    m_checkerConnections.append(c1);
    m_checkerConnections.append(c2);
    m_checkerConnections.append(c3);
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
