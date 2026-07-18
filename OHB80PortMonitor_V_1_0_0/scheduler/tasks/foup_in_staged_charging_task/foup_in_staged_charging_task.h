/*******************************************************************************************
 * @file foup_in_staged_charging_task.h
 * @author Simon <工号：13> 2026-07-18
 *
 * @class FoupInStagedChargingTask
 * @brief 负责在 FOUP IN 后按顺序调度前置准备阶段和多个充气阶段。
 *
 * 设计目标：
 *      1. 只负责阶段索引、阶段切换、阶段计时和任务生命周期。
 *      2. 将单个阶段的行为执行委托给 FoupInStagedChargingStageExecutor。
 *      3. 通过信号向外部报告准备、行为、阶段和任务结果。
 *******************************************************************************************/
#ifndef FOUP_IN_STAGED_CHARGING_TASK_H
#define FOUP_IN_STAGED_CHARGING_TASK_H

#include "../../scheduler_task.h"
#include "foup_in_staged_charging_task_types.h"
#include "ilogger.h"

#include <QJsonObject>
#include <QString>

class QTimer;
class ModbusCommandSender;
class FoupInStagedChargingStageExecutor;

class FoupInStagedChargingTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    // 创建绑定指定设备和 JSON 配置的分阶段充气任务。
    explicit FoupInStagedChargingTask(const QString &masterId,
                                      const QJsonObject &config,
                                      QObject *parent = nullptr);

    // 释放阶段执行器和任务计时器。
    ~FoupInStagedChargingTask() override;

    // 启动抽真空前置准备阶段。
    void start() override;

    // 停止当前阶段、计时器和任务。
    void stop() override;

    // 返回调度器使用的任务类型名称。
    QString taskType() const override
    {
        return QStringLiteral("FoupInStagedChargingTask");
    }

    // 解析并校验用户提供的分阶段充气 JSON 配置。
    static bool parseConfig(const QJsonObject &json,
                            FoupInStagedChargingTaskConfig *config,
                            QString *errorMessage = nullptr);

signals:
    // ---- 前置准备阶段 ----
    // 通知抽真空前置准备阶段开始。
    void preparationStarted(QString taskId, QString masterId);

    // 通知抽真空前置准备阶段结束。
    void preparationFinished(QString taskId, QString masterId);

    // 报告前置准备期间读取到的设备进气流量。
    void preparationFlowObserved(QString taskId, QString masterId, double inletFlow);

    // ---- 充气阶段 ----
    // 通知一个阶段的行为全部成功并开始阶段计时。
    void stageStarted(QString taskId, QString stageName, int stageIndex);

    // 通知一个阶段的持续时间计时结束。
    void stageFinished(QString taskId, QString stageName, int stageIndex);

    // ---- 行为执行 ----
    // 转发阶段执行器的行为开始通知。
    void behaviorStarted(QString taskId,
                         QString stageName,
                         int stageIndex,
                         QString behaviorName,
                         int behaviorIndex);

    // 转发阶段执行器的行为完成通知。
    void behaviorFinished(QString taskId,
                          QString stageName,
                          int stageIndex,
                          QString behaviorName,
                          int behaviorIndex,
                          bool success,
                          QString errorMessage);

    // 转发阶段执行器的行为重试通知。
    void behaviorRetrying(QString taskId,
                          QString stageName,
                          int stageIndex,
                          QString behaviorName,
                          int behaviorIndex,
                          int retryCount,
                          int maxRetryCount);

private slots:
    // 处理前置准备或充气阶段共用计时器的超时事件。
    void onPhaseTimerTimeout();

    // 读取并报告前置准备期间的进气流量。
    void onPreparationFlowSample();

    // 处理阶段执行器返回的阶段完成结果。
    void onStageExecutionFinished(QString taskId,
                                  QString stageName,
                                  int stageIndex,
                                  bool success,
                                  QString errorMessage);

private:
    // ---- 任务流程 ----
    // 启动固定时长的抽真空前置准备阶段。
    void startPreparation();

    // 进入当前阶段并创建阶段执行器。
    void beginCurrentStage();

    // 启动当前阶段执行器并转发其行为信号。
    void startStageExecutor();

    // 在当前阶段所有行为成功后启动阶段计时。
    void startCurrentStageTimer();

    // 统一停止资源、更新状态并发送任务完成信号。
    void finishTask(bool success,
                    const QString &message,
                    SchedulerTask::State finalState);

    // ---- 配置解析 ----
    // 读取并校验非负整数配置项。
    static bool readNonNegativeInt(const QJsonObject &object,
                                   const QString &key,
                                   int defaultValue,
                                   int *value,
                                   QString *errorMessage,
                                   bool required = false);

    // ---- 日志 ----
    // 写入任务信息日志。
    void logInfo(const QString &message);

    // 写入任务警告日志。
    void logWarn(const QString &message);

private:
    // ---- 任务配置 ----
    QString m_masterId;
    QJsonObject m_jsonConfig;
    FoupInStagedChargingTaskConfig m_config;

    // ---- 设备和阶段执行器 ----
    ModbusCommandSender *m_sender = nullptr;
    FoupInStagedChargingStageExecutor *m_stageExecutor = nullptr;

    // ---- 计时器和阶段位置 ----
    QTimer *m_phaseTimer = nullptr;
    QTimer *m_preparationFlowTimer = nullptr;
    FoupInStagedChargingTimerPurpose m_timerPurpose =
        FoupInStagedChargingTimerPurpose::None;
    int m_stageIndex = 0;

    // ---- 生命周期状态 ----
    bool m_started = false;
    bool m_stopped = false;
    bool m_taskFinishedEmitted = false;

    // ---- 日志 ----
    ILogger m_logger;
};

#endif // FOUP_IN_STAGED_CHARGING_TASK_H
