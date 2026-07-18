/*******************************************************************************************
 * @file foup_in_staged_charging_task.cpp
 * @author Simon <工号：13> 2026-07-18
 *
 * @brief 实现分阶段充气任务的前置准备、阶段切换和阶段计时。
 *******************************************************************************************/
#include "foup_in_staged_charging_task.h"

#include "app/shareddata.h"
#include "classes/foupofohbinfo.h"
#include "foup_in_staged_charging_stage_executor.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QTimer>

#include <cmath>
#include <limits>

namespace {

constexpr int PreparationFlowSampleIntervalMs = 1000;
constexpr int MaxTimerSeconds = std::numeric_limits<int>::max() / 1000;

} // namespace

FoupInStagedChargingTask::FoupInStagedChargingTask(const QString &masterId,
                                                   const QJsonObject &config,
                                                   QObject *parent)
    : SchedulerTask(parent)
    , m_masterId(masterId)
    , m_jsonConfig(config)
    , m_logger("scheduler/foup_in_staged_charging_task")
{
}

FoupInStagedChargingTask::~FoupInStagedChargingTask()
{
    if (m_stageExecutor) {
        m_stageExecutor->stop();
        delete m_stageExecutor;
        m_stageExecutor = nullptr;
    }
}

// 校验任务配置和设备资源后，进入固定时长的抽真空前置准备阶段。
void FoupInStagedChargingTask::start()
{
    if (m_started) {
        return;
    }

    m_started = true;
    m_stopped = false;
    m_taskFinishedEmitted = false;
    m_stageIndex = 0;
    setState(Running);

    QString errorMessage;
    if (m_masterId.trimmed().isEmpty()) {
        finishTask(false, QStringLiteral("masterId is empty"), Failed);
        return;
    }
    if (!parseConfig(m_jsonConfig, &m_config, &errorMessage)) {
        finishTask(false,
                   QStringLiteral("invalid charging task config: %1").arg(errorMessage),
                   Failed);
        return;
    }

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    ModbusTcpMaster *master = manager.getMaster(m_masterId);
    if (!master) {
        finishTask(false,
                   QStringLiteral("master not found: %1").arg(m_masterId),
                   Failed);
        return;
    }

    m_sender = master->sender();
    if (!m_sender) {
        finishTask(false,
                   QStringLiteral("command sender is null: %1").arg(m_masterId),
                   Failed);
        return;
    }
    if (!master->isConnected()) {
        finishTask(false,
                   QStringLiteral("device is not connected: %1").arg(m_masterId),
                   Failed);
        return;
    }

    if (!m_phaseTimer) {
        m_phaseTimer = new QTimer(this);
        m_phaseTimer->setSingleShot(true);
        connect(m_phaseTimer,
                &QTimer::timeout,
                this,
                &FoupInStagedChargingTask::onPhaseTimerTimeout);
    }
    if (!m_preparationFlowTimer) {
        m_preparationFlowTimer = new QTimer(this);
        m_preparationFlowTimer->setInterval(PreparationFlowSampleIntervalMs);
        connect(m_preparationFlowTimer,
                &QTimer::timeout,
                this,
                &FoupInStagedChargingTask::onPreparationFlowSample);
    }

    logInfo(QStringLiteral("task started: taskName=%1 masterId=%2 stages=%3")
                .arg(m_config.taskName)
                .arg(m_masterId)
                .arg(m_config.stages.size()));
    startPreparation();
}

// 停止阶段执行器和计时器，并以取消结果结束任务。
void FoupInStagedChargingTask::stop()
{
    if (m_taskFinishedEmitted) {
        return;
    }

    m_stopped = true;
    finishTask(false, QStringLiteral("task cancelled"), Cancelled);
}

