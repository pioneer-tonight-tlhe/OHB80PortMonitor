#include "sh85selfchecker.h"

#include "modbustcpmaster.h"
#include "modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"

#include "modbuslogger.h"

#include <QDebug>

namespace {
constexpr const char* kCmdStart = "StartSelfCheck";
constexpr const char* kCmdRead  = "ReadSelfCheckStatus";

constexpr quint16 kStatusIdle           = 0;
constexpr quint16 kStatusInProgress     = 1;
constexpr quint16 kStatusSuccess        = 2;
constexpr quint16 kStatusHumidityFail   = 3;
constexpr quint16 kStatusSensorCommFail = 4;
constexpr quint16 kStatusThresholdParam = 5;

// 将原始字节 + CRC 拼接后转成大写空格分隔的 HEX 字符串（用于日志）
inline QString frameToHex(const ModbusFrame& f)
{
    QByteArray bytes = f.rawBytes;
    bytes.append(f.crc);
    return QString::fromLatin1(bytes.toHex(' ').toUpper());
}
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

SH85SelfChecker::SH85SelfChecker(ModbusTcpMaster* master, QObject* parent)
    : QObject(parent ? parent : master)
    , m_master(master)
{
    Q_ASSERT(m_master);

    m_phase1Timer = new QTimer(this);
    m_phase1Timer->setSingleShot(true);
    m_phase1Timer->setInterval(kPhase1WaitMs);
    connect(m_phase1Timer, &QTimer::timeout, this, &SH85SelfChecker::onPhase1WaitElapsed);

    m_phase2Timer = new QTimer(this);
    m_phase2Timer->setSingleShot(true);
    m_phase2Timer->setInterval(kPhase2WaitMs);
    connect(m_phase2Timer, &QTimer::timeout, this, &SH85SelfChecker::onPhase2WaitElapsed);

    m_pollWindowTimer = new QTimer(this);
    m_pollWindowTimer->setSingleShot(true);
    m_pollWindowTimer->setInterval(kPollWindowMs);
    connect(m_pollWindowTimer, &QTimer::timeout, this, &SH85SelfChecker::onPollWindowElapsed);

    m_countdownTimer = new QTimer(this);
    m_countdownTimer->setInterval(kCountdownTickMs);
    connect(m_countdownTimer, &QTimer::timeout, this, &SH85SelfChecker::onCountdownTick);
}

SH85SelfChecker::~SH85SelfChecker()
{
    cleanup();
}

// ============================================================
// 公开接口
// ============================================================

bool SH85SelfChecker::start()
{
    if (isRunning()) {
        qWarning() << "[data][SH85SelfChecker] 已在运行中，忽略重复 start()，masterId=" << m_master->ID;
        return false;
    }

    if (!m_master || !m_master->isConnected() || !m_master->sender()) {
        qWarning() << "[data][SH85SelfChecker] master 未就绪，无法启动自检 masterId="
                   << (m_master ? m_master->ID : QString("<null>"));
        return false;
    }

    // 校验指令池存在所需指令
    CommandPool* pool = ModbusTcpMasterManager::instance().commandPool();
    if (!pool || !pool->contains(kCmdStart) || !pool->contains(kCmdRead)) {
        qWarning() << "[data][SH85SelfChecker] CommandPool 缺少 StartSelfCheck 或 ReadSelfCheckStatus";
        ModbusLogger::masterWarn(m_master->ID, "ModbusTcpMaster", "SH85SelfChecker", "start",
            "CommandPool 缺少 StartSelfCheck 或 ReadSelfCheckStatus");
        return false;
    }

    m_finished     = false;
    m_pendingUuid  = 0;

    // 建立 sender 信号槽（QueuedConnection 保证跨线程安全）
    if (m_senderConn) QObject::disconnect(m_senderConn);
    m_senderConn = connect(m_master->sender(), &ModbusCommandSender::commandFinished,
                           this, &SH85SelfChecker::onCommandFinished, Qt::QueuedConnection);
    if (m_senderRetryConn) QObject::disconnect(m_senderRetryConn);
    m_senderRetryConn = connect(m_master->sender(), &ModbusCommandSender::commandTimeoutRetry,
                                this, &SH85SelfChecker::onCommandTimeoutRetry, Qt::QueuedConnection);

    // 下发 StartSelfCheck
    if (!submitStartSelfCheck()) {
        cleanup();
        emitErrorAndFinish(Result::StartCommandFailed, "Failed to submit start self-check command");
        return false;
    }

    // 将倒计时定时器启动派发到对象所属线程，避免跨线程直接调用导致定时器不触发
    QMetaObject::invokeMethod(this, "startCountdownTimer", Qt::QueuedConnection);

    enterState(State::StartingSelfCheck);
    emit started(m_master->ID);

    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "SH85SelfChecker", "start", "自检流程已启动");
    return true;
}

