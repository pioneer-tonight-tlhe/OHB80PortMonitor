#include "sh85_self_check_log_service.h"

#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "logdatabases/databasemanager.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "app/shareddata.h"
#include "loggermanager.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QStringList>
#include <QVariantMap>
#include <QtGlobal>

namespace {
constexpr const char* kSelfCheckLogPath = "scheduler/sh85_self_check/self_check";
constexpr const char* kTaskName = "SH85PeriodicSelfCheckTask3";

QString valueOrDash(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("-") : value;
}

QString listOrNone(const QStringList& values)
{
    return values.isEmpty() ? QStringLiteral("无") : values.join(QStringLiteral(", "));
}

QString modeText(bool singleDeviceMode, const QString& qrcode)
{
    return singleDeviceMode
        ? QStringLiteral("单设备(%1)").arg(valueOrDash(qrcode))
        : QStringLiteral("全量设备");
}

QString resultText(bool success, SH85SelfChecker::Result result)
{
    if (result == SH85SelfChecker::Result::Cancelled) {
        return QStringLiteral("取消");
    }
    return success ? QStringLiteral("成功") : QStringLiteral("失败");
}

void writeFileLog(Level level, const QString& message)
{
    LoggerManager::getInstance()->log(kSelfCheckLogPath, level, "{}", message.toStdString());
    LoggerManager::getInstance()->flush(kSelfCheckLogPath);
}

void writeOperationLog(OperationDispatchTask::MsgType type, const QString& message)
{
    if (OperationDispatchTask* opTask = SharedData::getOperationDispatchTask()) {
        opTask->log(type, message, 0);
    }
}
} // namespace

void SH85SelfCheckLogService::writeTaskConstructed() const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][construct] 周期自检任务已创建").arg(kTaskName));
}

void SH85SelfCheckLogService::writeTaskStarted(int periodSec,
                                               bool singleDeviceMode,
                                               const QString& singleDeviceQrcode) const
{
    const QString message =
        QStringLiteral("[%1][start] 任务启动，周期=%2s，模式=%3")
            .arg(kTaskName)
            .arg(periodSec)
            .arg(modeText(singleDeviceMode, singleDeviceQrcode));
    writeFileLog(Level::INFO, message);
    writeOperationLog(OperationDispatchTask::MsgType::Message, message);
}

void SH85SelfCheckLogService::writeTaskStartIgnored(const QString& reason) const
{
    writeFileLog(Level::WARN,
                 QStringLiteral("[%1][start] 启动被忽略，原因=%2")
                     .arg(kTaskName, valueOrDash(reason)));
}

void SH85SelfCheckLogService::writeTaskStopping(int pendingCount) const
{
    const QString message =
        QStringLiteral("[%1][stop] 任务停止中，待收口设备数=%2")
            .arg(kTaskName)
            .arg(pendingCount);
    writeFileLog(Level::INFO, message);
    writeOperationLog(OperationDispatchTask::MsgType::Message, message);
}

void SH85SelfCheckLogService::writeTaskStopped() const
{
    const QString message = QStringLiteral("[%1][stop] 任务已停止").arg(kTaskName);
    writeFileLog(Level::INFO, message);
    writeOperationLog(OperationDispatchTask::MsgType::Message, message);
}

void SH85SelfCheckLogService::writeEnabledChanged(bool enabled) const
{
    const QString message =
        QStringLiteral("[%1][setEnabled] 周期自检%2")
            .arg(kTaskName, enabled ? QStringLiteral("启用") : QStringLiteral("停用"));
    writeFileLog(Level::INFO, message);
    writeOperationLog(OperationDispatchTask::MsgType::Message, message);
}

void SH85SelfCheckLogService::writePeriodChanged(int value,
                                                 const QString& unit,
                                                 int totalSec) const
{
    const QString message =
        QStringLiteral("[%1][setPeriod] 周期参数更新，输入=%2%3，折算=%4s")
            .arg(kTaskName)
            .arg(value)
            .arg(unit)
            .arg(totalSec);
    writeFileLog(Level::INFO, message);
    writeOperationLog(OperationDispatchTask::MsgType::Message, message);
}

