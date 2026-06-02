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
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"`r`n#include "defer/defer.h"

#include <QDebug>
#include <QDateTime>

// ============================================================
// 鏋勯€?/ 鏋愭瀯
// ============================================================

SH85SelfCheckTask::SH85SelfCheckTask(const QString &qrcode, QObject *parent)
    : SchedulerTask(parent)
    , m_qrcode(qrcode)
{
    qDebug() << "[SH85SelfCheckTask] 鍒涘缓浠诲姟: qrcode=" << qrcode;
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] 鍒涘缓浠诲姟: qrcode=%1").arg(qrcode).toStdString());
}

SH85SelfCheckTask::~SH85SelfCheckTask()
{
    // 鏂紑 checker 淇″彿杩炴帴
    for (const QMetaObject::Connection &c : qAsConst(m_checkerConnections))
        QObject::disconnect(c);
    m_checkerConnections.clear();

    qDebug() << "[SH85SelfCheckTask] 浠诲姟閿€姣? qrcode=" << m_qrcode;
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] 浠诲姟閿€姣? qrcode=%1").arg(m_qrcode).toStdString());
}

// ============================================================
// start()
// ============================================================

void SH85SelfCheckTask::start()
{
    setState(Running);
    m_stopped         = false;
    m_finishedEmitted = false;

    qDebug() << "[SH85SelfCheckTask] start() 琚皟鐢? qrcode=" << m_qrcode;
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] 浠诲姟鍚姩: qrcode=%1").arg(m_qrcode).toStdString());

    if (m_qrcode.isEmpty()) {
        qWarning() << "[SH85SelfCheckTask] qrcode 涓虹┖";
        // 绯荤粺鏃ュ織
        LoggerManager::instance().log(LOG_PATH, Level::WARN,
            QString("[SH85SelfCheckTask] qrcode 涓虹┖").toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }

    if (!ensureMaster()) {
        qWarning() << "[SH85SelfCheckTask] ensureMaster 澶辫触";
        // 绯荤粺鏃ュ織
        LoggerManager::instance().log(LOG_PATH, Level::WARN,
            QString("[SH85SelfCheckTask] ensureMaster 澶辫触: qrcode=%1").arg(m_qrcode).toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }

    qDebug() << "[SH85SelfCheckTask] master 鏍￠獙鎴愬姛";
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] master 鏍￠獙鎴愬姛: qrcode=%1").arg(m_qrcode).toStdString());

    m_checker = m_master->selfChecker();
    if (!m_checker) {
        qWarning() << "[SH85SelfCheckTask] master->selfChecker() 杩斿洖 null";
        // 绯荤粺鏃ュ織
        LoggerManager::instance().log(LOG_PATH, Level::WARN,
            QString("[SH85SelfCheckTask] checker 涓?null: qrcode=%1").arg(m_qrcode).toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }

    qDebug() << "[SH85SelfCheckTask] checker 鑾峰彇鎴愬姛";
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] checker 鑾峰彇鎴愬姛: qrcode=%1").arg(m_qrcode).toStdString());

    // 杩炴帴 checker 淇″彿
    qDebug() << "[SH85SelfCheckTask] 杩炴帴 checker 淇″彿";
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
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] checker 淇″彿杩炴帴瀹屾垚: qrcode=%1").arg(m_qrcode).toStdString());

    // 鍚姩 checker
    qDebug() << "[SH85SelfCheckTask] 鍚姩 checker";
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] 鍚姩 checker: qrcode=%1").arg(m_qrcode).toStdString());
    if (!m_checker->start()) {
        qWarning() << "[SH85SelfCheckTask] checker->start() 杩斿洖 false";
        // 绯荤粺鏃ュ織
        LoggerManager::instance().log(LOG_PATH, Level::WARN,
            QString("[SH85SelfCheckTask] checker->start() 澶辫触: qrcode=%1").arg(m_qrcode).toStdString());
        finishWith(false, Result::StartCommandFailed, "Network Error");
        return;
    }
    qDebug() << "[SH85SelfCheckTask] checker 鍚姩鎴愬姛";
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] checker 鍚姩鎴愬姛: qrcode=%1").arg(m_qrcode).toStdString());

    // 鍐欏叆杩愯鏃ュ織锛屽憡鐭ュ鎴?SH85 鑷寮€濮?
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString startDesc = QString("[QRCode: %1]StartSelfCheck command started").arg(m_qrcode);
        opTask->log(OperationDispatchTask::MsgType::Message, startDesc, 0);
    }

    // 涓嶅啀闇€瑕佸惎鍔ㄥ畾鏃跺櫒锛岃嚜妫€鏃跺簭鐢?SH85SelfChecker 鍐呴儴鎺у埗
}

