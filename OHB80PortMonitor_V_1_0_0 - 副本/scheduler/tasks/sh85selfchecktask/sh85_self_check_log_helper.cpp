#include "sh85_self_check_log_helper.h"

#include <QLatin1Char>
#include <QMap>

namespace {
constexpr const char* kLogPath = "scheduler/sh85_self_check/self_check";

QString valueOrDash(const QString& value)
{
    return value.isEmpty() ? QStringLiteral("-") : value;
}

QString timestamp()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString timestampFromMs(qint64 ms)
{
    return ms > 0
        ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("-");
}
} // namespace

ILogger& SH85SelfCheckLogHelper::logger()
{
    static ILogger logger(kLogPath);
    return logger;
}

QString SH85SelfCheckLogHelper::createRoundId()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
}

QString SH85SelfCheckLogHelper::modeToText(Mode mode)
{
    return mode == Mode::Manual ? QStringLiteral("手动自检") : QStringLiteral("周期自检");
}

QString SH85SelfCheckLogHelper::stateToChineseText(SH85SelfChecker::State state)
{
    switch (state) {
    case SH85SelfChecker::State::Idle:                 return QStringLiteral("空闲");
    case SH85SelfChecker::State::StartingSelfCheck:    return QStringLiteral("阶段1 - 下发启动自检指令");
    case SH85SelfChecker::State::WaitingPhase1:        return QStringLiteral("阶段1 - 等待5秒");
    case SH85SelfChecker::State::ReadingStatusEarly:   return QStringLiteral("阶段2 - 预检读取自检状态");
    case SH85SelfChecker::State::WaitingPhase2:        return QStringLiteral("阶段2 - 等待55秒");
    case SH85SelfChecker::State::PollingStatus:        return QStringLiteral("阶段3 - 轮询读取自检结果（10秒窗口）");
    case SH85SelfChecker::State::Done:                 return QStringLiteral("完成");
    }
    return QStringLiteral("未知状态");
}

QString SH85SelfCheckLogHelper::resultToCode(SH85SelfChecker::Result result)
{
    return SH85SelfChecker::resultToString(result);
}

QString SH85SelfCheckLogHelper::resultToChineseText(SH85SelfChecker::Result result)
{
    using Result = SH85SelfChecker::Result;
    switch (result) {
    case Result::Success:                return QStringLiteral("自检成功");
    case Result::StartCommandFailed:     return QStringLiteral("启动自检指令下发失败");
    case Result::ReadEarlyCommandFailed: return QStringLiteral("预检读取指令下发失败");
    case Result::DeviceNotEntered:       return QStringLiteral("设备未进入自检状态");
    case Result::FirmwareAbnormal:       return QStringLiteral("底层固件状态异常");
    case Result::ReadPollCommandFailed:  return QStringLiteral("轮询读取指令下发失败");
    case Result::HumidityExceeded:       return QStringLiteral("湿度超标");
    case Result::SensorCommError:        return QStringLiteral("SH85传感器通讯故障");
    case Result::ThresholdParamError:    return QStringLiteral("阈值参数错误（湿度下限阈值≤0）");
    case Result::Timeout:                return QStringLiteral("轮询窗口超时，未获取到终态值");
    case Result::Cancelled:              return QStringLiteral("用户取消");
    }
    return QStringLiteral("未知结果");
}

QString SH85SelfCheckLogHelper::resultText(bool success, SH85SelfChecker::Result result)
{
    if (result == SH85SelfChecker::Result::Cancelled) {
        return QStringLiteral("取消");
    }
    return success ? QStringLiteral("成功") : QStringLiteral("失败");
}

QString SH85SelfCheckLogHelper::descriptionToChinese(SH85SelfChecker::Result result,
                                                     const QString& description)
{
    return normalizedDescription(result, description);
}

void SH85SelfCheckLogHelper::initRecord(SH85SelfCheckTaskRecord& record,
                                        Mode mode,
                                        const QString& qrCode,
                                        const QString& roundId)
{
    record.reset();
    record.setModeText(modeToText(mode));
    record.setRoundId(roundId);
    record.setQrCode(qrCode);
    record.markTaskStart();
}

