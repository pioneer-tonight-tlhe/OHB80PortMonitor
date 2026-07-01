/*******************************************************************************************
 * @file alarm_dispatch_task_logger.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class AlarmDispatchTaskLogger
 * @brief 封装警报调度任务使用的 summary 与 device 日志对象。
 *
 * 设计目标：
 *      1. 统一管理 `AlarmDispatchTask` 相关日志对象的启停和访问入口。
 *      2. 避免调度任务流程代码直接维护底层 logger 实例。
 *      3. 将 summary 日志与设备日志输出策略收敛在单一类中管理。
 *******************************************************************************************/
#ifndef ALARM_DISPATCH_TASK_LOGGER_H
#define ALARM_DISPATCH_TASK_LOGGER_H

#include <QString>

#include "ilogger.h"

class AlarmDispatchTaskLogger
{
public:
    // ============================ 构造函数 ============================
    AlarmDispatchTaskLogger(bool summaryEnable, bool devicesEnable);

    // ============================ 日志访问 ============================
    ILogger& summaryLogger();
    ILogger& deviceLogger(const QString& qrCode);

    // ============================ 开关控制 ============================
    void setSummaryEnabled(bool enable);
    bool isSummaryEnabled() const;
    void setDeviceLogsEnabled(bool enable);
    bool isDeviceLogsEnabled() const;

private:
    // ---- 日志成员 ----
    ILogger m_summaryLogger;
    ILogger m_deviceLogger;
    bool m_deviceLogsEnabled = true;
};

#endif // ALARM_DISPATCH_TASK_LOGGER_H
