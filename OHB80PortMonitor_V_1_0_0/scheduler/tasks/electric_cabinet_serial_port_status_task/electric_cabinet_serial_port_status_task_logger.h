/*******************************************************************************************
 * @file electric_cabinet_serial_port_status_task_logger.h
 * @author Codex 2026-06-28
 *
 * @class ElectricCabinetSerialPortStatusTaskLogger
 * @brief 负责封装电控柜串口状态任务的统一日志写入。
 *
 * 设计目标：
 *      1. 为单个电控柜串口监听任务提供统一日志出口。
 *      2. 统一任务日志前缀与输出格式，便于检索与排查问题。
 *      3. 让状态任务代码聚焦业务流程，避免重复拼接日志消息。
 *******************************************************************************************/
#ifndef ELECTRIC_CABINET_SERIAL_PORT_STATUS_TASK_LOGGER_H
#define ELECTRIC_CABINET_SERIAL_PORT_STATUS_TASK_LOGGER_H

#include "ilogger.h"

#include <QString>

class ElectricCabinetSerialPortStatusTaskLogger
{
public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetSerialPortStatusTaskLogger(bool enable = true);

    // ============================ 日志写入 ============================
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

#endif // ELECTRIC_CABINET_SERIAL_PORT_STATUS_TASK_LOGGER_H
