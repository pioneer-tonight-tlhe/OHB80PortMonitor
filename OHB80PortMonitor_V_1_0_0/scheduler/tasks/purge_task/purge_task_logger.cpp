#include "purge_task_logger.h"

#include "modbustcpmastermanager/modbuscommand/modbuscrc.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QStringList>

namespace {
QString elapsedText(qint64 elapsedMs)
{
    return elapsedMs >= 0 ? QString::number(elapsedMs) : QStringLiteral("unknown");
}
}

PurgeTaskLogger::PurgeTaskLogger()
    : m_summaryLogger("scheduler/purge_task/summary")
    , m_commandLogger("scheduler/purge_task/commands")
{
}

void PurgeTaskLogger::logTaskStarting(const QString &taskId,
                                      const PurgeTaskDefinition &definition)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][TaskStarting]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Configured total duration: %3 s\n"
                             "Stage count: %4")
                             .arg(taskId,
                                  definition.qrCode,
                                  QString::number(definition.totalDurationSeconds),
                                  QString::number(definition.stages.size()));
    m_summaryLogger.info(text.toStdString());
}

void PurgeTaskLogger::logTaskStarted(const QString &taskId,
                                     const PurgeTaskDefinition &definition,
                                     const QString &outputDir)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][TaskStarted]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Output directory: %3")
                             .arg(taskId, definition.qrCode, outputDir);
    m_summaryLogger.info(text.toStdString());
}

void PurgeTaskLogger::logTaskFinished(const QString &taskId,
                                      const QString &qrCode,
                                      bool success,
                                      const QString &message,
                                      qint64 elapsedMs)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][TaskFinished]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Result: %3\n"
                             "Elapsed: %4 ms\n"
                             "Message: %5")
                             .arg(taskId,
                                  qrCode,
                                  success ? QStringLiteral("success") : QStringLiteral("failure"),
                                  elapsedText(elapsedMs),
                                  message);
    if (success) {
        m_summaryLogger.info(text.toStdString());
    } else {
        m_summaryLogger.warn(text.toStdString());
    }
}

void PurgeTaskLogger::logStagePreparing(const QString &taskId,
                                        const QString &qrCode,
                                        int stageNo,
                                        const PurgeStageDefinition &stage)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][StagePreparing]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Stage: %3\n"
                             "Name: %4\n"
                             "Duration: %5 s\n"
                             "Action count: %6")
                             .arg(taskId,
                                  qrCode,
                                  QString::number(stageNo),
                                  stage.name,
                                  QString::number(stage.durationSeconds),
                                  QString::number(stage.actions.size()));
    m_summaryLogger.info(text.toStdString());
}

void PurgeTaskLogger::logStageStarted(const QString &taskId,
                                      const QString &qrCode,
                                      int stageNo,
                                      const PurgeStageDefinition &stage)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][StageStarted]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Stage: %3\n"
                             "Name: %4\n"
                             "Duration: %5 s")
                             .arg(taskId,
                                  qrCode,
                                  QString::number(stageNo),
                                  stage.name,
                                  QString::number(stage.durationSeconds));
    m_summaryLogger.info(text.toStdString());
}

void PurgeTaskLogger::logStageFinished(const QString &taskId,
                                       const QString &qrCode,
                                       int stageNo,
                                       const PurgeStageDefinition &stage,
                                       qint64 elapsedMs)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][StageFinished]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Stage: %3\n"
                             "Name: %4\n"
                             "Elapsed: %5 ms")
                             .arg(taskId,
                                  qrCode,
                                  QString::number(stageNo),
                                  stage.name,
                                  elapsedText(elapsedMs));
    m_summaryLogger.info(text.toStdString());
}

void PurgeTaskLogger::logCommandSubmitting(const QString &taskId,
                                           const QString &qrCode,
                                           int stageNo,
                                           int actionNo,
                                           const PurgeActionDefinition &action,
                                           const ModbusCommand &cmd)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][CommandSubmitting]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Stage: %3\n"
                             "Action: %4\n"
                             "ActionId: %5\n"
                             "Command: %6\n"
                             "Command UUID: %7\n"
                             "Parameters: %8\n"
                             "Request frame: %9")
                             .arg(taskId,
                                  qrCode,
                                  QString::number(stageNo),
                                  QString::number(actionNo),
                                  action.actionId,
                                  action.commandId,
                                  QString::number(cmd.uuid),
                                  actionParams(action),
                                  requestFrame(cmd));
    m_commandLogger.info(text.toStdString());
}

void PurgeTaskLogger::logCommandSubmitFailed(const QString &taskId,
                                             const QString &qrCode,
                                             int stageNo,
                                             int actionNo,
                                             const PurgeActionDefinition &action,
                                             const QString &reason)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][CommandSubmitFailed]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Stage: %3\n"
                             "Action: %4\n"
                             "ActionId: %5\n"
                             "Command: %6\n"
                             "Parameters: %7\n"
                             "Failure reason: %8")
                             .arg(taskId,
                                  qrCode,
                                  QString::number(stageNo),
                                  QString::number(actionNo),
                                  action.actionId,
                                  action.commandId,
                                  actionParams(action),
                                  reason);
    m_commandLogger.warn(text.toStdString());
}