void SH85SelfChecker::stop()
{
    if (!isRunning()) return;
    qDebug() << "[data][SH85SelfChecker] stop() 被调用 masterId=" << (m_master ? m_master->ID : QString());
    finishOnly(false, Result::Cancelled, "Cancelled by user");
}

QString SH85SelfChecker::stateToString(State s)
{
    switch (s) {
    case State::Idle:                return "空闲";
    case State::StartingSelfCheck:   return "下发自检指令中";
    case State::WaitingPhase1:       return "等待 5s（阶段1前）";
    case State::ReadingStatusEarly:  return "阶段1读取自检状态中";
    case State::WaitingPhase2:       return "等待 55s（阶段2前）";
    case State::PollingStatus:       return "轮询自检状态中";
    case State::Done:                return "结束";
    }
    return "未知";
}

QString SH85SelfChecker::resultToString(Result r)
{
    switch (r) {
    case Result::Success:                return "Success";
    case Result::StartCommandFailed:     return "StartCommandFailed";
    case Result::ReadEarlyCommandFailed: return "ReadEarlyCommandFailed";
    case Result::DeviceNotEntered:       return "DeviceNotEntered";
    case Result::FirmwareAbnormal:       return "FirmwareAbnormal";
    case Result::ReadPollCommandFailed:  return "ReadPollCommandFailed";
    case Result::HumidityExceeded:       return "HumidityExceeded";
    case Result::SensorCommError:        return "SensorCommError";
    case Result::ThresholdParamError:    return "ThresholdParamError";
    case Result::Timeout:                return "Timeout";
    case Result::Cancelled:              return "Cancelled";
    }
    return "Unknown";
}

// ============================================================
// 指令下发
// ============================================================

bool SH85SelfChecker::submitStartSelfCheck()
{
    if (!m_master || !m_master->sender()) return false;

    CommandPool* pool = ModbusTcpMasterManager::instance().commandPool();
    if (!pool) return false;

    ModbusCommand cmd = pool->clone(kCmdStart);
    if (!cmd.isValid()) return false;

    cmd.module        = CommandModule::BusinessCommandIssuer;
    cmd.maxRetryCount = 0;   // StartSelfCheck 不允许超时重发

    m_pendingUuid = cmd.uuid;

    ModbusCommandSender* sender = m_master->sender();
    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);

    qDebug() << "[data][SH85SelfChecker] 下发 StartSelfCheck uuid=" << cmd.uuid
             << "masterId=" << m_master->ID;

    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "SH85SelfChecker", "submitStartSelfCheck",
        QString("下发 StartSelfCheck uuid=%1").arg(cmd.uuid));

    return true;
}

