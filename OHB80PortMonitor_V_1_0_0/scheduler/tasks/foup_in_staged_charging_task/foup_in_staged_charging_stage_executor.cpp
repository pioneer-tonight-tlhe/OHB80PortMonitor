/*******************************************************************************************
 * @file foup_in_staged_charging_stage_executor.cpp
 * @author Simon <工号：13> 2026-07-18
 *
 * @brief 实现单个充气阶段的行为串行执行和 Modbus 指令处理。
 *******************************************************************************************/
#include "foup_in_staged_charging_stage_executor.h"

#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QMetaObject>
#include <QStringList>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {

bool toUInt16(const QJsonValue &value, quint16 *result, QString *errorMessage)
{
    if (!result || !value.isDouble()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("value must be an integer");
        }
        return false;
    }

    const double number = value.toDouble();
    if (!std::isfinite(number)
        || number < 0.0
        || number > static_cast<double>(std::numeric_limits<quint16>::max())
        || std::floor(number) != number) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("value must be an integer between 0 and 65535");
        }
        return false;
    }

    *result = static_cast<quint16>(number);
    return true;
}

bool toScaledUInt16(const QJsonObject &parameters,
                    const QString &valueKey,
                    quint16 *result,
                    QString *errorMessage)
{
    const QJsonValue value = parameters.value(valueKey);
    if (!value.isDouble()) {
        if (errorMessage) {
            *errorMessage = QString("parameter '%1' must be numeric").arg(valueKey);
        }
        return false;
    }

    const double scale = parameters.value(QStringLiteral("scale")).isDouble()
        ? parameters.value(QStringLiteral("scale")).toDouble()
        : 1.0;
    const double scaledValue = value.toDouble() * scale;
    if (!std::isfinite(scaledValue)
        || scaledValue < 0.0
        || scaledValue > static_cast<double>(std::numeric_limits<quint16>::max())
        || std::floor(scaledValue) != scaledValue) {
        if (errorMessage) {
            *errorMessage = QString("parameter '%1' is outside the valid scaled range")
                                .arg(valueKey);
        }
        return false;
    }

    *result = static_cast<quint16>(scaledValue);
    return true;
}

} // namespace

FoupInStagedChargingStageExecutor::FoupInStagedChargingStageExecutor(
    const QString &taskId,
    const QString &masterId,
    int stageIndex,
    const FoupInStagedChargingStageConfig &stage,
    ModbusCommandSender *sender,
    QObject *parent)
    : QObject(parent)
    , m_taskId(taskId)
    , m_masterId(masterId)
    , m_stageIndex(stageIndex)
    , m_stage(stage)
    , m_sender(sender)
    , m_logger("scheduler/foup_in_staged_charging_task/stage_executor")
{
}

FoupInStagedChargingStageExecutor::~FoupInStagedChargingStageExecutor()
{
    disconnectSender();
}

void FoupInStagedChargingStageExecutor::start()
{
    if (m_started || m_stopped || m_finishedEmitted) {
        return;
    }

    m_started = true;
    m_behaviorIndex = 0;
    m_pendingCommandUuid = 0;

    if (!m_sender) {
        finishStage(false, QStringLiteral("command sender is null"));
        return;
    }
    if (m_stage.behaviors.isEmpty()) {
        finishStage(false,
                    QString("stage '%1' has no behaviors").arg(m_stage.stageName));
        return;
    }

    m_senderConnections.append(
        connect(m_sender,
                &ModbusCommandSender::commandFinished,
                this,
                &FoupInStagedChargingStageExecutor::onCommandFinished,
                Qt::QueuedConnection));
    m_senderConnections.append(
        connect(m_sender,
                &ModbusCommandSender::commandTimeoutRetry,
                this,
                &FoupInStagedChargingStageExecutor::onCommandTimeoutRetry,
                Qt::QueuedConnection));

    logInfo(QStringLiteral("stage executor started: taskId=%1 masterId=%2 stage=%3[%4] behaviors=%5")
                .arg(m_taskId)
                .arg(m_masterId)
                .arg(m_stage.stageName)
                .arg(m_stageIndex)
                .arg(m_stage.behaviors.size()));
    sendCurrentBehavior();
}

void FoupInStagedChargingStageExecutor::stop()
{
    if (m_finishedEmitted) {
        return;
    }

    m_stopped = true;
    m_pendingCommandUuid = 0;
    disconnectSender();
}

