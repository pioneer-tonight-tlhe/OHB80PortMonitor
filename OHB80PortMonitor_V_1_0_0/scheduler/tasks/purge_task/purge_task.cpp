#include "purge_task.h"

#include "appconfig.h"
#include "csvfilewriter.h"
#include "foupofohbinfo.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "shareddata.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonValue>
#include <QtGlobal>

namespace {
constexpr int kPurgeFlowRegisterScale = 10;

bool commandSucceeded(const ModbusCommand &cmd)
{
    return cmd.received
        && !cmd.timedOut
        && !cmd.checksumError
        && !cmd.deviceBusy;
}

bool jsonValueToDouble(const QJsonValue &value, double *out)
{
    if (!out) {
        return false;
    }

    if (value.isDouble()) {
        *out = value.toDouble();
        return true;
    }

    if (value.isString()) {
        bool ok = false;
        const double number = value.toString().toDouble(&ok);
        if (ok) {
            *out = number;
        }
        return ok;
    }

    return false;
}

bool jsonValueToUInt16(const QJsonValue &value, quint16 *out)
{
    double number = 0.0;
    if (!jsonValueToDouble(value, &number)) {
        return false;
    }

    const qint64 integer = qRound(number);
    if (integer < 0 || integer > 0xFFFF) {
        return false;
    }

    if (out) {
        *out = static_cast<quint16>(integer);
    }
    return true;
}
}

PurgeTask::PurgeTask(const PurgeTaskDefinition &definition, QObject *parent)
    : SchedulerTask(parent)
    , m_definition(definition)
{
}

PurgeTask::~PurgeTask()
{
    disconnectCommandSignals();
}

void PurgeTask::start()
{
    if (m_finishEmitted) {
        return;
    }

    QString errorMessage;
    if (!m_definition.isValid(&errorMessage)) {
        finishTask(false, errorMessage, SchedulerTask::Failed);
        return;
    }

    if (!SharedData::getFoupByQRCode(m_definition.qrCode)) {
        finishTask(false,
                   QStringLiteral("PurgeTask: shared OHB data not found, QRCode=%1").arg(m_definition.qrCode),
                   SchedulerTask::Failed);
        return;
    }

    if (!prepareOutputDirectory(&errorMessage)) {
        finishTask(false, errorMessage, SchedulerTask::Failed);
        return;
    }

    createTimersIfNeeded();
    m_cancelRequested = false;
    m_currentStageIndex = -1;
    m_currentActionIndex = -1;
    m_taskStartedAt = QDateTime::currentDateTime();

    setState(SchedulerTask::Running);
    emit outputDirectoryReady(m_outputDir);
    emit purgeStarted(m_definition.qrCode);
    emit progress(0, QStringLiteral("Purge task started: QRCode=%1").arg(m_definition.qrCode));

    startNextStage();
}

void PurgeTask::stop()
{
    if (m_finishEmitted) {
        return;
    }

    m_cancelRequested = true;
    finishTask(false, QStringLiteral("PurgeTask: cancelled"), SchedulerTask::Cancelled);
}

QString PurgeTask::outputDir() const
{
    return m_outputDir;
}

void PurgeTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_finishEmitted || m_cancelRequested || cmd.uuid != m_pendingCommandUuid) {
        return;
    }

    disconnectCommandSignals();
    m_pendingCommandUuid = 0;

    if (m_currentStageIndex < 0 || m_currentStageIndex >= m_definition.stages.size()) {
        finishTask(false, QStringLiteral("PurgeTask: command finished but stage index is invalid"),
                   SchedulerTask::Failed);
        return;
    }

    const PurgeStageDefinition &stage = m_definition.stages.at(m_currentStageIndex);
    if (m_currentActionIndex < 0 || m_currentActionIndex >= stage.actions.size()) {
        finishTask(false, QStringLiteral("PurgeTask: command finished but action index is invalid"),
                   SchedulerTask::Failed);
        return;
    }

    const PurgeActionDefinition &action = stage.actions.at(m_currentActionIndex);
    const bool success = commandSucceeded(cmd);
    const QString message = success
        ? QStringLiteral("OK")
        : (cmd.errorMessage.isEmpty() ? QStringLiteral("command failed") : cmd.errorMessage);

    emit purgeActionFinished(m_currentStageIndex + 1,
                             m_currentActionIndex + 1,
                             action.commandId,
                             success,
                             message);

    if (!success && action.required) {
        finishTask(false,
                   QStringLiteral("PurgeTask: stage %1 action %2 command %3 failed: %4")
                       .arg(m_currentStageIndex + 1)
                       .arg(m_currentActionIndex + 1)
                       .arg(action.commandId, message),
                   SchedulerTask::Failed);
        return;
    }

    startNextAction();
}

void PurgeTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_finishEmitted || cmd.uuid != m_pendingCommandUuid) {
        return;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);
    emit progress(0,
                  QStringLiteral("Purge command retry: %1 (%2/%3)")
                      .arg(cmd.id)
                      .arg(retryCount)
                      .arg(cmd.maxRetryCount));
}

void PurgeTask::onStageTimeout()
{
    if (m_finishEmitted || m_cancelRequested) {
        return;
    }

    finishCurrentStage();
}

void PurgeTask::onSampleTimeout()
{
    if (m_finishEmitted || m_cancelRequested || m_currentStageIndex < 0) {
        return;
    }

    const QStringList headers = csvHeaders();
    const QStringList row = currentCsvRow();
    QString errorMessage;

    if (!CsvFileWriter::appendRow(m_currentStageCsvPath, headers, row, &errorMessage)
        || !CsvFileWriter::appendRow(m_allStageCsvPath, headers, row, &errorMessage)) {
        finishTask(false,
                   QStringLiteral("PurgeTask: failed to write CSV record: %1").arg(errorMessage),
                   SchedulerTask::Failed);
    }
}

bool PurgeTask::prepareOutputDirectory(QString *errorMessage)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString baseDir = QDir(AppConfig::getInstance().getRootDir())
                                .filePath(QStringLiteral("OHB_%1_Monitor_Graph").arg(m_definition.qrCode));
    const QString dateDir = QDir(baseDir).filePath(now.toString(QStringLiteral("yyyyMMdd")));
    const QString qrDir = QDir(dateDir).filePath(m_definition.qrCode);
    m_outputDir = QDir(qrDir).filePath(now.toString(QStringLiteral("HHmmss")));

    QDir dir;
    if (!dir.mkpath(m_outputDir)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeTask: failed to create output directory: %1").arg(m_outputDir);
        }
        return false;
    }

    m_allStageCsvPath = QDir(m_outputDir).filePath(QStringLiteral("stage_all.csv"));
    if (!CsvFileWriter::ensureFileWithHeader(m_allStageCsvPath, csvHeaders(), errorMessage)) {
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool PurgeTask::prepareStageCsv(QString *errorMessage)
{
    if (m_currentStageIndex < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeTask: current stage index is invalid");
        }
        return false;
    }

    m_currentStageCsvPath = QDir(m_outputDir)
                                .filePath(QStringLiteral("stage_%1.csv").arg(m_currentStageIndex + 1));
    return CsvFileWriter::ensureFileWithHeader(m_currentStageCsvPath, csvHeaders(), errorMessage);
}

void PurgeTask::createTimersIfNeeded()
{
    if (!m_stageTimer) {
        m_stageTimer = new QTimer(this);
        m_stageTimer->setSingleShot(true);
        connect(m_stageTimer, &QTimer::timeout, this, &PurgeTask::onStageTimeout);
    }

    if (!m_sampleTimer) {
        m_sampleTimer = new QTimer(this);
        m_sampleTimer->setInterval(1000);
        connect(m_sampleTimer, &QTimer::timeout, this, &PurgeTask::onSampleTimeout);
    }
}

void PurgeTask::stopTimers()
{
    if (m_stageTimer) {
        m_stageTimer->stop();
    }

    if (m_sampleTimer) {
        m_sampleTimer->stop();
    }
}

void PurgeTask::startNextStage()
{
    if (m_finishEmitted || m_cancelRequested) {
        return;
    }

    ++m_currentStageIndex;
    m_currentActionIndex = -1;

    if (m_currentStageIndex >= m_definition.stages.size()) {
        finishTask(true, QStringLiteral("PurgeTask: all stages finished"), SchedulerTask::Finished);
        return;
    }

    QString errorMessage;
    if (!prepareStageCsv(&errorMessage)) {
        finishTask(false, errorMessage, SchedulerTask::Failed);
        return;
    }

    const int percent = m_definition.stages.isEmpty()
        ? 0
        : qBound(0, (m_currentStageIndex * 100) / m_definition.stages.size(), 99);
    emit progress(percent,
                  QStringLiteral("Purge stage %1 preparing").arg(m_currentStageIndex + 1));

    startNextAction();
}

void PurgeTask::startNextAction()
{
    if (m_finishEmitted || m_cancelRequested) {
        return;
    }

    const PurgeStageDefinition &stage = m_definition.stages.at(m_currentStageIndex);
    ++m_currentActionIndex;

    if (m_currentActionIndex >= stage.actions.size()) {
        startStageTiming();
        return;
    }

    const PurgeActionDefinition &action = stage.actions.at(m_currentActionIndex);
    QString errorMessage;
    if (!submitActionCommand(action, &errorMessage)) {
        emit purgeActionFinished(m_currentStageIndex + 1,
                                 m_currentActionIndex + 1,
                                 action.commandId,
                                 false,
                                 errorMessage);
        if (action.required) {
            finishTask(false, errorMessage, SchedulerTask::Failed);
            return;
        }
        startNextAction();
    }
}

