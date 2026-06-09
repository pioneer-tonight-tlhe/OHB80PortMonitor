#ifndef VEFC_SENSOR_MONITOR_TASK_LOGGER_H
#define VEFC_SENSOR_MONITOR_TASK_LOGGER_H

#include "ilogger.h"

#include <QString>

// ====================================================================
// VEFCSensorMonitorTaskLogger - VEFC 监控任务专属日志管理器
//
// 设计目标：
//   1. 统一管理 summary.log 与 devices.log 两类日志对象。
//   2. 子设备日志统一写入 devices.log，不再按 QRCode 拆分文件。
//   3. 具体设备通过日志正文中的 [QRCode:xxxxx] 字段区分。
// ====================================================================
class VEFCSensorMonitorTaskLogger
{
public:
    // 按配置开关初始化任务汇总日志和设备明细日志。
    VEFCSensorMonitorTaskLogger(bool summaryEnable, bool devicesEnable);

    // 返回汇总日志对象，供日志服务输出任务/轮次级信息。
    ILogger& summaryLogger();

    void setSummaryEnabled(bool enable);
    bool isSummaryEnabled() const;

    void setDeviceLogsEnabled(bool enable);
    bool isDeviceLogsEnabled() const;

    // 返回设备日志对象；当前实现统一复用 devices.log。
    ILogger& deviceLogger(const QString& qrCode);

private:
    // ---- 日志对象与开关状态 ----
    ILogger m_summaryLogger;
    ILogger m_deviceLogger;
    bool m_deviceLogsEnabled = true;
};

#endif // VEFC_SENSOR_MONITOR_TASK_LOGGER_H