void FoupInStagedChargingStageExecutor::sendCurrentBehavior()
{
    if (m_stopped || m_finishedEmitted) {
        return;
    }

    if (m_behaviorIndex < 0 || m_behaviorIndex >= m_stage.behaviors.size()) {
        finishStage(false, QStringLiteral("behavior index out of range"));
        return;
    }

    const FoupInStagedChargingBehaviorConfig &behavior =
        m_stage.behaviors.at(m_behaviorIndex);
    emit behaviorStarted(m_taskId,
                         m_stage.stageName,
                         m_stageIndex,
                         behavior.behaviorName,
                         m_behaviorIndex);

    QString errorMessage;
    const ModbusCommand command = buildCommand(behavior, &errorMessage);
    if (!command.isValid()) {
        emit behaviorFinished(m_taskId,
                              m_stage.stageName,
                              m_stageIndex,
                              behavior.behaviorName,
                              m_behaviorIndex,
                              false,
                              errorMessage);
        finishStage(false, errorMessage);
        return;
    }

    m_pendingCommandUuid = command.uuid;
    logInfo(QStringLiteral("behavior command submitting: stage=%1[%2] behavior=%3[%4] command=%5 uuid=%6")
                .arg(m_stage.stageName)
                .arg(m_stageIndex)
                .arg(behavior.behaviorName)
                .arg(m_behaviorIndex)
                .arg(command.id)
                .arg(command.uuid));

    ModbusCommandSender *sender = m_sender;
    QMetaObject::invokeMethod(sender,
                              [sender, command]() { sender->submit(command); },
                              Qt::QueuedConnection);
}

void FoupInStagedChargingStageExecutor::onCommandFinished(
    ModbusCommand command,
    const QString &masterId)
{
    if (m_stopped
        || m_finishedEmitted
        || masterId != m_masterId
        || command.uuid != m_pendingCommandUuid) {
        return;
    }

    m_pendingCommandUuid = 0;
    const FoupInStagedChargingBehaviorConfig &behavior =
        m_stage.behaviors.at(m_behaviorIndex);
    const bool success = commandSucceeded(command);
    const QString message = success
        ? QStringLiteral("OK")
        : commandFailureMessage(command);

    emit behaviorFinished(m_taskId,
                          m_stage.stageName,
                          m_stageIndex,
                          behavior.behaviorName,
                          m_behaviorIndex,
                          success,
                          message);

    logInfo(QStringLiteral("behavior command finished: stage=%1[%2] behavior=%3[%4] command=%5 success=%6 message=%7")
                .arg(m_stage.stageName)
                .arg(m_stageIndex)
                .arg(behavior.behaviorName)
                .arg(m_behaviorIndex)
                .arg(command.id)
                .arg(success)
                .arg(message));

    if (!success) {
        finishStage(false,
                    QString("behavior '%1' failed: %2")
                        .arg(behavior.behaviorName, message));
        return;
    }

    ++m_behaviorIndex;
    if (m_behaviorIndex < m_stage.behaviors.size()) {
        sendCurrentBehavior();
        return;
    }

    finishStage(true, QStringLiteral("all stage behaviors completed"));
}

void FoupInStagedChargingStageExecutor::onCommandTimeoutRetry(
    ModbusCommand command,
    const QString &masterId)
{
    if (m_stopped
        || m_finishedEmitted
        || masterId != m_masterId
        || command.uuid != m_pendingCommandUuid
        || m_behaviorIndex < 0
        || m_behaviorIndex >= m_stage.behaviors.size()) {
        return;
    }

    const FoupInStagedChargingBehaviorConfig &behavior =
        m_stage.behaviors.at(m_behaviorIndex);
    const int retryCount = qMax(0, command.sendCount - 1);
    logWarn(QStringLiteral("behavior command retrying: stage=%1[%2] behavior=%3[%4] command=%5 retry=%6/%7")
                .arg(m_stage.stageName)
                .arg(m_stageIndex)
                .arg(behavior.behaviorName)
                .arg(m_behaviorIndex)
                .arg(command.id)
                .arg(retryCount)
                .arg(command.maxRetryCount));

    emit behaviorRetrying(m_taskId,
                          m_stage.stageName,
                          m_stageIndex,
                          behavior.behaviorName,
                          m_behaviorIndex,
                          retryCount,
                          command.maxRetryCount);
}