bool FoupInStagedChargingTask::parseConfig(
    const QJsonObject &json,
    FoupInStagedChargingTaskConfig *config,
    QString *errorMessage)
{
    if (!config) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("output config is null");
        }
        return false;
    }

    *config = FoupInStagedChargingTaskConfig();
    config->taskName = json.value(QStringLiteral("taskName")).toString().trimmed();
    if (config->taskName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("taskName is required");
        }
        return false;
    }

    const QJsonValue preparationValue = json.value(QStringLiteral("preparation"));
    if (!preparationValue.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("preparation object is required");
        }
        return false;
    }

    if (!readNonNegativeInt(preparationValue.toObject(),
                            QStringLiteral("durationSeconds"),
                            0,
                            &config->preparation.durationSeconds,
                            errorMessage,
                            true)) {
        return false;
    }
    if (config->preparation.durationSeconds > MaxTimerSeconds) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("preparation.durationSeconds is too large");
        }
        return false;
    }

    const QJsonValue stagesValue = json.value(QStringLiteral("stages"));
    if (!stagesValue.isArray() || stagesValue.toArray().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("stages must be a non-empty array");
        }
        return false;
    }

    const QJsonArray stages = stagesValue.toArray();
    for (int stageIndex = 0; stageIndex < stages.size(); ++stageIndex) {
        const QJsonValue stageValue = stages.at(stageIndex);
        if (!stageValue.isObject()) {
            if (errorMessage) {
                *errorMessage = QString("stages[%1] must be an object").arg(stageIndex);
            }
            return false;
        }

        const QJsonObject stageObject = stageValue.toObject();
        FoupInStagedChargingStageConfig stage;
        stage.stageName = stageObject.value(QStringLiteral("stageName"))
                              .toString()
                              .trimmed();
        if (stage.stageName.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QString("stages[%1].stageName is required").arg(stageIndex);
            }
            return false;
        }

        QString valueError;
        if (!readNonNegativeInt(stageObject,
                                QStringLiteral("durationSeconds"),
                                0,
                                &stage.durationSeconds,
                                &valueError,
                                true)) {
            if (errorMessage) {
                *errorMessage = QString("stages[%1].%2")
                                    .arg(stageIndex)
                                    .arg(valueError);
            }
            return false;
        }
        if (stage.durationSeconds > MaxTimerSeconds) {
            if (errorMessage) {
                *errorMessage = QString("stages[%1].durationSeconds is too large")
                                    .arg(stageIndex);
            }
            return false;
        }

        const QJsonValue behaviorsValue = stageObject.value(QStringLiteral("behaviors"));
        if (!behaviorsValue.isArray() || behaviorsValue.toArray().isEmpty()) {
            if (errorMessage) {
                *errorMessage = QString("stages[%1].behaviors must be a non-empty array")
                                    .arg(stageIndex);
            }
            return false;
        }

        const QJsonArray behaviors = behaviorsValue.toArray();
        for (int behaviorIndex = 0; behaviorIndex < behaviors.size(); ++behaviorIndex) {
            const QJsonValue behaviorValue = behaviors.at(behaviorIndex);
            if (!behaviorValue.isObject()) {
                if (errorMessage) {
                    *errorMessage = QString("stages[%1].behaviors[%2] must be an object")
                                        .arg(stageIndex)
                                        .arg(behaviorIndex);
                }
                return false;
            }

            const QJsonObject behaviorObject = behaviorValue.toObject();
            FoupInStagedChargingBehaviorConfig behavior;
            behavior.behaviorName = behaviorObject.value(QStringLiteral("behaviorName"))
                                        .toString()
                                        .trimmed();
            behavior.commandName = behaviorObject.value(QStringLiteral("commandName"))
                                       .toString()
                                       .trimmed();
            if (behavior.behaviorName.isEmpty() || behavior.commandName.isEmpty()) {
                if (errorMessage) {
                    *errorMessage = QString("stages[%1].behaviors[%2] requires behaviorName and commandName")
                                        .arg(stageIndex)
                                        .arg(behaviorIndex);
                }
                return false;
            }

            const QJsonValue parametersValue = behaviorObject.value(QStringLiteral("parameters"));
            if (!parametersValue.isUndefined() && !parametersValue.isObject()) {
                if (errorMessage) {
                    *errorMessage = QString("stages[%1].behaviors[%2].parameters must be an object")
                                        .arg(stageIndex)
                                        .arg(behaviorIndex);
                }
                return false;
            }
            behavior.parameters = parametersValue.isObject()
                ? parametersValue.toObject()
                : QJsonObject();
            stage.behaviors.append(behavior);
        }

        config->stages.append(stage);
    }

    return true;
}