bool SH85SelfChecker::submitReadStatus()
{
    if (!m_master || !m_master->sender()) return false;

    CommandPool* pool = ModbusTcpMasterManager::instance().commandPool();
    if (!pool) return false;

    ModbusCommand cmd = pool->clone(kCmdRead);
    if (!cmd.isValid()) return false;

    cmd.module = CommandModule::BusinessCommandIssuer;

    m_pendingUuid = cmd.uuid;

    ModbusCommandSender* sender = m_master->sender();
    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);

    qDebug() << "[data][SH85SelfChecker] 下发 ReadSelfCheckStatus uuid=" << cmd.uuid
             << "masterId=" << m_master->ID;

    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "SH85SelfChecker", "submitReadStatus",
        QString("下发 ReadSelfCheckStatus uuid=%1").arg(cmd.uuid));

    return true;
}

// ============================================================
// 槽：指令完成
// ============================================================

void SH85SelfChecker::onCommandFinished(ModbusCommand cmd, const QString& masterId)
{
    if (!isRunning()) return;
    if (!m_master || masterId != m_master->ID) return;
    if (cmd.uuid == 0 || cmd.uuid != m_pendingUuid) return;

    m_pendingUuid = 0;

    // 通知 Task 层写通讯日志（Data 层不直接操作 communicatelogdb）
    emit commandCompleted(cmd, masterId);

    const bool ok = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    qDebug() << "[data][SH85SelfChecker] 响应 state=" << stateToString(m_state)
             << "ok=" << ok << "id=" << cmd.id << "masterId=" << masterId;

    const QString txHex = frameToHex(cmd.request);
    const QString rxHex = frameToHex(cmd.response);

    if (ok) {
        ModbusLogger::masterInfo(masterId, "ModbusTcpMaster", "SH85SelfChecker", "onCommandFinished",
            QString("响应 state=%1 ok=%2 id=%3 uuid=%4\nTX：%5\nRX：%6")
                .arg(stateToString(m_state)).arg(ok).arg(cmd.id).arg(cmd.uuid).arg(txHex).arg(rxHex));
    } else {
        ModbusLogger::masterWarn(masterId, "ModbusTcpMaster", "SH85SelfChecker", "onCommandFinished",
            QString("响应 state=%1 ok=%2 id=%3 uuid=%4 error=%5\nTX：%6\nRX：%7")
                .arg(stateToString(m_state)).arg(ok).arg(cmd.id).arg(cmd.uuid).arg(cmd.errorMessage).arg(txHex).arg(rxHex));
    }

    switch (m_state) {
    // -------- 1) StartSelfCheck 响应 --------
    case State::StartingSelfCheck: {
        if (!ok) {
            emitErrorAndFinish(Result::StartCommandFailed,
                QString("Start self-check command failed: %1").arg(cmd.errorMessage));
            return;
        }
        // 进入阶段 1 等待
        enterState(State::WaitingPhase1);
        m_phase1Timer->start();
        return;
    }

    // -------- 2) 阶段 1 ReadSelfCheckStatus 响应 --------
    case State::ReadingStatusEarly: {
        if (!ok) {
            emitErrorAndFinish(Result::ReadEarlyCommandFailed,
                QString("Phase 1 read status command failed: %1").arg(cmd.errorMessage));
            return;
        }
        const quint16 v = parseStatusValue(cmd);
        if (v == kStatusIdle) {
            // CH_1 == 0：未进入自检功能
            emitErrorAndFinish(Result::DeviceNotEntered, "Device not entered self-check state (CH_1=0)");
            return;
        }
        if (v != kStatusInProgress) {
            // CH_1 != 0 && != 1：底层固件异常
            emitErrorAndFinish(Result::FirmwareAbnormal,
                QString("Phase 1 firmware status abnormal, CH_1=%1").arg(v));
            return;
        }
        // CH_1 == 1：进入阶段 2 等待 55s
        enterState(State::WaitingPhase2);
        m_phase2Timer->start();
        return;
    }

    // -------- 3) 阶段 2 ReadSelfCheckStatus 轮询响应 --------
    case State::PollingStatus: {
        if (!ok) {
            emitErrorAndFinish(Result::ReadPollCommandFailed,
                QString("Phase 2 read status command failed: %1").arg(cmd.errorMessage));
            return;
        }
        const quint16 v = parseStatusValue(cmd);
        switch (v) {
        case kStatusIdle:
            // CH_1 == 0：自检中却返回空闲，固件异常
            emitErrorAndFinish(Result::FirmwareAbnormal,
                "Firmware status abnormal during polling, CH_1=0");
            return;
        case kStatusInProgress:
            // 仍在自检中：若 10s 窗口未超时，继续轮询
            if (!m_pollWindowTimer->isActive()) {
                emitErrorAndFinish(Result::Timeout, "Self-check timeout");
                return;
            }
            if (!submitReadStatus()) {
                emitErrorAndFinish(Result::ReadPollCommandFailed, "Failed to submit read status command");
            }
            return;
        case kStatusSuccess:
            // CH_1 == 2：自检成功
            finishOnly(true, Result::Success, "Self-check passed");
            return;
        case kStatusHumidityFail:
            emitErrorAndFinish(Result::HumidityExceeded, "Humidity exceeded (CH_1=3)");
            return;
        case kStatusSensorCommFail:
            emitErrorAndFinish(Result::SensorCommError, "SH85 sensor communication error (CH_1=4)");
            return;
        case kStatusThresholdParam:
            emitErrorAndFinish(Result::ThresholdParamError, "Threshold parameter error (CH_1=5)");
            return;
        default:
            emitErrorAndFinish(Result::FirmwareAbnormal,
                QString("Firmware status abnormal during polling, unknown CH_1=%1").arg(v));
            return;
        }
    }

    default:
        // 其他状态收到响应：忽略
        return;
    }
}

