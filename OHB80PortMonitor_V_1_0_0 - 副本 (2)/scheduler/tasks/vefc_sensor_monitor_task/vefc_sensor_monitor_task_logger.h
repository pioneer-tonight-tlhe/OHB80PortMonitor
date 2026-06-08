#ifndef VEFC_SENSOR_MONITOR_TASK_LOGGER_H
#define VEFC_SENSOR_MONITOR_TASK_LOGGER_H

#include "ilogger.h"

#include <QString>

/**
 * @brief VEFCSensorMonitorTask 专属日志管理类。
 *
 * 日志文件约定：
 * - summary：scheduler/vefc_sensor_monitor_task/summary.log，记录任务启动、轮次开始/结束、失败摘要。
 * - devices：scheduler/vefc_sensor_monitor_task/devices.log，记录所有子设备的指令帧和记录生成明细。
 *
 * 子设备日志统一写入 devices.log，不再按 QRCode 拆分文件。
 * 具体设备通过日志正文中的 [QRCode:xxxxx] 字段区分。
 */
class VEFCSensorMonitorTaskLogger
{
public:
    VEFCSensorMonitorTaskLogger(bool summaryEnable, bool devicesEnable);

    ILogger& summaryLogger();

    void setSummaryEnabled(bool enable);
    bool isSummaryEnabled() const;

    void setDeviceLogsEnabled(bool enable);
    bool isDeviceLogsEnabled() const;

    ILogger& deviceLogger(const QString& qrCode);

private:
    ILogger m_summaryLogger;
    ILogger m_deviceLogger;
    bool m_deviceLogsEnabled = true;
};

#endif // VEFC_SENSOR_MONITOR_TASK_LOGGER_H
