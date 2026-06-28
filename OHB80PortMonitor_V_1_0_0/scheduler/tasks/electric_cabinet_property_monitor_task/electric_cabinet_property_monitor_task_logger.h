/*******************************************************************************************
 * @file electric_cabinet_property_monitor_task_logger.h
 * @author Simon <工号：13> 2026-06-28
 *
 * @class ElectricCabinetPropertyMonitorTaskLogger
 * @brief 负责封装电控柜属性监控任务的单设备日志写入。
 *
 * 设计目标：
 *      1. 为单台电控柜属性轮询任务提供统一日志出口。
 *      2. 任务代码只关注轮询、解析和状态更新，日志格式由本类统一维护。
 *      3. 正常轮询与异常场景按日志级别区分，便于运行阶段定位串口和协议问题。
 *******************************************************************************************/
#ifndef ELECTRIC_CABINET_PROPERTY_MONITOR_TASK_LOGGER_H
#define ELECTRIC_CABINET_PROPERTY_MONITOR_TASK_LOGGER_H

#include "ilogger.h"

#include <QString>

class ElectricCabinetPropertyMonitorTaskLogger
{
public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetPropertyMonitorTaskLogger(bool enable = true);

    // ============================ 日志写入 ============================
    void debug(const QString& action, const QString& message);
    void info(const QString& action, const QString& message);
    void warn(const QString& action, const QString& message);
    void error(const QString& action, const QString& message);

private:
    // ---- 日志消息拼装 ----
    static QString buildMessage(const QString& action, const QString& message);

private:
    // ---- 日志实例 ----
    ILogger m_logger;
};

#endif // ELECTRIC_CABINET_PROPERTY_MONITOR_TASK_LOGGER_H