void SH85SelfChecker::onCommandTimeoutRetry(ModbusCommand cmd, const QString& masterId)
{
    if (!isRunning()) return;
    if (!m_master || masterId != m_master->ID) return;
    if (cmd.uuid == 0 || cmd.uuid != m_pendingUuid) return;

    emit commandRetrying(cmd, masterId);
}

// ============================================================
// 槽：定时器到期
// ============================================================

void SH85SelfChecker::onPhase1WaitElapsed()
{
    if (m_state != State::WaitingPhase1) return;

    const QString id = m_master ? m_master->ID : QString();
    ModbusLogger::masterInfo(id, "ModbusTcpMaster", "SH85SelfChecker", "onPhase1WaitElapsed",
        "阶段1等待5s到期");

    enterState(State::ReadingStatusEarly);
    if (!submitReadStatus()) {
        emitErrorAndFinish(Result::ReadEarlyCommandFailed,
            "Failed to submit phase 1 read status command");
    }
}

void SH85SelfChecker::onPhase2WaitElapsed()
{
    if (m_state != State::WaitingPhase2) return;

    const QString id = m_master ? m_master->ID : QString();
    ModbusLogger::masterInfo(id, "ModbusTcpMaster", "SH85SelfChecker", "onPhase2WaitElapsed",
        "阶段2等待55s到期，开始轮询");

    enterState(State::PollingStatus);
    // 启动 10s 轮询窗口；首发 ReadSelfCheckStatus
    m_pollWindowTimer->start();
    if (!submitReadStatus()) {
        emitErrorAndFinish(Result::ReadPollCommandFailed,
            "Failed to submit phase 2 read status command");
    }
}

void SH85SelfChecker::onPollWindowElapsed()
{
    if (m_state != State::PollingStatus) return;

    const QString id = m_master ? m_master->ID : QString();
    ModbusLogger::masterWarn(id, "ModbusTcpMaster", "SH85SelfChecker", "onPollWindowElapsed",
        "10s轮询窗口超时");

    // 10s 轮询窗口结束：若仍未拿到终态值，则视为自检功能超时
    // 注：若此刻刚好有响应未回，可在 onCommandFinished 中检查 m_pollWindowTimer->isActive() 判定
    if (m_pendingUuid != 0) {
        // 已下发但还未回响应：以超时收尾
        emitErrorAndFinish(Result::Timeout, "Self-check timeout (no response within 10s window)");
        return;
    }
    emitErrorAndFinish(Result::Timeout, "Self-check timeout");
}

