#include "vefc_sensor_monitor_task_logger.h"

#include <QtGlobal>

// ====================================================================
// VEFCSensorMonitorTaskLogger - 日志管理器实现
//
// 说明：
//   1. 本类本身很轻，只负责封装两个 ILogger 对象及其开关状态。
//   2. 真正的日志文本格式由 VEFCSensorMonitorLogService 统一组织。
// ====================================================================
VEFCSensorMonitorTaskLogger::VEFCSensorMonitorTaskLogger(bool summaryEnable, bool devicesEnable)
    : m_summaryLogger("scheduler/vefc_sensor_monitor_task/summary")
    , m_deviceLogger("scheduler/vefc_sensor_monitor_task/devices")
    , m_deviceLogsEnabled(devicesEnable)
{
    m_summaryLogger.set_enable(summaryEnable);
    m_deviceLogger.set_enable(devicesEnable);
}

ILogger& VEFCSensorMonitorTaskLogger::summaryLogger()
{
    return m_summaryLogger;
}

void VEFCSensorMonitorTaskLogger::setSummaryEnabled(bool enable)
{
    m_summaryLogger.set_enable(enable);
}

bool VEFCSensorMonitorTaskLogger::isSummaryEnabled() const
{
    return m_summaryLogger.get_enable();
}

void VEFCSensorMonitorTaskLogger::setDeviceLogsEnabled(bool enable)
{
    m_deviceLogsEnabled = enable;
    m_deviceLogger.set_enable(enable);
}

bool VEFCSensorMonitorTaskLogger::isDeviceLogsEnabled() const
{
    return m_deviceLogsEnabled;
}

ILogger& VEFCSensorMonitorTaskLogger::deviceLogger(const QString& qrCode)
{
    Q_UNUSED(qrCode)
    return m_deviceLogger;
}