// ============================================================
// stop()
// ============================================================

void SH85SelfCheckTask::stop()
{
    if (m_finishedEmitted) return;
    m_stopped = true;
    qDebug() << "[SH85SelfCheckTask] stop() 琚皟鐢? qrcode=" << m_qrcode;
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask] 浠诲姟琚彇娑? qrcode=%1").arg(m_qrcode).toStdString());

    // 鍐欏叆杩愯鏃ュ織锛屽憡鐭ュ鎴?SH85 鑷琚彇娑?
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString cancelDesc = QString("[QRCode: %1]StartSelfCheck command cancelled").arg(m_qrcode);
        opTask->log(OperationDispatchTask::MsgType::Message, cancelDesc, 0);
    }

    // 鍋滄 checker
    if (m_checker) {
        m_checker->stop();
    }
}

// ============================================================
// Master 鏍￠獙
// ============================================================

bool SH85SelfCheckTask::ensureMaster()
{
    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    m_master = mgr.getMaster(m_qrcode);
    if (!m_master || !m_master->isConnected()) {
        qWarning() << "[SH85SelfCheckTask] master 涓嶅彲鐢?/ 鏈繛鎺? qrcode=" << m_qrcode;
        // 绯荤粺鏃ュ織
        LoggerManager::instance().log(LOG_PATH, Level::WARN,
            QString("[SH85SelfCheckTask] 璁惧涓嶅彲鐢? qrcode=%1").arg(m_qrcode).toStdString());
        return false;
    }
    qDebug() << "[SH85SelfCheckTask] master 鍙敤涓斿凡杩炴帴";
    return true;
}

// ============================================================
// Checker 淇″彿妲?
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
    qDebug() << "[SH85SelfCheckTask][onCheckerStarted] checker started masterId=" << masterId;
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[SH85SelfCheckTask][onCheckerStarted] checker started: qrcode=%1 masterId=%2").arg(m_qrcode, masterId).toStdString());
}

void SH85SelfCheckTask::onCheckerCountdownTick(int remainingSeconds, const QString& masterId)
{
    Q_UNUSED(masterId)
    if (m_finishedEmitted) return;

    // 杞彂鍊掕鏃朵俊鍙风粰 UI
    emit countdownTick(remainingSeconds, m_qrcode);

    // 鏈€鍚?10s 杞闃舵锛氬彂鍑?"Checking (N)" 鐘舵€佹枃鏈┍鍔ㄦ寜閽?
    const int pollWindowSec = SH85SelfChecker::kPollWindowMs / 1000;
    if (remainingSeconds <= pollWindowSec) {
        emit statusChanged(QString("Checking (%1)").arg(remainingSeconds), m_qrcode);
    }
}

void SH85SelfCheckTask::onCheckerErrorOccurred(SH85SelfChecker::Result result, const QString& message, const QString& masterId)
{
    qDebug() << "[SH85SelfCheckTask][onCheckerErrorOccurred] checker error result=" << SH85SelfChecker::resultToString(result)
             << "message=" << message << "masterId=" << masterId;
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH, Level::WARN,
        QString("[SH85SelfCheckTask][onCheckerErrorOccurred] checker error: qrcode=%1 result=%2 message=%3")
            .arg(m_qrcode).arg(SH85SelfChecker::resultToString(result)).arg(message).toStdString());

    // 鍙戝嚭 UI 鐘舵€?
    emit statusChanged(message, m_qrcode);
}