// ============================================================
// 内部辅助
// ============================================================

quint16 SH85SelfChecker::parseStatusValue(const ModbusCommand& cmd) const
{
    const QByteArray& v = cmd.response.registerValue;
    if (v.size() < 2) return 0xFFFF;
    return (static_cast<quint16>(static_cast<quint8>(v[0])) << 8)
         |  static_cast<quint16>(static_cast<quint8>(v[1]));
}

void SH85SelfChecker::enterState(State s)
{
    if (m_state == s) return;
    State old = m_state;
    m_state = s;
    qDebug() << "[data][SH85SelfChecker] 状态切换:"
             << stateToString(old) << "->" << stateToString(s)
             << "masterId=" << (m_master ? m_master->ID : QString());

    const QString id = m_master ? m_master->ID : QString();
    ModbusLogger::masterInfo(id, "ModbusTcpMaster", "SH85SelfChecker", "enterState",
        QString("状态切换: %1 -> %2").arg(stateToString(old), stateToString(s)));

    emit stateChanged(s, id);
}

void SH85SelfChecker::emitErrorAndFinish(Result r, const QString& msg)
{
    if (m_finished) return;
    const QString id = m_master ? m_master->ID : QString();

    ModbusLogger::masterWarn(id, "ModbusTcpMaster", "SH85SelfChecker", "emitErrorAndFinish",
        QString("自检失败 result=%1 msg=%2").arg(resultToString(r), msg));

    emit errorOccurred(r, msg, id);
    finishOnly(false, r, msg);
}

void SH85SelfChecker::finishOnly(bool success, Result r, const QString& msg)
{
    if (m_finished) return;
    m_finished = true;

    const QString id = m_master ? m_master->ID : QString();
    cleanup();
    enterState(State::Done);

    if (success) {
        ModbusLogger::masterInfo(id, "ModbusTcpMaster", "SH85SelfChecker", "finishOnly",
            QString("自检完成 success=%1 result=%2 msg=%3").arg(success).arg(resultToString(r), msg));
    } else {
        ModbusLogger::masterWarn(id, "ModbusTcpMaster", "SH85SelfChecker", "finishOnly",
            QString("自检完成 success=%1 result=%2 msg=%3").arg(success).arg(resultToString(r), msg));
    }

    emit finished(success, r, msg, id);
}

void SH85SelfChecker::startCountdownTimer()
{
    m_elapsed.start();
    m_countdownTimer->start();
    emit countdownTick(kTotalDurationMs / 1000, m_master ? m_master->ID : QString());
}

void SH85SelfChecker::onCountdownTick()
{
    if (!isRunning()) return;
    const int rem = qMax(0, static_cast<int>((kTotalDurationMs - m_elapsed.elapsed()) / 1000));
    emit countdownTick(rem, m_master ? m_master->ID : QString());
}

void SH85SelfChecker::cleanup()
{
    if (m_phase1Timer)     m_phase1Timer->stop();
    if (m_phase2Timer)     m_phase2Timer->stop();
    if (m_pollWindowTimer) m_pollWindowTimer->stop();
    if (m_countdownTimer)  m_countdownTimer->stop();

    if (m_senderConn) {
        QObject::disconnect(m_senderConn);
        m_senderConn = QMetaObject::Connection();
    }
    if (m_senderRetryConn) {
        QObject::disconnect(m_senderRetryConn);
        m_senderRetryConn = QMetaObject::Connection();
    }
    m_pendingUuid = 0;
}
