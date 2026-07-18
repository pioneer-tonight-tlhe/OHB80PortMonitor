/*******************************************************************************************
 * @file foup_in_staged_charging_stage_executor.h
 * @author Simon <工号：13> 2026-07-18
 *
 * @class FoupInStagedChargingStageExecutor
 * @brief 负责执行一个充气阶段内的行为和 Modbus 指令。
 *
 * 设计目标：
 *      1. 将行为串行执行和指令响应处理从调度任务中隔离。
 *      2. 保证当前行为最终成功后才执行下一个行为。
 *      3. 向调度任务报告行为结果和阶段最终结果，不负责阶段持续时间计时。
 *******************************************************************************************/
#ifndef FOUP_IN_STAGED_CHARGING_STAGE_EXECUTOR_H
#define FOUP_IN_STAGED_CHARGING_STAGE_EXECUTOR_H

#include "foup_in_staged_charging_task_types.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QString>

class ModbusCommandSender;

class FoupInStagedChargingStageExecutor : public QObject
{
    Q_OBJECT

public:
    // 创建绑定一个充气阶段和一个设备的阶段执行器。
    explicit FoupInStagedChargingStageExecutor(
        const QString &taskId,
        const QString &masterId,
        int stageIndex,
        const FoupInStagedChargingStageConfig &stage,
        ModbusCommandSender *sender,
        QObject *parent = nullptr);

    // 停止当前阶段执行并断开底层指令信号。
    ~FoupInStagedChargingStageExecutor() override;

    // 开始执行当前阶段的第一个行为。
    void start();

    // 停止当前阶段，忽略后续的迟到指令响应。
    void stop();

signals:
    // ---- 行为执行 ----
    // 通知一个行为对应的指令开始下发。
    void behaviorStarted(QString taskId,
                         QString stageName,
                         int stageIndex,
                         QString behaviorName,
                         int behaviorIndex);

    // 通知一个行为对应的指令收到最终结果。
    void behaviorFinished(QString taskId,
                          QString stageName,
                          int stageIndex,
                          QString behaviorName,
                          int behaviorIndex,
                          bool success,
                          QString errorMessage);

    // 通知一个行为对应的指令正在重试。
    void behaviorRetrying(QString taskId,
                          QString stageName,
                          int stageIndex,
                          QString behaviorName,
                          int behaviorIndex,
                          int retryCount,
                          int maxRetryCount);

    // 通知调度任务当前阶段的行为是否全部执行成功。
    void stageExecutionFinished(QString taskId,
                                QString stageName,
                                int stageIndex,
                                bool success,
                                QString errorMessage);

private slots:
    // 处理当前行为指令的最终结果。
    void onCommandFinished(ModbusCommand command, const QString &masterId);

    // 处理当前行为指令的超时重试通知。
    void onCommandTimeoutRetry(ModbusCommand command, const QString &masterId);

private:
    // ---- 阶段执行 ----
    // 下发当前行为对应的单条 Modbus 指令。
    void sendCurrentBehavior();

    // 统一结束当前阶段并发送阶段结果。
    void finishStage(bool success, const QString &message);

    // 断开底层指令发送器信号。
    void disconnectSender();

    // ---- Modbus 指令 ----
    // 根据行为配置克隆并构建 Modbus 指令。
    ModbusCommand buildCommand(const FoupInStagedChargingBehaviorConfig &behavior,
                               QString *errorMessage) const;

    // 将行为参数写入 Modbus 指令模板。
    bool applyBehaviorParameters(ModbusCommand *command,
                                 const QJsonObject &parameters,
                                 QString *errorMessage) const;

    // 解析 JSON 中的寄存器值或原始字节值。
    static QByteArray parseRegisterValue(const QJsonValue &value,
                                         bool *ok,
                                         QString *errorMessage);

    // 按大端序生成一个 16 位寄存器值。
    static QByteArray registerBytes(quint16 value);

    // 将 16 位值写入原始 Modbus 帧。
    static bool writeFrameU16(QByteArray *frame, int offset, quint16 value);

    // 判断 Modbus 指令是否成功完成。
    static bool commandSucceeded(const ModbusCommand &command);

    // 提取 Modbus 指令失败原因。
    static QString commandFailureMessage(const ModbusCommand &command);

    // 写入阶段执行日志。
    void logInfo(const QString &message);
    void logWarn(const QString &message);

private:
    // ---- 阶段配置 ----
    QString m_taskId;
    QString m_masterId;
    int m_stageIndex = -1;
    FoupInStagedChargingStageConfig m_stage;

    // ---- 执行状态 ----
    ModbusCommandSender *m_sender = nullptr;
    QList<QMetaObject::Connection> m_senderConnections;
    int m_behaviorIndex = 0;
    qint64 m_pendingCommandUuid = 0;
    bool m_started = false;
    bool m_stopped = false;
    bool m_finishedEmitted = false;

    // ---- 日志 ----
    ILogger m_logger;
};

#endif // FOUP_IN_STAGED_CHARGING_STAGE_EXECUTOR_H