void FoupInStagedChargingTask::startPreparation()
{
    // 前置阶段只做固定时间等待和流量观测，不根据流量值判断异常。
    m_timerPurpose = FoupInStagedChargingTimerPurpose::VacuumPreparation;
    emit preparationStarted(taskId(), m_masterId);
    logInfo(QStringLiteral("vacuum preparation started: durationSeconds=%1")
                .arg(m_config.preparation.durationSeconds));

    m_preparationFlowTimer->start();
    onPreparationFlowSample();
    m_phaseTimer->start(m_config.preparation.durationSeconds * 1000);
}

void FoupInStagedChargingTask::onPreparationFlowSample()
{
    FoupOfOHBInfo *foup = SharedData::getFoupByQRCode(m_masterId);
    if (!foup) {
        logWarn(QStringLiteral("preparation flow sample skipped: FOUP not found for %1")
                    .arg(m_masterId));
        return;
    }

    const double flow = foup->inletFlow();
    emit preparationFlowObserved(taskId(), m_masterId, flow);
    logInfo(QStringLiteral("preparation flow observed: masterId=%1 inletFlow=%2")
                .arg(m_masterId)
                .arg(flow, 0, 'f', 3));
}

void FoupInStagedChargingTask::onPhaseTimerTimeout()
{
    const FoupInStagedChargingTimerPurpose purpose = m_timerPurpose;
    m_timerPurpose = FoupInStagedChargingTimerPurpose::None;
    if (m_stopped || m_taskFinishedEmitted) {
        return;
    }

    if (purpose == FoupInStagedChargingTimerPurpose::VacuumPreparation) {
        // 前置准备时间到达后直接进入第一充气阶段。
        m_preparationFlowTimer->stop();
        emit preparationFinished(taskId(), m_masterId);
        beginCurrentStage();
        return;
    }

    if (purpose != FoupInStagedChargingTimerPurpose::ChargingStage
        || m_stageIndex < 0
        || m_stageIndex >= m_config.stages.size()) {
        finishTask(false, QStringLiteral("invalid phase timer state"), Failed);
        return;
    }

    const FoupInStagedChargingStageConfig &stage = m_config.stages.at(m_stageIndex);
    emit stageFinished(taskId(), stage.stageName, m_stageIndex);
    logInfo(QStringLiteral("stage finished: index=%1 name=%2")
                .arg(m_stageIndex)
                .arg(stage.stageName));
    ++m_stageIndex;
    beginCurrentStage();
}

void FoupInStagedChargingTask::beginCurrentStage()
{
    if (m_stopped || m_taskFinishedEmitted) {
        return;
    }
    if (m_stageIndex >= m_config.stages.size()) {
        finishTask(true, QStringLiteral("all charging stages completed"), Finished);
        return;
    }
    startStageExecutor();
}

void FoupInStagedChargingTask::startStageExecutor()
{
    if (m_stageExecutor) {
        m_stageExecutor->stop();
        delete m_stageExecutor;
        m_stageExecutor = nullptr;
    }

    const FoupInStagedChargingStageConfig &stage = m_config.stages.at(m_stageIndex);
    m_stageExecutor = new FoupInStagedChargingStageExecutor(
        taskId(),
        m_masterId,
        m_stageIndex,
        stage,
        m_sender,
        this);

    connect(m_stageExecutor,
            &FoupInStagedChargingStageExecutor::behaviorStarted,
            this,
            &FoupInStagedChargingTask::behaviorStarted);
    connect(m_stageExecutor,
            &FoupInStagedChargingStageExecutor::behaviorFinished,
            this,
            &FoupInStagedChargingTask::behaviorFinished);
    connect(m_stageExecutor,
            &FoupInStagedChargingStageExecutor::behaviorRetrying,
            this,
            &FoupInStagedChargingTask::behaviorRetrying);
    connect(m_stageExecutor,
            &FoupInStagedChargingStageExecutor::stageExecutionFinished,
            this,
            &FoupInStagedChargingTask::onStageExecutionFinished);

    logInfo(QStringLiteral("starting stage executor: index=%1 name=%2")
                .arg(m_stageIndex)
                .arg(stage.stageName));
    m_stageExecutor->start();
}