void SH85SelfCheckLogHelper::appendStart(SH85SelfCheckTaskRecord& record)
{
    record.appendRecord(QStringLiteral("自检任务开始\n总时长: 70 s"));
}

void SH85SelfCheckLogHelper::appendSkip(SH85SelfCheckTaskRecord& record, const QString& reason)
{
    record.appendRecord(QStringLiteral("跳过自检\n原因: %1").arg(reason));
}

void SH85SelfCheckLogHelper::appendStage(SH85SelfCheckTaskRecord& record,
                                         SH85SelfChecker::State state)
{
    if (state == SH85SelfChecker::State::Done) {
        return;
    }
    record.appendRecord(QStringLiteral("进入阶段: %1").arg(stateToChineseText(state)));
}

void SH85SelfCheckLogHelper::appendCommand(SH85SelfCheckTaskRecord& record,
                                           SH85SelfChecker::State state,
                                           const ModbusCommand& cmd,
                                           bool retrying)
{
    const bool success = cmd.received
                      && !cmd.timedOut
                      && !cmd.checksumError
                      && !cmd.deviceBusy;

    QStringList lines;
    lines << QStringLiteral("指令下发明细: %1").arg(retrying ? QStringLiteral("超时重试") : QStringLiteral("完成"));
    lines << QStringLiteral("指令: %1").arg(valueOrDash(cmd.id));
    lines << QStringLiteral("所属阶段: %1").arg(stateToChineseText(state));
    lines << QStringLiteral("发送时间: %1").arg(timestampFromMs(cmd.sentMs));
    lines << QStringLiteral("响应时间: %1").arg(timestampFromMs(cmd.responseMs));
    lines << QStringLiteral("发送次数: %1/%2").arg(cmd.sendCount).arg(cmd.maxRetryCount + 1);
    lines << QStringLiteral("发送指令帧: %1").arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc));
    lines << QStringLiteral("响应帧: %1").arg(responseFrameString(cmd));

    const QString parsedValue = parsedValueText(cmd);
    if (!parsedValue.isEmpty()) {
        lines << QStringLiteral("解析值: %1").arg(parsedValue);
    }

    if (!cmd.errorMessage.isEmpty()) {
        lines << QStringLiteral("错误信息: %1").arg(cmd.errorMessage);
    }

    if (retrying) {
        lines << QStringLiteral("处理结果: 超时，准备重试");
    } else {
        lines << QStringLiteral("处理结果: %1").arg(success ? QStringLiteral("成功") : QStringLiteral("失败"));
    }
    record.appendRecord(lines.join(QStringLiteral("\n")));
}

void SH85SelfCheckLogHelper::appendCommandNotSubmitted(SH85SelfCheckTaskRecord& record,
                                                       SH85SelfChecker::State state,
                                                       const ModbusCommand& cmd,
                                                       const QString& reason)
{
    const QString commandId = cmd.id.isEmpty() ? QStringLiteral("StartSelfCheck") : cmd.id;
    const QString frameText = cmd.request.rawBytes.isEmpty()
        ? QStringLiteral("无")
        : bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc);
    const QString failureReason = reason.isEmpty() ? QStringLiteral("未知原因") : reason;

    QStringList lines;
    lines << QStringLiteral("指令下发明细: 未下发");
    lines << QStringLiteral("指令: %1").arg(commandId);
    lines << QStringLiteral("所属阶段: %1").arg(stateToChineseText(state));
    lines << QStringLiteral("发送时间: -");
    lines << QStringLiteral("响应时间: -");
    lines << QStringLiteral("发送次数: 0/%1").arg(cmd.maxRetryCount + 1);
    lines << QStringLiteral("请求帧: %1").arg(frameText);
    lines << QStringLiteral("响应帧: 未下发, %1").arg(failureReason);
    lines << QStringLiteral("处理结果: 未下发");
    record.appendRecord(lines.join(QStringLiteral("\n")));
}

