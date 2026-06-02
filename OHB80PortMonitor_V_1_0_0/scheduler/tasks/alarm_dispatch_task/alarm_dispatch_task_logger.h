#ifndef ALARM_DISPATCH_TASK_LOGGER_H
#define ALARM_DISPATCH_TASK_LOGGER_H

#include "ilogger.h"

#include <QString>

/**
 * @brief AlarmDispatchTask 专属日志管理类。
 *
 * 统一管理告警调度任务中的日志对象，避免任务流程代码直接维护 logger。
 *
 * 日志文件约定：
 * - summary：scheduler/alarm_dispatch_task/summary.log
 * - devices：scheduler/alarm_dispatch_task/devices.log
 *
 * 子设备日志统一写入 devices.log，不再按 QRCode 拆分文件。
 * 具体设备通过日志正文中的“设备标识”或 QRCode 字段区分。
 */
class AlarmDispatchTaskLogger
{
public:
    AlarmDispatchTaskLogger(bool summaryEnable, bool devicesEnable);

    // 获取 summary 日志对象，用于记录任务生命周期和汇总信息。
    ILogger& summaryLogger();

    // 启用或禁用 summary 日志写入。
    void setSummaryEnabled(bool enable);
    bool isSummaryEnabled() const;

    // 启用或禁用子设备日志写入。
    void setDeviceLogsEnabled(bool enable);
    bool isDeviceLogsEnabled() const;

    // 获取子设备日志对象；qrCode 只用于保持调用语义，实际统一写入 devices.log。
    ILogger& deviceLogger(const QString& qrCode);

private:
    ILogger m_summaryLogger;
    ILogger m_deviceLogger;
    bool m_deviceLogsEnabled = true;
};

#endif // ALARM_DISPATCH_TASK_LOGGER_H
