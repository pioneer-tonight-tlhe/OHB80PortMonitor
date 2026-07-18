/*******************************************************************************************
 * @file foup_in_staged_charging_task_types.h
 * @brief 定义 FOUP IN 分阶段充气任务使用的配置数据结构和辅助枚举。
 * @author Simon <工号：13> 2026-07-18
 *
 * 设计目标：
 *      1. 将用户配置数据与调度任务执行逻辑分离。
 *      2. 统一描述前置准备阶段、充气阶段和阶段行为。
 *      3. 为任务状态机提供独立的计时用途定义。
 *******************************************************************************************/
#ifndef FOUP_IN_STAGED_CHARGING_TASK_TYPES_H
#define FOUP_IN_STAGED_CHARGING_TASK_TYPES_H

#include <QJsonObject>
#include <QString>
#include <QVector>

// 一个行为只对应一条实际执行的 Modbus 指令。
struct FoupInStagedChargingBehaviorConfig
{
    QString behaviorName;   ///< 行为名称，用于日志和状态展示。
    QString commandName;    ///< 行为对应的 Modbus 指令名称。
    QJsonObject parameters; ///< 生成 Modbus 指令所需的参数。
};

// 一个充气阶段包含阶段持续时间和按顺序执行的行为列表。
struct FoupInStagedChargingStageConfig
{
    QString stageName;   ///< 阶段名称，用于日志和状态展示。
    int durationSeconds = 0; ///< 阶段持续时间，单位为秒。
    QVector<FoupInStagedChargingBehaviorConfig> behaviors; ///< 阶段行为列表。
};

// FOUP IN 后抽真空前置准备阶段的固定等待配置。
struct FoupInStagedChargingPreparationConfig
{
    int durationSeconds = 0; ///< 前置准备时间，单位为秒。
};

// 完整的分阶段充气任务配置。
struct FoupInStagedChargingTaskConfig
{
    QString taskName; ///< 任务名称。
    FoupInStagedChargingPreparationConfig preparation; ///< 前置准备配置。
    QVector<FoupInStagedChargingStageConfig> stages; ///< 充气阶段列表。
};

// 任务共用阶段计时器当前服务的流程阶段。
enum class FoupInStagedChargingTimerPurpose
{
    None,               ///< 当前没有运行中的阶段计时。
    VacuumPreparation,  ///< 正在等待抽真空前置准备时间结束。
    ChargingStage       ///< 正在执行充气阶段持续时间计时。
};

#endif // FOUP_IN_STAGED_CHARGING_TASK_TYPES_H