void SH85SelfCheckLogHelper::finishRecord(SH85SelfCheckTaskRecord& record,
                                          bool success,
                                          SH85SelfChecker::Result result,
                                          const QString& description)
{
    record.setResultText(resultText(success, result));
    record.setResultCode(resultToCode(result));
    record.setDescription(normalizedDescription(result, description));
    record.markTaskEnd();
}

void SH85SelfCheckLogHelper::finishSkippedRecord(SH85SelfCheckTaskRecord& record,
                                                 const QString& reason)
{
    record.setResultText(QStringLiteral("跳过"));
    record.setResultCode(QStringLiteral("Skipped"));
    record.setDescription(reason);
    record.markTaskEnd();
}

void SH85SelfCheckLogHelper::writeRecord(const SH85SelfCheckTaskRecord& record, bool warn)
{
    const std::string message = record.toLogString().toStdString();
    if (warn) {
        logger().warn(message);
    } else {
        logger().info(message);
    }
}

void SH85SelfCheckLogHelper::writeRoundReport(const RoundSummary& summary,
                                              const QList<SH85SelfCheckTaskRecord>& records)
{
    QStringList lines;
    lines << QStringLiteral("################################################################################");
    lines << QStringLiteral("############################# SH85_PERIODIC_ROUND_BEGIN #############################");
    lines << QStringLiteral("任务名称: SH85PeriodicSelfCheckTask");
    lines << QStringLiteral("模式: 周期自检");
    lines << QStringLiteral("轮次ID: %1").arg(valueOrDash(summary.roundId));
    lines << QStringLiteral("开始时间: %1").arg(valueOrDash(summary.startTime));
    lines << QStringLiteral("结束时间: %1").arg(valueOrDash(summary.endTime));
    lines << QStringLiteral("总设备数: %1").arg(summary.totalDevices);
    lines << QStringLiteral("参与设备数: %1").arg(summary.participatedCount);
    lines << QStringLiteral("成功设备数: %1").arg(summary.successCount);
    lines << QStringLiteral("失败设备数: %1").arg(summary.failureCount);
    lines << QStringLiteral("跳过设备数: %1").arg(summary.skippedCount);
    lines << QStringLiteral("结果: %1").arg(summary.failureCount == 0 ? QStringLiteral("成功") : QStringLiteral("失败"));
    lines << QStringLiteral("失败设备: %1").arg(summary.failedDevices.isEmpty() ? QStringLiteral("无") : summary.failedDevices.join(QStringLiteral(", ")));
    lines << QStringLiteral("跳过设备: %1").arg(summary.skippedDevices.isEmpty() ? QStringLiteral("无") : summary.skippedDevices.join(QStringLiteral(", ")));
    lines << QStringLiteral("----------------------------- 设备执行记录列表 -----------------------------");

    for (int i = 0; i < records.size(); ++i) {
        const SH85SelfCheckTaskRecord& record = records.at(i);
        lines << QStringLiteral("[设备记录 %1/%2 BEGIN] QRCode=%3 轮次ID=%4")
                     .arg(i + 1)
                     .arg(records.size())
                     .arg(valueOrDash(record.qrCode()))
                     .arg(valueOrDash(summary.roundId));
        lines << record.toLogString();
        lines << QStringLiteral("[设备记录 %1/%2 END] QRCode=%3 轮次ID=%4")
                     .arg(i + 1)
                     .arg(records.size())
                     .arg(valueOrDash(record.qrCode()))
                     .arg(valueOrDash(summary.roundId));
    }

    lines << QStringLiteral("############################## SH85_PERIODIC_ROUND_END ##############################");
    lines << QStringLiteral("轮次ID: %1").arg(valueOrDash(summary.roundId));
    lines << QStringLiteral("################################################################################");

    const std::string message = lines.join(QStringLiteral("\n")).toStdString();
    logger().info(message);
}

