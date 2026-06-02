#include "sh85_self_check_task.h"
#include "sh85_self_check_log_helper.h"

#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "app/shareddata.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "usermanager/usermanager.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"

#include <QDebug>
#include <QDateTime>

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
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

SH85SelfCheckTask::SH85SelfCheckTask(const QString &qrcode, QObject *parent)
    : SchedulerTask(parent)
    , m_qrcode(qrcode)
{
    qDebug() << "[Scheduler][SH85SelfCheckTask] 创建任务: qrcode=" << qrcode;
}

SH85SelfCheckTask::~SH85SelfCheckTask()
{
    // 断开 checker 信号连接
    for (const QMetaObject::Connection &c : qAsConst(m_checkerConnections))
        QObject::disconnect(c);
    m_checkerConnections.clear();

    qDebug() << "[Scheduler][SH85SelfCheckTask] 任务销毁: qrcode=" << m_qrcode;
}

// ============================================================
// start()
// ============================================================

void SH85SelfCheckTask::start()
{
    setState(Running);
    m_stopped         = false;
    m_finishedEmitted = false;
    m_currentCheckerState = SH85SelfChecker::State::Idle;
    SH85SelfCheckLogHelper::initRecord(m_record,
                                       SH85SelfCheckLogHelper::Mode::Manual,
                                       m_qrcode);
    SH85SelfCheckLogHelper::appendStart(m_record);

    qDebug() << "[Scheduler][SH85SelfCheckTask] start() 被调用: qrcode=" << m_qrcode;

    if (m_qrcode.isEmpty()) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] qrcode 为空";
        SH85SelfCheckLogHelper::appendCommandNotSubmitted(
            m_record,
            SH85SelfChecker::State::StartingSelfCheck,
            startSelfCheckTemplateCommand(),
            QStringLiteral("qrcode 为空"));
        m_record.appendRecord(QStringLiteral("启动失败\n原因: qrcode 为空"));
        finishWith(false, Result::StartCommandFailed, QStringLiteral("qrcode 为空"));
        return;
    }

    if (!ensureMaster()) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] ensureMaster 失败";
        SH85SelfCheckLogHelper::appendCommandNotSubmitted(
            m_record,
            SH85SelfChecker::State::StartingSelfCheck,
            startSelfCheckTemplateCommand(),
            QStringLiteral("设备不可用或未连接"));
        m_record.appendRecord(QStringLiteral("启动失败\n原因: 设备不可用或未连接"));
        finishWith(false, Result::StartCommandFailed, QStringLiteral("设备不可用或未连接"));
        return;
    }

    m_checker = m_master->selfChecker();
    if (!m_checker) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] master->selfChecker() 返回 null";
        SH85SelfCheckLogHelper::appendCommandNotSubmitted(
            m_record,
            SH85SelfChecker::State::StartingSelfCheck,
            startSelfCheckTemplateCommand(),
            QStringLiteral("SelfChecker 为空"));
        m_record.appendRecord(QStringLiteral("启动失败\n原因: SelfChecker 为空"));
        finishWith(false, Result::StartCommandFailed, QStringLiteral("SelfChecker 为空"));
        return;
    }

    // 连接 checker 信号
    m_checkerConnections.append(
        connect(m_checker, &SH85SelfChecker::started,
                this, &SH85SelfCheckTask::onCheckerStarted, Qt::QueuedConnection));
    m_checkerConnections.append(
        connect(m_checker, &SH85SelfChecker::countdownTick,
                this, &SH85SelfCheckTask::onCheckerCountdownTick, Qt::QueuedConnection));
    m_checkerConnections.append(
        connect(m_checker, &SH85SelfChecker::errorOccurred,
                this, &SH85SelfCheckTask::onCheckerErrorOccurred, Qt::QueuedConnection));
    m_checkerConnections.append(
        connect(m_checker, &SH85SelfChecker::finished,
                this, &SH85SelfCheckTask::onCheckerFinished, Qt::QueuedConnection));
    m_checkerConnections.append(
        connect(m_checker, &SH85SelfChecker::commandCompleted,
                this, &SH85SelfCheckTask::onCommandCompleted, Qt::QueuedConnection));
    m_checkerConnections.append(
        connect(m_checker, &SH85SelfChecker::commandRetrying,
                this, &SH85SelfCheckTask::onCommandRetrying, Qt::QueuedConnection));
    m_checkerConnections.append(
        connect(m_checker, &SH85SelfChecker::stateChanged,
                this, &SH85SelfCheckTask::onCheckerStateChanged, Qt::QueuedConnection));

    // 启动 checker
    if (!m_checker->start()) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] checker->start() 返回 false";
        SH85SelfCheckLogHelper::appendCommandNotSubmitted(
            m_record,
            SH85SelfChecker::State::StartingSelfCheck,
            startSelfCheckTemplateCommand(),
            QStringLiteral("自检器启动失败"));
        m_record.appendRecord(QStringLiteral("启动失败\n原因: checker 启动失败"));
        finishWith(false, Result::StartCommandFailed, QStringLiteral("自检器启动失败"));
        return;
    }

    m_record.appendRecord(QStringLiteral("自检器启动成功"));

    // 写入运行日志，告知客户 SH85 自检开始
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString startDesc = QString("[QRCode:%1]: SH85 自检开始").arg(m_qrcode);
        opTask->log(OperationDispatchTask::MsgType::Message, startDesc, 0);
    }
}