void PurgeTask::startStageTiming()
{
    if (m_finishEmitted || m_cancelRequested) {
        return;
    }

    const PurgeStageDefinition &stage = m_definition.stages.at(m_currentStageIndex);
    m_currentStageTimingStartedAt = QDateTime::currentDateTime();

    emit purgeStageStarted(m_currentStageIndex + 1,
                           stage.name,
                           stage.durationSeconds);
    emit progress(0,
                  QStringLiteral("Purge stage %1 started").arg(m_currentStageIndex + 1));

    onSampleTimeout();
    if (m_finishEmitted) {
        return;
    }

    if (m_sampleTimer) {
        m_sampleTimer->start();
    }

    if (stage.durationSeconds <= 0) {
        QTimer::singleShot(0, this, [this]() {
            finishCurrentStage();
        });
        return;
    }

    if (m_stageTimer) {
        const int timeoutMs = qBound(1, stage.durationSeconds, 24 * 60 * 60) * 1000;
        m_stageTimer->start(timeoutMs);
    }
}

void PurgeTask::finishCurrentStage()
{
    if (m_finishEmitted || m_cancelRequested) {
        return;
    }

    if (m_sampleTimer) {
        m_sampleTimer->stop();
    }

    const PurgeStageDefinition &stage = m_definition.stages.at(m_currentStageIndex);
    emit purgeStageFinished(m_currentStageIndex + 1, stage.name);

    const int percent = m_definition.stages.isEmpty()
        ? 100
        : qBound(0, ((m_currentStageIndex + 1) * 100) / m_definition.stages.size(), 100);
    emit progress(percent,
                  QStringLiteral("Purge stage %1 finished").arg(m_currentStageIndex + 1));

    startNextStage();
}

void PurgeTask::finishTask(bool success,
                           const QString &message,
                           SchedulerTask::State finalState)
{
    if (m_finishEmitted) {
        return;
    }

    m_finishEmitted = true;
    stopTimers();
    disconnectCommandSignals();
    m_pendingCommandUuid = 0;
    m_pendingCommandId.clear();
    m_lastError = message;

    setState(finalState);
    emit purgeFinished(success, message, m_outputDir);
    emit finished(success, message);
}