void SH85SelfCheckLogHelper::writeRoundWarning(const RoundSummary& summary,
                                               const QList<SH85SelfCheckTaskRecord>& records)
{
    if (summary.failureCount <= 0) {
        return;
    }

    QMap<QString, QStringList> failedReasonMap;
    for (const SH85SelfCheckTaskRecord& record : records) {
        if (record.resultText() != QStringLiteral("失败")) {
            continue;
        }

        QString reason = record.description();
        if (reason.isEmpty()) {
            reason = QStringLiteral("未知失败原因");
        }
        failedReasonMap[reason].append(record.qrCode());
    }

    QStringList lines;
    lines << QStringLiteral("[SH85PeriodicSelfCheckTask] 周期自检轮次失败");
    lines << QStringLiteral("轮次ID: %1").arg(valueOrDash(summary.roundId));
    lines << QStringLiteral("开始时间: %1").arg(valueOrDash(summary.startTime));
    lines << QStringLiteral("结束时间: %1").arg(valueOrDash(summary.endTime));
    lines << QStringLiteral("总设备数: %1").arg(summary.totalDevices);
    lines << QStringLiteral("参与设备数: %1").arg(summary.participatedCount);
    lines << QStringLiteral("成功设备数: %1").arg(summary.successCount);
    lines << QStringLiteral("失败设备数: %1").arg(summary.failureCount);
    lines << QStringLiteral("跳过设备数: %1").arg(summary.skippedCount);
    lines << QStringLiteral("失败原因摘要:");

    if (failedReasonMap.isEmpty()) {
        lines << QStringLiteral("- 未记录具体失败原因");
    } else {
        for (auto it = failedReasonMap.constBegin(); it != failedReasonMap.constEnd(); ++it) {
            lines << QStringLiteral("- %1: %2 台").arg(it.key()).arg(it.value().size());
            lines << QStringLiteral("  设备: %1").arg(it.value().join(QStringLiteral(", ")));
        }
    }

    lines << QStringLiteral("详细执行记录: 请在 self_check.log 搜索轮次ID %1").arg(valueOrDash(summary.roundId));
    logger().warn(lines.join(QStringLiteral("\n")).toStdString());
}

void SH85SelfCheckLogHelper::writeRoundSummary(const RoundSummary& summary)
{
    QStringList lines;
    lines << QStringLiteral("[SH85PeriodicSelfCheckTask] 一轮自检汇总");
    lines << QStringLiteral("轮次ID: %1").arg(valueOrDash(summary.roundId));
    lines << QStringLiteral("开始时间: %1").arg(valueOrDash(summary.startTime));
    lines << QStringLiteral("结束时间: %1").arg(valueOrDash(summary.endTime));
    lines << QStringLiteral("总设备数: %1").arg(summary.totalDevices);
    lines << QStringLiteral("参与设备数: %1").arg(summary.participatedCount);
    lines << QStringLiteral("成功设备数: %1").arg(summary.successCount);
    lines << QStringLiteral("失败设备数: %1").arg(summary.failureCount);
    lines << QStringLiteral("跳过设备数: %1").arg(summary.skippedCount);
    lines << QStringLiteral("结果: %1").arg(summary.failureCount == 0 ? QStringLiteral("成功") : QStringLiteral("失败"));
    lines << QStringLiteral("失败设备: %1").arg(summary.failedDevices.isEmpty() ? QStringLiteral("无") : summary.failedDevices.join(QStringLiteral(", ")));
    lines << QStringLiteral("跳过设备: %1").arg(summary.skippedDevices.isEmpty() ? QStringLiteral("无") : summary.skippedDevices.join(QStringLiteral(", ")));
    logger().info(lines.join(QStringLiteral("\n")).toStdString());
}