// ============================================================
// stop()
// ============================================================

void SH85SelfCheckTask::stop()
{
    if (m_finishedEmitted) return;
    m_stopped = true;
    qDebug() << "[Scheduler][SH85SelfCheckTask] stop() 被调用: qrcode=" << m_qrcode;
    m_record.appendRecord(QStringLiteral("用户取消自检任务"));

    // 写入运行日志，告知客户 SH85 自检被取消
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString cancelDesc = QString("[QRCode:%1]: SH85 自检取消").arg(m_qrcode);
        opTask->log(OperationDispatchTask::MsgType::Message, cancelDesc, 0);
    }

    // 停止 checker
    if (m_checker) {
        m_checker->stop();
    } else {
        finishWith(false, Result::Cancelled, QStringLiteral("用户取消"));
    }
}

// ============================================================
// Master 校验
// ============================================================

bool SH85SelfCheckTask::ensureMaster()
{
    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    m_master = mgr.getMaster(m_qrcode);
    if (!m_master || !m_master->isConnected()) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] master 不可用 / 未连接: qrcode=" << m_qrcode;
        return false;
    }
    return true;
}

// ============================================================
// Checker 信号槽
// ============================================================

void SH85SelfCheckTask::onCommandCompleted(ModbusCommand cmd, const QString& masterId)
{
    SH85SelfCheckLogHelper::appendCommand(m_record, m_currentCheckerState, cmd);

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

void SH85SelfCheckTask::onCommandRetrying(ModbusCommand cmd, const QString& masterId)
{
    Q_UNUSED(masterId)
    SH85SelfCheckLogHelper::appendCommand(m_record, m_currentCheckerState, cmd, true);
}

void SH85SelfCheckTask::onCheckerStarted(const QString& masterId)
{
    Q_UNUSED(masterId)
    qDebug() << "[Scheduler][SH85SelfCheckTask] checker started: qrcode=" << m_qrcode;
}

void SH85SelfCheckTask::onCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId)
{
    Q_UNUSED(masterId)
    if (m_finishedEmitted) return;
    if (state == SH85SelfChecker::State::Done) return;

    m_currentCheckerState = state;
    SH85SelfCheckLogHelper::appendStage(m_record, state);
}

