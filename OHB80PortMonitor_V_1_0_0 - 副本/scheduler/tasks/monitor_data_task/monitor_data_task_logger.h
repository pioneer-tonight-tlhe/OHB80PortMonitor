#ifndef MONITOR_DATA_TASK_LOGGER_H
#define MONITOR_DATA_TASK_LOGGER_H

#include "ilogger.h"

#include <QString>

/**
 * @brief MonitorDataTask 专属日志管理类。
 *
 * 统一管理监控数据任务中的日志对象，避免任务流程代码直接维护多个 ILogger。
 *
 * 日志文件约定：
 * - summary：scheduler/monitor_data_task/summary.log
 * - 子设备：scheduler/monitor_data_task/<QRCode>.log
 */
class MonitorDataTaskLogger
{
public:
    MonitorDataTaskLogger(bool summaryEnable, bool devicesEable);

    // 获取 summary 日志对象，用于记录任务生命周期和汇总信息。
    ILogger& summaryLogger();

    // 启用或禁用 summary 日志写入。
    void setSummaryEnabled(bool enable);
    bool isSummaryEnabled() const;

    // 启用或禁用全部子设备日志写入；已创建和后续创建的设备日志都会同步该状态。
    void setDeviceLogsEnabled(bool enable);
    bool isDeviceLogsEnabled() const;

    // 根据 QRCode 获取子设备日志对象；不存在时自动创建。
    ILogger& deviceLogger(const QString& qrCode);

private:
    ILogger m_summaryLogger;
    ILogger m_deviceLogger;
    bool m_deviceLogsEnabled = true;
};

#endif // MONITOR_DATA_TASK_LOGGER_H