void SH85SelfCheckLogHelper::writeTaskSeparator(Mode mode,
                                                const QString& roundId,
                                                const QString& qrCode,
                                                const QString& resultText,
                                                const QString& startTime,
                                                const QString& endTime,
                                                int totalDevices,
                                                int participatedCount,
                                                int successCount,
                                                int failureCount,
                                                int skippedCount)
{
    QStringList lines;
    if (mode == Mode::Periodic) {
        lines << QStringLiteral("============================= SH85 周期自检轮次结束 =============================");
        lines << QStringLiteral("模式: %1").arg(modeToText(mode));
        lines << QStringLiteral("轮次ID: %1").arg(valueOrDash(roundId));
        lines << QStringLiteral("开始时间: %1").arg(valueOrDash(startTime));
        lines << QStringLiteral("结束时间: %1").arg(valueOrDash(endTime));
        lines << QStringLiteral("总设备数: %1").arg(totalDevices);
        lines << QStringLiteral("参与设备数: %1").arg(participatedCount);
        lines << QStringLiteral("成功设备数: %1").arg(successCount);
        lines << QStringLiteral("失败设备数: %1").arg(failureCount);
        lines << QStringLiteral("跳过设备数: %1").arg(skippedCount);
        lines << QStringLiteral("结果: %1").arg(valueOrDash(resultText));
    } else {
        lines << QStringLiteral("============================= SH85SelfCheckTask 任务结束 =============================");
        lines << QStringLiteral("模式: %1").arg(modeToText(mode));
        lines << QStringLiteral("设备: %1").arg(valueOrDash(qrCode));
        lines << QStringLiteral("结果: %1").arg(valueOrDash(resultText));
        lines << QStringLiteral("时间: %1").arg(timestamp());
    }
    lines << QStringLiteral("================================================================================");
    logger().info(lines.join(QStringLiteral("\n")).toStdString());
}

QString SH85SelfCheckLogHelper::commandDescription(const ModbusCommand& cmd)
{
    const QString parsedValue = parsedValueText(cmd);
    if (!parsedValue.isEmpty()) {
        return parsedValue;
    }
    if (!cmd.errorMessage.isEmpty()) {
        return cmd.errorMessage;
    }
    return QStringLiteral("OK");
}

QString SH85SelfCheckLogHelper::normalizedDescription(SH85SelfChecker::Result result,
                                                      const QString& description)
{
    QString text = description.trimmed();
    if (text.isEmpty()) {
        return resultToChineseText(result);
    }

    const QString resultCode = resultToCode(result);
    const QString resultCodePrefix = QStringLiteral("%1:").arg(resultCode);
    while (!resultCode.isEmpty() && text.startsWith(resultCodePrefix)) {
        text = text.mid(resultCodePrefix.size()).trimmed();
    }

    if (text == resultCode) {
        return resultToChineseText(result);
    }
    if (text == QStringLiteral("OK")) {
        return QStringLiteral("成功");
    }

    text.replace(QStringLiteral("Submit StartSelfCheck failed"),
                 QStringLiteral("提交启动自检指令失败"));
    text.replace(QStringLiteral("StartSelfCheck failed"),
                 QStringLiteral("启动自检指令下发失败"));
    text.replace(QStringLiteral("Submit phase1 ReadSelfCheckStatus failed"),
                 QStringLiteral("提交阶段1读取自检状态指令失败"));
    text.replace(QStringLiteral("Phase1 ReadSelfCheckStatus failed"),
                 QStringLiteral("阶段1读取自检状态指令失败"));
    text.replace(QStringLiteral("Submit phase2 ReadSelfCheckStatus failed"),
                 QStringLiteral("提交阶段2读取自检状态指令失败"));
    text.replace(QStringLiteral("Phase2 ReadSelfCheckStatus failed"),
                 QStringLiteral("阶段2读取自检状态指令失败"));
    text.replace(QStringLiteral("Submit ReadSelfCheckStatus failed"),
                 QStringLiteral("提交读取自检状态指令失败"));
    text.replace(QStringLiteral("ReadSelfCheckStatus failed"),
                 QStringLiteral("读取自检状态指令失败"));
    text.replace(QStringLiteral("Device not connected"),
                 QStringLiteral("设备未连接"));
    text.replace(QStringLiteral("Device did not enter self-check"),
                 QStringLiteral("设备未进入自检状态"));
    text.replace(QStringLiteral("Firmware abnormal in phase1"),
                 QStringLiteral("阶段1固件状态异常"));
    text.replace(QStringLiteral("Firmware abnormal during polling"),
                 QStringLiteral("轮询阶段固件状态异常"));
    text.replace(QStringLiteral("unknown CH_1"),
                 QStringLiteral("未知 CH_1"));
    text.replace(QStringLiteral("Self-check function timeout (no response in 10s window)"),
                 QStringLiteral("自检功能超时（10秒窗口内无响应）"));
    text.replace(QStringLiteral("Self-check function timeout"),
                 QStringLiteral("自检功能超时"));
    text.replace(QStringLiteral("Self-check OK"),
                 QStringLiteral("自检成功"));
    text.replace(QStringLiteral("Humidity exceeded threshold"),
                 QStringLiteral("湿度超标"));
    text.replace(QStringLiteral("SH85 sensor comm error"),
                 QStringLiteral("SH85传感器通讯故障"));
    text.replace(QStringLiteral("Threshold parameter error"),
                 QStringLiteral("阈值参数错误"));
    text.replace(QStringLiteral("FOUP in place"),
                 QStringLiteral("FOUP 到位"));
    text.replace(QStringLiteral("Self-checker is null"),
                 QStringLiteral("SelfChecker 为空"));
    text.replace(QStringLiteral("Checker start failed"),
                 QStringLiteral("自检器启动失败"));
    text.replace(QStringLiteral("Network Error"),
                 QStringLiteral("网络异常"));
    text.replace(QStringLiteral("Cancelled by user"),
                 QStringLiteral("用户取消"));

    return text.isEmpty() ? resultToChineseText(result) : text;
}