void SH85SelfCheckTask::onCheckerCountdownTick(int remainingSeconds, const QString& masterId)
{
    Q_UNUSED(masterId)
    if (m_finishedEmitted) return;

    // 转发倒计时信号给 UI
    emit countdownTick(remainingSeconds, m_qrcode);

    // 最后 10s 轮询阶段：发出 "Checking (N)" 状态文本驱动按钮
    const int pollWindowSec = SH85SelfChecker::kPollWindowMs / 1000;
    if (remainingSeconds <= pollWindowSec) {
        emit statusChanged(QString("Checking (%1)").arg(remainingSeconds), m_qrcode);
    }
}

void SH85SelfCheckTask::onCheckerErrorOccurred(SH85SelfChecker::Result result, const QString& message, const QString& masterId)
{
    Q_UNUSED(masterId)
    qDebug() << "[Scheduler][SH85SelfCheckTask] checker error result=" << SH85SelfChecker::resultToString(result)
             << "message=" << message;
    const QString normalizedMessage = SH85SelfCheckLogHelper::descriptionToChinese(result, message);
    m_record.appendRecord(QStringLiteral("自检异常\n结果: %1\n说明: %2")
                              .arg(resultToChineseText(result), normalizedMessage));

    // 发出 UI 状态
    emit statusChanged(normalizedMessage, m_qrcode);
}

void SH85SelfCheckTask::onCheckerFinished(bool success, SH85SelfChecker::Result result, const QString& message, const QString& masterId)
{
    Q_UNUSED(masterId)
    qDebug() << "[Scheduler][SH85SelfCheckTask] checker finished success=" << success
             << "result=" << SH85SelfChecker::resultToString(result)
             << "message=" << message;

    finishWith(success, result, message);
}

// ============================================================
// 内部辅助
// ============================================================

void SH85SelfCheckTask::finishWith(bool success, SH85SelfChecker::Result result, const QString &uiText)
{
    if (m_finishedEmitted) return;
    m_finishedEmitted = true;

    // 断开 checker 信号连接
    for (const QMetaObject::Connection &c : qAsConst(m_checkerConnections))
        QObject::disconnect(c);
    m_checkerConnections.clear();

    setState(success ? Finished : (result == Result::Cancelled ? Cancelled : Failed));

    const QString finalText = SH85SelfCheckLogHelper::descriptionToChinese(
        result,
        uiText.isEmpty() ? resultToChineseText(result) : uiText);

    emit statusChanged(finalText, m_qrcode);
    emit allFinished(success, result, m_qrcode);
    emit finished(success,
                  QString("SH85SelfCheckTask: qrcode=%1 result=%2 (%3)")
                      .arg(m_qrcode).arg(resultToChineseText(result)).arg(finalText));

    SH85SelfCheckLogHelper::finishRecord(m_record, success, result, finalText);
    SH85SelfCheckLogHelper::writeRecord(m_record, !success && result != Result::Cancelled);
    SH85SelfCheckLogHelper::writeTaskSeparator(SH85SelfCheckLogHelper::Mode::Manual,
                                               QString(),
                                               m_qrcode,
                                               SH85SelfCheckLogHelper::resultText(success, result),
                                               QString(),
                                               QString());

    writeCompletionLog(success, result);

    // 运行日志：任务完成
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString completeDesc = QString("[QRCode:%1]: SH85 自检%2: %3")
            .arg(m_qrcode)
            .arg(success ? QStringLiteral("完成") : QStringLiteral("失败"))
            .arg(resultToChineseText(result));
        opTask->log(OperationDispatchTask::MsgType::Message, completeDesc, 0);
    }
}

QString SH85SelfCheckTask::resultToText(SH85SelfChecker::Result r)
{
    return SH85SelfChecker::resultToString(r);
}