void FoupInStagedChargingStageExecutor::finishStage(bool success,
                                                     const QString &message)
{
    if (m_finishedEmitted) {
        return;
    }

    m_finishedEmitted = true;
    m_stopped = true;
    m_pendingCommandUuid = 0;
    disconnectSender();
    logInfo(QStringLiteral("stage executor finished: stage=%1[%2] success=%3 message=%4")
                .arg(m_stage.stageName)
                .arg(m_stageIndex)
                .arg(success)
                .arg(message));
    emit stageExecutionFinished(m_taskId,
                                m_stage.stageName,
                                m_stageIndex,
                                success,
                                message);
}

void FoupInStagedChargingStageExecutor::disconnectSender()
{
    for (const QMetaObject::Connection &connection : qAsConst(m_senderConnections)) {
        QObject::disconnect(connection);
    }
    m_senderConnections.clear();
}

ModbusCommand FoupInStagedChargingStageExecutor::buildCommand(
    const FoupInStagedChargingBehaviorConfig &behavior,
    QString *errorMessage) const
{
    CommandPool *pool = ModbusTcpMasterManager::instance().commandPool();
    if (!pool) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("command pool is not initialized");
        }
        return ModbusCommand();
    }
    if (!pool->contains(behavior.commandName)) {
        if (errorMessage) {
            *errorMessage = QString("command not found: %1").arg(behavior.commandName);
        }
        return ModbusCommand();
    }

    ModbusCommand command = pool->clone(behavior.commandName);
    if (!command.isValid()) {
        if (errorMessage) {
            *errorMessage = QString("failed to clone command: %1")
                                .arg(behavior.commandName);
        }
        return ModbusCommand();
    }

    command.module = CommandModule::BusinessCommandIssuer;
    if (!applyBehaviorParameters(&command, behavior.parameters, errorMessage)) {
        return ModbusCommand();
    }
    return command;
}

bool FoupInStagedChargingStageExecutor::applyBehaviorParameters(
    ModbusCommand *command,
    const QJsonObject &parameters,
    QString *errorMessage) const
{
    if (!command) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("command is null");
        }
        return false;
    }

    if (parameters.contains(QStringLiteral("startAddr"))) {
        quint16 startAddr = 0;
        if (!toUInt16(parameters.value(QStringLiteral("startAddr")),
                      &startAddr,
                      errorMessage)) {
            return false;
        }
        command->request.startAddr = startAddr;
        command->response.startAddr = startAddr;
        writeFrameU16(&command->request.rawBytes, 2, startAddr);
        writeFrameU16(&command->response.rawBytes, 2, startAddr);
    }

    QByteArray payload;
    bool hasPayload = false;
    if (parameters.contains(QStringLiteral("registerValue"))) {
        bool ok = false;
        payload = parseRegisterValue(parameters.value(QStringLiteral("registerValue")),
                                     &ok,
                                     errorMessage);
        if (!ok) {
            return false;
        }
        hasPayload = true;
    } else if (parameters.contains(QStringLiteral("registerValues"))) {
        const QJsonValue valuesValue = parameters.value(QStringLiteral("registerValues"));
        if (!valuesValue.isArray()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("registerValues must be an array");
            }
            return false;
        }

        for (const QJsonValue &value : valuesValue.toArray()) {
            quint16 registerValue = 0;
            if (!toUInt16(value, &registerValue, errorMessage)) {
                return false;
            }
            payload.append(registerBytes(registerValue));
        }
        hasPayload = true;
    } else {
        QString valueKey;
        double scale = 1.0;
        if (parameters.contains(QStringLiteral("flow"))) {
            valueKey = QStringLiteral("flow");
            scale = parameters.value(QStringLiteral("scale")).isDouble()
                ? parameters.value(QStringLiteral("scale")).toDouble()
                : 10.0;
        } else if (parameters.contains(QStringLiteral("value"))) {
            valueKey = QStringLiteral("value");
        } else if (parameters.contains(QStringLiteral("enable"))) {
            valueKey = QStringLiteral("enable");
        } else if (parameters.contains(QStringLiteral("gasType"))) {
            valueKey = QStringLiteral("gasType");
        }

        if (!valueKey.isEmpty()) {
            QJsonObject valueParameters = parameters;
            valueParameters.insert(QStringLiteral("scale"), scale);
            quint16 value = 0;
            if (!toScaledUInt16(valueParameters, valueKey, &value, errorMessage)) {
                return false;
            }
            payload = registerBytes(value);
            hasPayload = true;
        }
    }

    if (!hasPayload) {
        if (!parameters.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "parameters must contain registerValue, registerValues, flow, value, enable or gasType");
            }
            return false;
        }
        return true;
    }

    command->request.registerValue = payload;
    command->request.byteCount = static_cast<quint8>(payload.size());
    if (command->request.functionCode == 0x06 && payload.size() >= 2) {
        const quint16 value = static_cast<quint16>(
            (static_cast<quint8>(payload.at(0)) << 8)
            | static_cast<quint8>(payload.at(1)));
        writeFrameU16(&command->request.rawBytes, 4, value);
        command->response.registerValue = payload;
        writeFrameU16(&command->response.rawBytes, 4, value);
    }
    return true;
}