QString SH85SelfCheckLogHelper::bytesToHexWithCrc(const QByteArray& bytes, const QByteArray& crc)
{
    QStringList hexList;
    for (int i = 0; i < bytes.size(); ++i) {
        hexList << QString(QStringLiteral("%1"))
                       .arg(static_cast<quint8>(bytes[i]), 2, 16, QLatin1Char('0'))
                       .toUpper();
    }
    if (crc.size() >= 2) {
        hexList << QString(QStringLiteral("%1"))
                       .arg(static_cast<quint8>(crc[0]), 2, 16, QLatin1Char('0'))
                       .toUpper();
        hexList << QString(QStringLiteral("%1"))
                       .arg(static_cast<quint8>(crc[1]), 2, 16, QLatin1Char('0'))
                       .toUpper();
    }
    return hexList.isEmpty() ? QStringLiteral("无") : hexList.join(QStringLiteral(" "));
}

QString SH85SelfCheckLogHelper::responseFrameString(const ModbusCommand& cmd)
{
    if (cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy) {
        return bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
    }

    QStringList failureReasons;
    if (cmd.timedOut) failureReasons << QStringLiteral("超时");
    if (cmd.checksumError) failureReasons << QStringLiteral("校验错误");
    if (cmd.deviceBusy) failureReasons << QStringLiteral("设备忙");
    if (!cmd.errorMessage.isEmpty()) failureReasons << cmd.errorMessage;

    const QString failureText = failureReasons.isEmpty()
        ? QStringLiteral("失败")
        : failureReasons.join(QStringLiteral(", "));
    if (!cmd.received) {
        return failureText;
    }

    const QString frameText = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
    return frameText == QStringLiteral("无")
        ? failureText
        : QStringLiteral("%1, %2").arg(failureText, frameText);
}

QString SH85SelfCheckLogHelper::parsedValueText(const ModbusCommand& cmd)
{
    if (cmd.id != QStringLiteral("ReadSelfCheckStatus")) {
        return QString();
    }
    const QByteArray& value = cmd.response.registerValue;
    if (value.size() < 2) {
        return QString();
    }
    const quint16 ch1 = (static_cast<quint16>(static_cast<quint8>(value[0])) << 8)
                      |  static_cast<quint16>(static_cast<quint8>(value[1]));
    return QStringLiteral("CH_1=%1").arg(ch1);
}