void SH85SelfCheckTask::onCheckerFinished(bool success, SH85SelfChecker::Result result, const QString& message, const QString& masterId)
{
    qDebug() << "[SH85SelfCheckTask][onCheckerFinished] checker finished success=" << success
             << "result=" << SH85SelfChecker::resultToString(result)
             << "message=" << message << "masterId=" << masterId;
    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH,
        success ? Level::INFO : Level::WARN,
        QString("[SH85SelfCheckTask][onCheckerFinished] checker finished: qrcode=%1 success=%2 result=%3")
            .arg(m_qrcode).arg(success).arg(SH85SelfChecker::resultToString(result)).toStdString());

    finishWith(success, result, message);
}

// ============================================================
// 鍐呴儴杈呭姪
// ============================================================

void SH85SelfCheckTask::finishWith(bool success, SH85SelfChecker::Result result, const QString &uiText)
{
    if (m_finishedEmitted) return;
    m_finishedEmitted = true;

    // 鏂紑 checker 淇″彿杩炴帴
    for (const QMetaObject::Connection &c : qAsConst(m_checkerConnections))
        QObject::disconnect(c);
    m_checkerConnections.clear();

    setState(success ? Finished : (result == Result::Cancelled ? Cancelled : Failed));

    emit statusChanged(uiText, m_qrcode);
    emit allFinished(success, result, m_qrcode);
    emit finished(success,
                  QString("SH85SelfCheckTask: qrcode=%1 result=%2 (%3)")
                      .arg(m_qrcode).arg(resultToText(result)).arg(uiText));

    // 绯荤粺鏃ュ織
    LoggerManager::instance().log(LOG_PATH,
        success ? Level::INFO : Level::WARN,
        QString("[SH85SelfCheckTask] 浠诲姟缁撴潫 qrcode=%1 result=%2 ui='%3'")
            .arg(m_qrcode).arg(resultToText(result)).arg(uiText).toStdString());

    writeCompletionLog(success, result);

    // 杩愯鏃ュ織锛氫换鍔″畬鎴?
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
// 鍐欏叆杩愯鏃ュ織 / 璀︽姤鏃ュ織
// ============================================================

void SH85SelfCheckTask::writeCompletionLog(bool success, SH85SelfChecker::Result result)
{
    if (result == Result::Cancelled) return;

    auto* opTask    = SharedData::getOperationDispatchTask();
    auto* alarmTask = SharedData::getAlarmDispatchTask();
    if (!opTask && !alarmTask) return;

    // DeviceNotEntered / FirmwareAbnormal 鈫?userPermission = Engineer锛屽叾浣?= Guest
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

    // ---- 鍐欒繍琛屾棩蹇?----
    if (opTask) {
        const auto msgType = success ? OperationDispatchTask::MsgType::Message
                                     : OperationDispatchTask::MsgType::Error;
        opTask->log(msgType, fullDesc, perm);
    }

    // ---- 鍐欒鎶ユ棩蹇楋紙浠呭け璐ョ粨鏋滐級----
    if (!success && alarmType != 0 && alarmTask) {
        AlarmInfo info;
        info.record.alarmType      = alarmType;
        info.alarmSource           = static_cast<int>(AlarmSource::Device);
        info.record.qrCode         = m_qrcode;
        info.record.description    = fullDesc;
        info.record.userPermission = perm;
        alarmTask->submitAlarm(info);
    }

    // ---- perm=3 缁撴灉棰濆鎻掑叆涓€鏉?perm=0 鐨勯€氱敤璁板綍锛堢粰鏅€氱敤鎴风湅锛?---
    if (isPerm3) {
        const QString genericDesc =
            QString("[QRCode: %1][Error]:85妫€娴嬪姛鑳藉彂鐢熷紓甯?).arg(m_qrcode);

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