QString SH85SelfCheckTask::resultToChineseText(SH85SelfChecker::Result r)
{
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

QString SH85SelfCheckTask::stateToChineseText(SH85SelfChecker::State s)
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

// ============================================================
// 写入运行日志 / 警报日志
// ============================================================

void SH85SelfCheckTask::writeCompletionLog(bool success, SH85SelfChecker::Result result)
{
    if (result == Result::Cancelled) return;

    auto* opTask    = SharedData::getOperationDispatchTask();
    auto* alarmTask = SharedData::getAlarmDispatchTask();
    if (!opTask && !alarmTask) return;

    // DeviceNotEntered / FirmwareAbnormal → userPermission = Engineer，其余 = Guest
    const bool isPerm3 = (result == Result::DeviceNotEntered ||
                          result == Result::FirmwareAbnormal);
    const int  perm    = isPerm3 ? UserPermission::Engineer : UserPermission::Guest;
    const QString tag  = success ? QStringLiteral("[成功]") : QStringLiteral("[错误]");

    QString specificDesc;
    int     alarmType = 0;

    switch (result) {
        case Result::Success:
            specificDesc = QStringLiteral("StartSelfCheck 指令执行成功");
            break;
        case Result::StartCommandFailed:
            specificDesc = QStringLiteral("StartSelfCheck 指令下发失败");
            alarmType = static_cast<int>(AlarmType::SH85StartSelfCheckNetworkError);
            break;
        case Result::ReadEarlyCommandFailed:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 预检指令下发失败");
            alarmType = static_cast<int>(AlarmType::SH85PreCheckNetworkError);
            break;
        case Result::DeviceNotEntered:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 指令返回设备未进入自检状态");
            alarmType = static_cast<int>(AlarmType::SH85PreCheckNotEnterSelfCheck);
            break;
        case Result::FirmwareAbnormal:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 指令返回固件状态异常");
            alarmType = static_cast<int>(AlarmType::SH85PreCheckStatusAbnormal);
            break;
        case Result::ReadPollCommandFailed:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 轮询指令下发失败");
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceNetworkError);
            break;
        case Result::HumidityExceeded:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 指令返回湿度超标");
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceHumidityExceeded);
            break;
        case Result::SensorCommError:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 指令返回传感器通讯故障");
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceSensorCommError);
            break;
        case Result::ThresholdParamError:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 指令返回阈值参数错误");
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceThresholdParamError);
            break;
        case Result::Timeout:
            specificDesc = QStringLiteral("ReadSelfCheckStatus 轮询超时");
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceTimeout);
            break;
        default:
            return;
    }

    const QString fullDesc = QString("[QRCode:%1]: %2%3").arg(m_qrcode, tag, specificDesc);

    // ---- 写运行日志 ----
    if (opTask) {
        const auto msgType = success ? OperationDispatchTask::MsgType::Message
                                     : OperationDispatchTask::MsgType::Error;
        opTask->log(msgType, fullDesc, perm);
    }

    // ---- 写警报日志（仅失败结果）----
    if (!success && alarmType != 0 && alarmTask) {
        AlarmInfo info;
        info.record.alarmType      = alarmType;
        info.alarmSource           = static_cast<int>(AlarmSource::Device);
        info.record.qrCode         = m_qrcode;
        info.record.description    = fullDesc;
        info.record.userPermission = perm;
        alarmTask->submitAlarm(info);
    }

    // ---- perm=3 结果额外插入一条 perm=0 的通用记录（给普通用户看）----
    if (isPerm3) {
        const QString genericDesc =
            QString("[QRCode:%1]: [错误]SH85 自检功能发生异常").arg(m_qrcode);

        if (opTask) {
            opTask->log(OperationDispatchTask::MsgType::Error, genericDesc, 0);
        }
        if (alarmTask) {
            AlarmInfo generic;
            generic.record.alarmType      = static_cast<int>(AlarmType::SH85SelfCheckActionFailed);
            generic.alarmSource           = static_cast<int>(AlarmSource::Device);
            generic.record.qrCode         = m_qrcode;
            generic.record.description    = genericDesc;
            generic.record.userPermission = 0;
            alarmTask->submitAlarm(generic);
        }
    }
}