void PurgeTaskLogger::logCommandRetry(const QString &taskId,
                                      const QString &qrCode,
                                      int stageNo,
                                      int actionNo,
                                      const ModbusCommand &cmd)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][CommandRetry]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Stage: %3\n"
                             "Action: %4\n"
                             "Command: %5\n"
                             "Command UUID: %6\n"
                             "Send count: %7/%8\n"
                             "Request frame: %9\n"
                             "Retry reason: %10")
                             .arg(taskId)
                             .arg(qrCode)
                             .arg(stageNo)
                             .arg(actionNo)
                             .arg(cmd.id)
                             .arg(cmd.uuid)
                             .arg(cmd.sendCount)
                             .arg(cmd.maxRetryCount + 1)
                             .arg(requestFrame(cmd))
                             .arg(commandFailureReason(cmd, QStringLiteral("retry requested")));
    m_commandLogger.warn(text.toStdString());
}

void PurgeTaskLogger::logCommandFinished(const QString &taskId,
                                         const QString &qrCode,
                                         int stageNo,
                                         int actionNo,
                                         const PurgeActionDefinition &action,
                                         const ModbusCommand &cmd,
                                         bool success,
                                         const QString &message)
{
    const qint64 elapsedMs = cmd.sentMs > 0 && cmd.responseMs >= cmd.sentMs
        ? cmd.responseMs - cmd.sentMs
        : -1;
    QString resultDetails;
    if (success) {
        resultDetails = QStringLiteral("Response frame: %1").arg(responseFrame(cmd));
    } else {
        const QString rawResponse = responseFrame(cmd);
        if (rawResponse != QStringLiteral("none")) {
            resultDetails = QStringLiteral("Response frame: %1\n").arg(rawResponse);
        }
        resultDetails += QStringLiteral("Failure reason: %1")
                             .arg(commandFailureReason(cmd, message));
    }

    const QString text = QStringLiteral(
                             "[PurgeTask][CommandFinished]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Stage: %3\n"
                             "Action: %4\n"
                             "ActionId: %5\n"
                             "Command: %6\n"
                             "Command UUID: %7\n"
                             "Result: %8\n"
                             "Send count: %9\n"
                             "Sent at: %10\n"
                             "Response at: %11\n"
                             "Elapsed: %12 ms\n"
                             "Request frame: %13\n"
                             "%14")
                             .arg(taskId)
                             .arg(qrCode)
                             .arg(stageNo)
                             .arg(actionNo)
                             .arg(action.actionId)
                             .arg(action.commandId)
                             .arg(cmd.uuid)
                             .arg(success ? QStringLiteral("success") : QStringLiteral("failure"))
                             .arg(cmd.sendCount)
                             .arg(dateTimeText(cmd.sentMs))
                             .arg(dateTimeText(cmd.responseMs))
                             .arg(elapsedText(elapsedMs))
                             .arg(requestFrame(cmd))
                             .arg(resultDetails);
    if (success) {
        m_commandLogger.info(text.toStdString());
    } else {
        m_commandLogger.warn(text.toStdString());
    }
}

void PurgeTaskLogger::logError(const QString &taskId,
                               const QString &qrCode,
                               const QString &operation,
                               const QString &message)
{
    const QString text = QStringLiteral(
                             "[PurgeTask][Error]\n"
                             "TaskId: %1\n"
                             "QRCode: %2\n"
                             "Operation: %3\n"
                             "Message: %4")
                             .arg(taskId, qrCode, operation, message);
    m_summaryLogger.error(text.toStdString());
}

void PurgeTaskLogger::flush()
{
    m_summaryLogger.flush();
    m_commandLogger.flush();
}

QString PurgeTaskLogger::requestFrame(const ModbusCommand &cmd)
{
    if (cmd.request.rawBytes.isEmpty()) {
        return QStringLiteral("none");
    }

    QByteArray frame = cmd.request.rawBytes;
    frame.append(cmd.request.crc.isEmpty()
                     ? ModbusCrc::modbusCRC16(cmd.request.rawBytes)
                     : cmd.request.crc);
    return QString(frame.toHex(' ').toUpper());
}

QString PurgeTaskLogger::responseFrame(const ModbusCommand &cmd)
{
    if (!cmd.received) {
        return QStringLiteral("none");
    }

    QByteArray frame = cmd.response.rawBytes;
    frame.append(cmd.response.crc);
    return frame.isEmpty() ? QStringLiteral("none") : QString(frame.toHex(' ').toUpper());
}

QString PurgeTaskLogger::commandFailureReason(const ModbusCommand &cmd,
                                              const QString &fallbackMessage)
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
    if (!cmd.received && !cmd.timedOut && !cmd.deviceBusy) {
        reasons << QStringLiteral("no response");
    }
    if (!cmd.errorMessage.trimmed().isEmpty()) {
        reasons << cmd.errorMessage.trimmed();
    }
    if (reasons.isEmpty() && !fallbackMessage.trimmed().isEmpty()) {
        reasons << fallbackMessage.trimmed();
    }
    return reasons.isEmpty() ? QStringLiteral("command failed")
                             : reasons.join(QStringLiteral(", "));
}

QString PurgeTaskLogger::actionParams(const PurgeActionDefinition &action)
{
    return QString::fromUtf8(QJsonDocument(action.params).toJson(QJsonDocument::Compact));
}

QString PurgeTaskLogger::dateTimeText(qint64 timestampMs)
{
    return timestampMs > 0
        ? QDateTime::fromMSecsSinceEpoch(timestampMs)
              .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
        : QStringLiteral("unknown");
}