void SH85SelfCheckLogService::writeSingleDeviceChanged(bool singleDeviceMode,
                                                       const QString& qrcode) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][setSingleDevice] 模式=%2")
                     .arg(kTaskName, modeText(singleDeviceMode, qrcode)));
}

void SH85SelfCheckLogService::writeBootDelayScheduled(int delaySeconds) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][bootDelay] 首轮自检将在 %2s 后触发")
                     .arg(kTaskName)
                     .arg(delaySeconds));
}

void SH85SelfCheckLogService::writeBootDelaySkipped(const QString& reason) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][bootDelay] 首轮触发跳过，原因=%2")
                     .arg(kTaskName, valueOrDash(reason)));
}

void SH85SelfCheckLogService::writeTriggerSkipped(const QString& roundId,
                                                  int pendingCount) const
{
    writeFileLog(Level::WARN,
                 QStringLiteral("[%1][trigger] 上一轮仍未结束，本次触发跳过，roundId=%2，pending=%3")
                     .arg(kTaskName, valueOrDash(roundId))
                     .arg(pendingCount));
}

void SH85SelfCheckLogService::writeTaskStateChanged(const QString& stateText) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][state] 状态变更为 %2")
                     .arg(kTaskName, valueOrDash(stateText)));
}

void SH85SelfCheckLogService::writeNextRoundScheduled(int periodSec) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][schedule] 下一轮将在 %2s 后触发")
                     .arg(kTaskName)
                     .arg(periodSec));
}

void SH85SelfCheckLogService::writeRoundStarted(const QString& roundId,
                                                const QString& startTime,
                                                int totalCount,
                                                const QStringList& orderedQrcodes) const
{
    const QString message =
        QStringLiteral("[%1][roundStart] 轮次开始，roundId=%2，startTime=%3，总设备数=%4，目标设备=%5")
            .arg(kTaskName, valueOrDash(roundId), valueOrDash(startTime))
            .arg(totalCount)
            .arg(listOrNone(orderedQrcodes));
    writeFileLog(Level::INFO, message);
    writeOperationLog(OperationDispatchTask::MsgType::Message,
                      QStringLiteral("SH85 周期自检轮次开始：设备数=%1，roundId=%2")
                          .arg(totalCount)
                          .arg(valueOrDash(roundId)));
}

void SH85SelfCheckLogService::writeRoundFinished(const QString& roundId,
                                                 const QString& startTime,
                                                 const QString& endTime,
                                                 const SH85SelfCheck::RoundSummary& summary,
                                                 const QStringList& failedDevices,
                                                 const QStringList& skippedDevices) const
{
    const QString result = summary.failureCount == 0 ? QStringLiteral("成功") : QStringLiteral("失败");
    const QString message =
        QStringLiteral("[%1][roundFinish] 轮次结束，roundId=%2，startTime=%3，endTime=%4，总数=%5，参与=%6，成功=%7，失败=%8，跳过=%9，结果=%10，失败设备=%11，跳过设备=%12")
            .arg(kTaskName, valueOrDash(roundId), valueOrDash(startTime), valueOrDash(endTime))
            .arg(summary.totalCount)
            .arg(summary.participatedCount)
            .arg(summary.successCount)
            .arg(summary.failureCount)
            .arg(summary.skippedCount)
            .arg(result, listOrNone(failedDevices), listOrNone(skippedDevices));

    const bool hasFailure = summary.failureCount > 0;
    writeFileLog(hasFailure ? Level::WARN : Level::INFO, message);
    writeOperationLog(hasFailure ? OperationDispatchTask::MsgType::Error
                                 : OperationDispatchTask::MsgType::Message,
                      QStringLiteral("SH85 周期自检轮次结束：成功=%1，失败=%2，跳过=%3，roundId=%4")
                          .arg(summary.successCount)
                          .arg(summary.failureCount)
                          .arg(summary.skippedCount)
                          .arg(valueOrDash(roundId)));
}

void SH85SelfCheckLogService::writeDeviceSkipped(const QString& roundId,
                                                 const QString& qrcode,
                                                 const QString& reason) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][deviceSkip] roundId=%2，qrcode=%3，原因=%4")
                     .arg(kTaskName, valueOrDash(roundId), valueOrDash(qrcode), valueOrDash(reason)));
}

