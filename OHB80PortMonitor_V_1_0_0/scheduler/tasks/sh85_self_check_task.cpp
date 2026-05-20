#include "sh85_self_check_task.h"

#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "scheduler/tasks/alarm_dispatch_task.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "loggermanager.h"
#include "usermanager/usermanager.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"

#include <QDebug>
#include <QDateTime>

// ============================================================
// 构造 / 析构
// ============================================================

SH85SelfCheckTask::SH85SelfCheckTask(const QString &qrcode, QObject *parent)
    : SchedulerTask(parent)
    , m_qrcode(qrcode)
{
    qDebug() << "[Scheduler][SH85SelfCheckTask] 创建任务: qrcode=" << qrcode;
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] 创建任务: qrcode=%1").arg(qrcode).toStdString());
}

SH85SelfCheckTask::~SH85SelfCheckTask()
{
    // 断开 checker 信号连接
    for (const QMetaObject::Connection &c : qAsConst(m_checkerConnections))
        QObject::disconnect(c);
    m_checkerConnections.clear();

    qDebug() << "[Scheduler][SH85SelfCheckTask] 任务销毁: qrcode=" << m_qrcode;
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] 任务销毁: qrcode=%1").arg(m_qrcode).toStdString());
}

// ============================================================
// start()
// ============================================================

void SH85SelfCheckTask::start()
{
    setState(Running);
    m_stopped         = false;
    m_finishedEmitted = false;

    qDebug() << "[Scheduler][SH85SelfCheckTask] start() 被调用: qrcode=" << m_qrcode;
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] 任务启动: qrcode=%1").arg(m_qrcode).toStdString());

    if (m_qrcode.isEmpty()) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] qrcode 为空";
        // 系统日志
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SH85SelfCheckTask] qrcode 为空").toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }

    if (!ensureMaster()) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] ensureMaster 失败";
        // 系统日志
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SH85SelfCheckTask] ensureMaster 失败: qrcode=%1").arg(m_qrcode).toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }

    qDebug() << "[Scheduler][SH85SelfCheckTask] master 校验成功";
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] master 校验成功: qrcode=%1").arg(m_qrcode).toStdString());

    m_checker = m_master->selfChecker();
    if (!m_checker) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] master->selfChecker() 返回 null";
        // 系统日志
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SH85SelfCheckTask] checker 为 null: qrcode=%1").arg(m_qrcode).toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }

    qDebug() << "[Scheduler][SH85SelfCheckTask] checker 获取成功";
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] checker 获取成功: qrcode=%1").arg(m_qrcode).toStdString());

    // 连接 checker 信号
    qDebug() << "[Scheduler][SH85SelfCheckTask] 连接 checker 信号";
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
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] checker 信号连接完成: qrcode=%1").arg(m_qrcode).toStdString());

    // 启动 checker
    qDebug() << "[Scheduler][SH85SelfCheckTask] 启动 checker";
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] 启动 checker: qrcode=%1").arg(m_qrcode).toStdString());
    if (!m_checker->start()) {
        qWarning() << "[Scheduler][SH85SelfCheckTask] checker->start() 返回 false";
        // 系统日志
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SH85SelfCheckTask] checker->start() 失败: qrcode=%1").arg(m_qrcode).toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }
    qDebug() << "[Scheduler][SH85SelfCheckTask] checker 启动成功";
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] checker 启动成功: qrcode=%1").arg(m_qrcode).toStdString());

    // 写入运行日志，告知客户 SH85 自检开始
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString startDesc = QString("[QRCode: %1]StartSelfCheck command started").arg(m_qrcode);
        opTask->log(OperationDispatchTask::MsgType::Message, startDesc, 0);
    }

    // 不再需要启动定时器，自检时序由 SH85SelfChecker 内部控制
}

// ============================================================
// stop()
// ============================================================

void SH85SelfCheckTask::stop()
{
    if (m_finishedEmitted) return;
    m_stopped = true;
    qDebug() << "[Scheduler][SH85SelfCheckTask] stop() 被调用: qrcode=" << m_qrcode;
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] 任务被取消: qrcode=%1").arg(m_qrcode).toStdString());

    // 写入运行日志，告知客户 SH85 自检被取消
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString cancelDesc = QString("[QRCode: %1]StartSelfCheck command cancelled").arg(m_qrcode);
        opTask->log(OperationDispatchTask::MsgType::Message, cancelDesc, 0);
    }

    // 停止 checker
    if (m_checker) {
        m_checker->stop();
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
        // 系统日志
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SH85SelfCheckTask] 设备不可用: qrcode=%1").arg(m_qrcode).toStdString());
        return false;
    }
    qDebug() << "[Scheduler][SH85SelfCheckTask] master 可用且已连接";
    return true;
}

// ============================================================
// Checker 信号槽
// ============================================================

