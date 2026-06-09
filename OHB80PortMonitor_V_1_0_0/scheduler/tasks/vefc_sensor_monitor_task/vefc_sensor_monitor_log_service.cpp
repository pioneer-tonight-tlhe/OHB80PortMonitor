#include "vefc_sensor_monitor_log_service.h"

#include "config/loggerconfig.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QStringList>

VEFCSensorMonitorLogService::VEFCSensorMonitorLogService()
    : m_logger(LoggerConfig::getInstance()->isVEFCSensorMonitorTaskSummaryEnabled(),
               LoggerConfig::getInstance()->isVEFCSensorMonitorTaskDevicesEnabled())
{
}

void VEFCSensorMonitorLogService::writeTaskConstructed()
{
    m_logger.summaryLogger().info("[VEFCSensorMonitorTask] object constructed");
}

void VEFCSensorMonitorLogService::writeTaskStarted(int intervalMs, const QString& startTime)
{
    m_logger.summaryLogger().info(QString(
        "============================= VEFCSensorMonitorTask started =============================\n"
        "intervalMs: %1\n"
        "startTime: %2\n"
        "logDir: scheduler/vefc_sensor_monitor_task")
        .arg(intervalMs)
        .arg(startTime)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeTaskStopped(qint64 runtimeMs, const QString& stopTime)
{
    m_logger.summaryLogger().info(QString(
        "============================= VEFCSensorMonitorTask stopped =============================\n"
        "runtimeMs: %1\n"
        "stopTime: %2")
        .arg(runtimeMs)
        .arg(stopTime)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeTriggerSkipped(const QString& roundId, int pendingCount)
{
    m_logger.summaryLogger().warn(QString(
        "[VEFCSensorMonitorTask] skip trigger because previous round is still active\n"
        "roundId: %1\n"
        "pendingCommands: %2")
        .arg(roundId)
        .arg(pendingCount)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeRoundStarted(const QString& roundId,
                                                    const QString& startTime,
                                                    qint64 recordTimestamp,
                                                    int totalCount)
{
    m_logger.summaryLogger().info(QString(
        "============================= VEFC monitor round started =============================\n"
        "roundId: %1\n"
        "recordTimestamp: %2\n"
        "startTime: %3\n"
        "targetCount: %4")
        .arg(roundId)
        .arg(recordTimestamp)
        .arg(startTime)
        .arg(totalCount)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeRoundCancelled(const QString& roundId, const QString& reason)
{
    m_logger.summaryLogger().error(QString(
        "[VEFCSensorMonitorTask] round cancelled\n"
        "roundId: %1\n"
        "reason: %2")
        .arg(roundId)
        .arg(reason)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeRoundFinished(const VEFCSensorMonitor::RoundSummary& summary)
{
    m_logger.summaryLogger().info(QString(
        "============================= VEFC monitor round finished =============================\n"
        "roundId: %1\n"
        "startTime: %2\n"
        "endTime: %3\n"
        "totalCount: %4\n"
        "persistedCount: %5\n"
        "failedCount: %6\n"
        "skippedCount: %7\n"
        "failedDevices: %8\n"
        "skippedDevices: %9")
        .arg(summary.roundId)
        .arg(summary.startTime)
        .arg(summary.endTime)
        .arg(summary.totalCount)
        .arg(summary.persistedCount)
        .arg(summary.failedCount)
        .arg(summary.skippedCount)
        .arg(summary.failedDevices.isEmpty() ? QStringLiteral("none") : summary.failedDevices.join(QStringLiteral(", ")))
        .arg(summary.skippedDevices.isEmpty() ? QStringLiteral("none") : summary.skippedDevices.join(QStringLiteral(", ")))
        .toStdString());
}

void VEFCSensorMonitorLogService::writeDeviceSkipped(const QString& roundId,
                                                     const QString& qrCode,
                                                     const QString& reason,
                                                     const QString& recordTime)
{
    m_logger.deviceLogger(qrCode).info(QString(
        "[VEFCSensorMonitorTask][QRCode:%1] device skipped\n"
        "roundId: %2\n"
        "recordTime: %3\n"
        "reason: %4")
        .arg(qrCode)
        .arg(roundId)
        .arg(recordTime)
        .arg(reason)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeCommandRetrying(const QString& roundId,
                                                       const QString& qrCode,
                                                       const ModbusCommand& cmd)
{
    m_logger.deviceLogger(qrCode).info(QString(
        "[VEFCSensorMonitorTask][QRCode:%1] command retrying\n"
        "roundId: %2\n"
        "command: %3\n"
        "attempt: %4/%5")
        .arg(qrCode)
        .arg(roundId)
        .arg(cmd.id)
        .arg(cmd.sendCount)
        .arg(cmd.maxRetryCount + 1)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeCommandSucceeded(const QString& roundId,
                                                        const QString& qrCode,
                                                        const ModbusCommand& cmd)
{
    m_logger.deviceLogger(qrCode).info(QString(
        "[VEFCSensorMonitorTask][QRCode:%1] command finished\n"
        "roundId: %2\n"
        "command: %3\n"
        "requestFrame: %4\n"
        "responseFrame: %5\n"
        "result: success")
        .arg(qrCode)
        .arg(roundId)
        .arg(cmd.id)
        .arg(commandRequestFrame(cmd))
        .arg(commandResponseFrame(cmd))
        .toStdString());
}

void VEFCSensorMonitorLogService::writeCommandFailed(const QString& roundId,
                                                     const QString& qrCode,
                                                     const ModbusCommand& cmd,
                                                     const QString& reason)
{
    m_logger.deviceLogger(qrCode).warn(QString(
        "[VEFCSensorMonitorTask][QRCode:%1] command finished\n"
        "roundId: %2\n"
        "command: %3\n"
        "requestFrame: %4\n"
        "responseFrame: %5\n"
        "result: failed\n"
        "reason: %6")
        .arg(qrCode)
        .arg(roundId)
        .arg(cmd.id)
        .arg(commandRequestFrame(cmd))
        .arg(commandResponseFrame(cmd))
        .arg(reason)
        .toStdString());
}

void VEFCSensorMonitorLogService::writePersistFailed(const QString& roundId,
                                                     const VEFCSensorMonitor::DeviceRoundState& state,
                                                     const QString& reason)
{
    m_logger.deviceLogger(state.qrCode).error(QString(
        "[VEFCSensorMonitorTask][QRCode:%1] persist failed\n"
        "roundId: %2\n"
        "recordTime: %3\n"
        "reason: %4")
        .arg(state.qrCode)
        .arg(roundId)
        .arg(state.record.recordTimeString())
        .arg(reason)
        .toStdString());
}

void VEFCSensorMonitorLogService::writeRecordPersisted(const QString& roundId,
                                                       const VEFCSensorMonitor::DeviceRoundState& state)
{
    m_logger.deviceLogger(state.qrCode).info(QString(
        "[VEFCSensorMonitorTask][QRCode:%1] record persisted\n"
        "roundId: %2\n"
        "recordTime: %3\n"
        "gasPressure: %4\n"
        "actualFlow: %5\n"
        "sensorPressure: %6\n"
        "sensorTemperature: %7")
        .arg(state.qrCode)
        .arg(roundId)
        .arg(state.record.recordTimeString())
        .arg(state.record.gasPressure)
        .arg(state.record.actualFlow)
        .arg(state.record.sensorPressure)
        .arg(state.record.sensorTemperature)
        .toStdString());
}

QString VEFCSensorMonitorLogService::commandFailureReason(const ModbusCommand& cmd)
{
    QStringList reasons;
    if (cmd.timedOut) {
        reasons << QStringLiteral("timed out");
    }
    if (cmd.checksumError) {
        reasons << QStringLiteral("checksum error");
    }
    if (cmd.deviceBusy) {
        reasons << QStringLiteral("device busy");
    }
    if (!cmd.errorMessage.trimmed().isEmpty()) {
        reasons << cmd.errorMessage.trimmed();
    }
    return reasons.isEmpty() ? QStringLiteral("no response") : reasons.join(QStringLiteral(", "));
}

QString VEFCSensorMonitorLogService::commandRequestFrame(const ModbusCommand& cmd)
{
    QByteArray frame = cmd.request.rawBytes;
    frame.append(cmd.request.crc);
    return frame.isEmpty() ? QStringLiteral("none") : QString(frame.toHex(' ').toUpper());
}

QString VEFCSensorMonitorLogService::commandResponseFrame(const ModbusCommand& cmd)
{
    QByteArray frame = cmd.response.rawBytes;
    frame.append(cmd.response.crc);
    const QString frameText = frame.isEmpty() ? QStringLiteral("none") : QString(frame.toHex(' ').toUpper());
    if (cmd.received) {
        return frameText;
    }
    const QString reason = commandFailureReason(cmd);
    return frame.isEmpty() ? reason : QStringLiteral("%1, %2").arg(reason, frameText);
}