void SH85SelfCheckLogService::writeDeviceStarted(const QString& roundId,
                                                 const QString& qrcode) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][deviceStart] roundId=%2，qrcode=%3，checker 已启动")
                     .arg(kTaskName, valueOrDash(roundId), valueOrDash(qrcode)));
}

void SH85SelfCheckLogService::writeDeviceStartFailed(const QString& roundId,
                                                     const QString& qrcode,
                                                     SH85SelfChecker::Result result,
                                                     const QString& reason) const
{
    writeFileLog(Level::WARN,
                 QStringLiteral("[%1][deviceStartFailed] roundId=%2，qrcode=%3，result=%4，原因=%5")
                     .arg(kTaskName,
                          valueOrDash(roundId),
                          valueOrDash(qrcode),
                          SH85SelfChecker::resultToString(result),
                          valueOrDash(reason)));
}

void SH85SelfCheckLogService::writeDeviceFinished(const QString& roundId,
                                                  const QString& qrcode,
                                                  bool success,
                                                  SH85SelfChecker::Result result,
                                                  const QString& description) const
{
    const Level level = (!success && result != SH85SelfChecker::Result::Cancelled)
        ? Level::WARN
        : Level::INFO;
    writeFileLog(level,
                 QStringLiteral("[%1][deviceFinish] roundId=%2，qrcode=%3，结果=%4，result=%5，说明=%6")
                     .arg(kTaskName,
                          valueOrDash(roundId),
                          valueOrDash(qrcode),
                          resultText(success, result),
                          SH85SelfChecker::resultToString(result),
                          valueOrDash(description)));
}

void SH85SelfCheckLogService::writeCheckerStateChanged(const QString& roundId,
                                                       const QString& qrcode,
                                                       SH85SelfChecker::State state) const
{
    writeFileLog(Level::INFO,
                 QStringLiteral("[%1][checkerState] roundId=%2，qrcode=%3，state=%4")
                     .arg(kTaskName,
                          valueOrDash(roundId),
                          valueOrDash(qrcode),
                          SH85SelfChecker::stateToString(state)));
}

void SH85SelfCheckLogService::writeCheckerCommandRetrying(const QString& roundId,
                                                          const QString& qrcode,
                                                          const ModbusCommand& cmd) const
{
    writeFileLog(Level::WARN,
                 QStringLiteral("[%1][commandRetrying] roundId=%2，qrcode=%3，commandId=%4，sendCount=%5，error=%6")
                     .arg(kTaskName,
                          valueOrDash(roundId),
                          valueOrDash(qrcode),
                          valueOrDash(cmd.id),
                          QString::number(cmd.sendCount),
                          valueOrDash(cmd.errorMessage)));
}

void SH85SelfCheckLogService::writeCheckerError(const QString& roundId,
                                                const QString& qrcode,
                                                SH85SelfChecker::Result result,
                                                const QString& message) const
{
    writeFileLog(Level::ERROR,
                 QStringLiteral("[%1][checkerError] roundId=%2，qrcode=%3，result=%4，message=%5")
                     .arg(kTaskName,
                          valueOrDash(roundId),
                          valueOrDash(qrcode),
                          SH85SelfChecker::resultToString(result),
                          valueOrDash(message)));
}

void SH85SelfCheckLogService::writeCommunicateLog(const ModbusCommand& cmd,
                                                  const QString& masterId) const
{
    const QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("-");

    // 保持 Task2 写 communicate_log 时使用的执行状态语义。
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
        // 成功响应解析为 key=value 描述，便于操作人员查看通讯明细。
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

        db->insertRecord(sentTimeStr,
                         respTimeStr,
                         cmd.id,
                         masterId,
                         execStatus,
                         retryCount,
                         cmd.request.rawBytes,
                         cmd.response.rawBytes,
                         description,
                         UserPermission::Engineer);
    } else {
        writeFileLog(Level::WARN,
                     QStringLiteral("[%1][communicateLog] 通讯日志数据库不可用，commandId=%2，qrcode=%3")
                         .arg(kTaskName, valueOrDash(cmd.id), valueOrDash(masterId)));
    }
}