void FoupInStagedChargingTask::onStageExecutionFinished(
    QString taskId,
    QString stageName,
    int stageIndex,
    bool success,
    QString errorMessage)
{
    if (m_stopped
        || m_taskFinishedEmitted
        || taskId != this->taskId()
        || stageIndex != m_stageIndex
        || m_stageIndex < 0
        || m_stageIndex >= m_config.stages.size()
        || stageName != m_config.stages.at(m_stageIndex).stageName) {
        return;
    }

    if (!success) {
        finishTask(false, errorMessage, Failed);
        return;
    }

    startCurrentStageTimer();
}

void FoupInStagedChargingTask::startCurrentStageTimer()
{
    if (m_stageIndex < 0 || m_stageIndex >= m_config.stages.size()) {
        finishTask(false, QStringLiteral("cannot start timer for invalid stage"), Failed);
        return;
    }

    const FoupInStagedChargingStageConfig &stage = m_config.stages.at(m_stageIndex);
    // 阶段计时只能在该阶段全部行为成功后启动。
    emit stageStarted(taskId(), stage.stageName, m_stageIndex);
    logInfo(QStringLiteral("stage started: index=%1 name=%2 durationSeconds=%3")
                .arg(m_stageIndex)
                .arg(stage.stageName)
                .arg(stage.durationSeconds));

    m_timerPurpose = FoupInStagedChargingTimerPurpose::ChargingStage;
    m_phaseTimer->start(stage.durationSeconds * 1000);
}

void FoupInStagedChargingTask::finishTask(bool success,
                                          const QString &message,
                                          SchedulerTask::State finalState)
{
    if (m_taskFinishedEmitted) {
        return;
    }

    // 所有结束路径统一收口，保证资源和完成信号只处理一次。
    m_taskFinishedEmitted = true;
    m_stopped = true;
    m_timerPurpose = FoupInStagedChargingTimerPurpose::None;
    if (m_phaseTimer) {
        m_phaseTimer->stop();
    }
    if (m_preparationFlowTimer) {
        m_preparationFlowTimer->stop();
    }
    if (m_stageExecutor) {
        m_stageExecutor->stop();
        delete m_stageExecutor;
        m_stageExecutor = nullptr;
    }
    m_sender = nullptr;

    setState(finalState);
    logInfo(QStringLiteral("task finished: success=%1 message=%2")
                .arg(success)
                .arg(message));
    emit finished(success, message);
}

bool FoupInStagedChargingTask::readNonNegativeInt(
    const QJsonObject &object,
    const QString &key,
    int defaultValue,
    int *value,
    QString *errorMessage,
    bool required)
{
    if (!value) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("output value is null");
        }
        return false;
    }

    const QJsonValue jsonValue = object.value(key);
    if (jsonValue.isUndefined()) {
        if (required) {
            if (errorMessage) {
                *errorMessage = QString("%1 is required").arg(key);
            }
            return false;
        }
        *value = defaultValue;
        return true;
    }

    if (!jsonValue.isDouble()) {
        if (errorMessage) {
            *errorMessage = QString("%1 must be a non-negative integer").arg(key);
        }
        return false;
    }

    const double number = jsonValue.toDouble();
    if (!std::isfinite(number)
        || number < 0.0
        || number > static_cast<double>(std::numeric_limits<int>::max())
        || std::floor(number) != number) {
        if (errorMessage) {
            *errorMessage = QString("%1 must be a non-negative integer").arg(key);
        }
        return false;
    }

    *value = static_cast<int>(number);
    return true;
}

void FoupInStagedChargingTask::logInfo(const QString &message)
{
    m_logger.info(message.toStdString());
}

void FoupInStagedChargingTask::logWarn(const QString &message)
{
    m_logger.warn(message.toStdString());
}