void SH85SelfCheckTask::onCommandCompleted(ModbusCommand cmd, const QString& masterId)
{
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
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
            ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QString();
        db->insertRecord(sentTimeStr, respTimeStr, cmd.id, masterId,
                         execStatus, retryCount,
                         cmd.request.rawBytes, cmd.response.rawBytes, description,
                         UserPermission::Engineer);
    }
}

void SH85SelfCheckTask::onCheckerStarted(const QString& masterId)
{
    qDebug() << "[Scheduler][SH85SelfCheckTask] checker started masterId=" << masterId;
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SH85SelfCheckTask] checker started: qrcode=%1 masterId=%2").arg(m_qrcode, masterId).toStdString());
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
    qDebug() << "[Scheduler][SH85SelfCheckTask] checker error result=" << SH85SelfChecker::resultToString(result)
             << "message=" << message << "masterId=" << masterId;
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][SH85SelfCheckTask] checker error: qrcode=%1 result=%2 message=%3")
            .arg(m_qrcode).arg(SH85SelfChecker::resultToString(result)).arg(message).toStdString());

    // 发出 UI 状态
    emit statusChanged(message, m_qrcode);
}

void SH85SelfCheckTask::onCheckerFinished(bool success, SH85SelfChecker::Result result, const QString& message, const QString& masterId)
{
    qDebug() << "[Scheduler][SH85SelfCheckTask] checker finished success=" << success
             << "result=" << SH85SelfChecker::resultToString(result)
             << "message=" << message << "masterId=" << masterId;
    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        success ? Level::INFO : Level::WARN,
        QString("[Scheduler][SH85SelfCheckTask] checker finished: qrcode=%1 success=%2 result=%3")
            .arg(m_qrcode).arg(success).arg(SH85SelfChecker::resultToString(result)).toStdString());

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

    emit statusChanged(uiText, m_qrcode);
    emit allFinished(success, result, m_qrcode);
    emit finished(success,
                  QString("SH85SelfCheckTask: qrcode=%1 result=%2 (%3)")
                      .arg(m_qrcode).arg(resultToText(result)).arg(uiText));

    // 系统日志
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        success ? Level::INFO : Level::WARN,
        QString("[Scheduler][SH85SelfCheckTask] 任务结束 qrcode=%1 result=%2 ui='%3'")
            .arg(m_qrcode).arg(resultToText(result)).arg(uiText).toStdString());

    writeCompletionLog(success, result);

    // 运行日志：任务完成
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString completeDesc = QString("[QRCode: %1]SH85 self-check %2: %3")
            .arg(m_qrcode)
            .arg(success ? "completed" : "failed")
            .arg(resultToText(result));
        opTask->log(success ? OperationDispatchTask::MsgType::Message : OperationDispatchTask::MsgType::Error,
                    completeDesc, 0);
    }
}

QString SH85SelfCheckTask::resultToText(SH85SelfChecker::Result r)
{
    return SH85SelfChecker::resultToString(r);
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
    const QString tag  = success ? "[Succeeded]" : "[Error]";

    QString specificDesc;
    int     alarmType = 0;

    switch (result) {
        case Result::Success:
            specificDesc = "StartSelfCheck command succeeded";
            break;
        case Result::StartCommandFailed:
            specificDesc = "StartSelfCheck command failed to send";
            alarmType = static_cast<int>(AlarmType::SH85StartSelfCheckNetworkError);
            break;
        case Result::ReadEarlyCommandFailed:
            specificDesc = "ReadSelfCheckStatus command (pre-check) failed to send";
            alarmType = static_cast<int>(AlarmType::SH85PreCheckNetworkError);
            break;
        case Result::DeviceNotEntered:
            specificDesc = "ReadSelfCheckStatus command: device not in self-check state";
            alarmType = static_cast<int>(AlarmType::SH85PreCheckNotEnterSelfCheck);
            break;
        case Result::FirmwareAbnormal:
            specificDesc = "ReadSelfCheckStatus command: firmware status abnormal";
            alarmType = static_cast<int>(AlarmType::SH85PreCheckStatusAbnormal);
            break;
        case Result::ReadPollCommandFailed:
            specificDesc = "ReadSelfCheckStatus command (polling) failed to send";
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceNetworkError);
            break;
        case Result::HumidityExceeded:
            specificDesc = "ReadSelfCheckStatus command: humidity exceeded";
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceHumidityExceeded);
            break;
        case Result::SensorCommError:
            specificDesc = "ReadSelfCheckStatus command: sensor communication error";
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceSensorCommError);
            break;
        case Result::ThresholdParamError:
            specificDesc = "ReadSelfCheckStatus command: threshold parameter error";
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceThresholdParamError);
            break;
        case Result::Timeout:
            specificDesc = "ReadSelfCheckStatus command (polling) timeout";
            alarmType = static_cast<int>(AlarmType::SH85AcceptanceTimeout);
            break;
        default:
            return;
    }

    const QString fullDesc = QString("[QRCode: %1]%2:%3").arg(m_qrcode, tag, specificDesc);

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
            QString("[QRCode: %1][Error]:85检测功能发生异常").arg(m_qrcode);

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