QByteArray FoupInStagedChargingStageExecutor::parseRegisterValue(
    const QJsonValue &value,
    bool *ok,
    QString *errorMessage)
{
    if (ok) {
        *ok = false;
    }

    if (value.isDouble()) {
        quint16 number = 0;
        if (!toUInt16(value, &number, errorMessage)) {
            return QByteArray();
        }
        if (ok) {
            *ok = true;
        }
        return registerBytes(number);
    }

    if (value.isString()) {
        QString text = value.toString();
        text.remove(QStringLiteral("0x"), Qt::CaseInsensitive);
        text.remove(QChar(' '));
        text.remove(QChar(','));
        text.remove(QChar(':'));
        text.remove(QChar('-'));
        if (text.isEmpty() || text.size() % 2 != 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("registerValue hex string must contain complete bytes");
            }
            return QByteArray();
        }

        const QByteArray bytes = QByteArray::fromHex(text.toLatin1());
        if (bytes.size() * 2 != text.size()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("registerValue contains non-hex characters");
            }
            return QByteArray();
        }
        if (ok) {
            *ok = true;
        }
        return bytes;
    }

    if (value.isArray()) {
        QByteArray bytes;
        for (const QJsonValue &item : value.toArray()) {
            quint16 byte = 0;
            if (!toUInt16(item, &byte, errorMessage) || byte > 0xFF) {
                if (errorMessage && errorMessage->isEmpty()) {
                    *errorMessage = QStringLiteral("registerValue byte array values must be 0..255");
                }
                return QByteArray();
            }
            bytes.append(static_cast<char>(byte));
        }
        if (bytes.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("registerValue byte array must not be empty");
            }
            return QByteArray();
        }
        if (ok) {
            *ok = true;
        }
        return bytes;
    }

    if (errorMessage) {
        *errorMessage = QStringLiteral("registerValue must be a number, hex string or byte array");
    }
    return QByteArray();
}

QByteArray FoupInStagedChargingStageExecutor::registerBytes(quint16 value)
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

bool FoupInStagedChargingStageExecutor::writeFrameU16(QByteArray *frame,
                                                      int offset,
                                                      quint16 value)
{
    if (!frame || offset < 0 || frame->size() < offset + 2) {
        return false;
    }
    (*frame)[offset] = static_cast<char>((value >> 8) & 0xFF);
    (*frame)[offset + 1] = static_cast<char>(value & 0xFF);
    return true;
}

bool FoupInStagedChargingStageExecutor::commandSucceeded(
    const ModbusCommand &command)
{
    return command.received
        && !command.timedOut
        && !command.checksumError
        && !command.deviceBusy;
}

QString FoupInStagedChargingStageExecutor::commandFailureMessage(
    const ModbusCommand &command)
{
    QStringList reasons;
    if (command.timedOut) {
        reasons << QStringLiteral("command timeout");
    }
    if (command.checksumError) {
        reasons << QStringLiteral("checksum error");
    }
    if (command.deviceBusy) {
        reasons << QStringLiteral("device busy");
    }
    if (!command.errorMessage.trimmed().isEmpty()) {
        reasons << command.errorMessage.trimmed();
    }
    return reasons.isEmpty() ? QStringLiteral("command failed") : reasons.join(QStringLiteral(", "));
}

void FoupInStagedChargingStageExecutor::logInfo(const QString &message)
{
    m_logger.info(message.toStdString());
}

void FoupInStagedChargingStageExecutor::logWarn(const QString &message)
{
    m_logger.warn(message.toStdString());
}