bool PurgeTask::submitActionCommand(const PurgeActionDefinition &action, QString *errorMessage)
{
    if (action.commandId.trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeTask: action command id is empty");
        }
        return false;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(action.commandId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeTask: command '%1' not found").arg(action.commandId);
        }
        return false;
    }

    ModbusTcpMaster *master = mgr.getMaster(m_definition.qrCode);
    if (!master || !master->isConnected() || !master->sender()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeTask: device unavailable, QRCode=%1").arg(m_definition.qrCode);
        }
        return false;
    }

    ModbusCommand cmd = pool->clone(action.commandId);
    if (!cmd.isValid()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeTask: failed to clone command '%1'").arg(action.commandId);
        }
        return false;
    }

    if (!applyActionParams(cmd, action, errorMessage)) {
        return false;
    }

    cmd.module = CommandModule::BusinessCommandIssuer;
    disconnectCommandSignals();

    ModbusCommandSender *sender = master->sender();
    m_pendingCommandUuid = cmd.uuid;
    m_pendingCommandId = cmd.id;

    m_commandConnections.append(connect(sender,
                                        &ModbusCommandSender::commandFinished,
                                        this,
                                        &PurgeTask::onCommandFinished,
                                        Qt::QueuedConnection));
    m_commandConnections.append(connect(sender,
                                        &ModbusCommandSender::commandTimeoutRetry,
                                        this,
                                        &PurgeTask::onCommandTimeoutRetry,
                                        Qt::QueuedConnection));

    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool PurgeTask::applyActionParams(ModbusCommand &cmd,
                                  const PurgeActionDefinition &action,
                                  QString *errorMessage) const
{
    const QJsonObject params = action.params;
    if (params.isEmpty()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    if (params.contains(QStringLiteral("flow_l_min"))) {
        double flow = 0.0;
        if (!jsonValueToDouble(params.value(QStringLiteral("flow_l_min")), &flow)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("PurgeTask: flow_l_min is invalid");
            }
            return false;
        }

        const qint64 registerValue = qRound(flow * kPurgeFlowRegisterScale);
        if (registerValue < 0 || registerValue > 0xFFFF) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("PurgeTask: flow_l_min is out of range");
            }
            return false;
        }

        applyRegisterValue(cmd, buildRegisterValue(static_cast<quint16>(registerValue)));
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    const QString rawValueKey = params.contains(QStringLiteral("register_value"))
        ? QStringLiteral("register_value")
        : (params.contains(QStringLiteral("value")) ? QStringLiteral("value") : QString());

    if (!rawValueKey.isEmpty()) {
        quint16 value = 0;
        if (!jsonValueToUInt16(params.value(rawValueKey), &value)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("PurgeTask: register value is invalid");
            }
            return false;
        }

        applyRegisterValue(cmd, buildRegisterValue(value));
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    if (params.contains(QStringLiteral("register_values")) && params.value(QStringLiteral("register_values")).isArray()) {
        const QJsonArray values = params.value(QStringLiteral("register_values")).toArray();
        QByteArray registerBytes;
        registerBytes.reserve(values.size() * 2);

        for (const QJsonValue &jsonValue : values) {
            quint16 value = 0;
            if (!jsonValueToUInt16(jsonValue, &value)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("PurgeTask: register_values contains invalid value");
                }
                return false;
            }
            registerBytes.append(buildRegisterValue(value));
        }

        applyRegisterValue(cmd, registerBytes);
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QByteArray PurgeTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void PurgeTask::applyRegisterValue(ModbusCommand &cmd, const QByteArray &registerValue) const
{
    if (registerValue.isEmpty()) {
        return;
    }

    cmd.request.registerValue = registerValue;
    cmd.request.byteCount = static_cast<quint8>(registerValue.size());

    if (cmd.request.functionCode == 0x06
        && cmd.request.rawBytes.size() >= 6
        && registerValue.size() >= 2) {
        cmd.request.rawBytes[4] = registerValue[0];
        cmd.request.rawBytes[5] = registerValue[1];
    } else if (cmd.request.functionCode == 0x10
               && cmd.request.rawBytes.size() >= 7 + registerValue.size()) {
        const quint16 registerCount = static_cast<quint16>(registerValue.size() / 2);
        cmd.request.count = registerCount;
        cmd.request.rawBytes[4] = static_cast<char>((registerCount >> 8) & 0xFF);
        cmd.request.rawBytes[5] = static_cast<char>(registerCount & 0xFF);
        cmd.request.rawBytes[6] = static_cast<char>(registerValue.size());
        for (int i = 0; i < registerValue.size(); ++i) {
            cmd.request.rawBytes[7 + i] = registerValue.at(i);
        }
    }

    if (cmd.request.functionCode == 0x06 && registerValue.size() >= 2) {
        cmd.response.registerValue = registerValue;
        if (cmd.response.rawBytes.size() >= 6) {
            cmd.response.rawBytes[4] = registerValue[0];
            cmd.response.rawBytes[5] = registerValue[1];
        }
    }
}

void PurgeTask::disconnectCommandSignals()
{
    for (const QMetaObject::Connection &connection : qAsConst(m_commandConnections)) {
        QObject::disconnect(connection);
    }
    m_commandConnections.clear();
}

QStringList PurgeTask::csvHeaders() const
{
    return {
        QStringLiteral("timestamp"),
        QStringLiteral("qr_code"),
        QStringLiteral("stage_no"),
        QStringLiteral("stage_name"),
        QStringLiteral("inlet_pressure"),
        QStringLiteral("negative_pressure"),
        QStringLiteral("inlet_flow"),
        QStringLiteral("humidity"),
        QStringLiteral("temperature"),
        QStringLiteral("foup_in"),
        QStringLiteral("idle_purge_enabled"),
        QStringLiteral("idle_state"),
        QStringLiteral("idle_working_time_sec")
    };
}

QStringList PurgeTask::currentCsvRow() const
{
    const FoupOfOHBInfo *foup = SharedData::getFoupByQRCode(m_definition.qrCode);
    const PurgeStageDefinition &stage = m_definition.stages.at(m_currentStageIndex);

    return {
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
        m_definition.qrCode,
        QString::number(m_currentStageIndex + 1),
        stage.name,
        foup ? formatNumber(foup->inletPressure()) : QString(),
        foup ? formatNumber(foup->negativePressure()) : QString(),
        foup ? formatNumber(foup->inletFlow()) : QString(),
        foup ? formatNumber(foup->RH()) : QString(),
        foup ? formatNumber(foup->temperature()) : QString(),
        foup ? QString::number(foup->foupIn() ? 1 : 0) : QString(),
        foup ? QString::number(foup->idlePurgeEnabled() ? 1 : 0) : QString(),
        foup ? QString::number(static_cast<int>(foup->idleState())) : QString(),
        foup ? QString::number(foup->idleWorkingTimeSec()) : QString()
    };
}

QString PurgeTask::formatNumber(double value, int precision) const
{
    return QString::number(value, 'f', precision);
}
