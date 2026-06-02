#include "monitor_data_task_logger.h"

#include <QtGlobal>

MonitorDataTaskLogger::MonitorDataTaskLogger(bool summaryEnable, bool devicesEable)
    : m_summaryLogger("scheduler/monitor_data_task/summary")
    , m_deviceLogger("scheduler/monitor_data_task/devices")
    , m_deviceLogsEnabled(devicesEable)
{
    m_summaryLogger.set_enable(summaryEnable);
    m_deviceLogger.set_enable(devicesEable);
}

ILogger& MonitorDataTaskLogger::summaryLogger()
{
    return m_summaryLogger;
}

void MonitorDataTaskLogger::setSummaryEnabled(bool enable)
{
    m_summaryLogger.set_enable(enable);
}

bool MonitorDataTaskLogger::isSummaryEnabled() const
{
    return m_summaryLogger.get_enable();
}

void MonitorDataTaskLogger::setDeviceLogsEnabled(bool enable)
{
    m_deviceLogsEnabled = enable;
    m_deviceLogger.set_enable(enable);
}

bool MonitorDataTaskLogger::isDeviceLogsEnabled() const
{
    return m_deviceLogsEnabled;
}

ILogger& MonitorDataTaskLogger::deviceLogger(const QString& qrCode)
{
    Q_UNUSED(qrCode)
    return m_deviceLogger;
}
