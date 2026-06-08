#include "alarm_dispatch_task_logger.h"

#include <QtGlobal>

AlarmDispatchTaskLogger::AlarmDispatchTaskLogger(bool summaryEnable, bool devicesEnable)
    : m_summaryLogger("scheduler/alarm_dispatch_task/summary")
    , m_deviceLogger("scheduler/alarm_dispatch_task/devices")
    , m_deviceLogsEnabled(devicesEnable)
{
    m_summaryLogger.set_enable(summaryEnable);
    m_deviceLogger.set_enable(devicesEnable);
}

ILogger& AlarmDispatchTaskLogger::summaryLogger()
{
    return m_summaryLogger;
}

void AlarmDispatchTaskLogger::setSummaryEnabled(bool enable)
{
    m_summaryLogger.set_enable(enable);
}

bool AlarmDispatchTaskLogger::isSummaryEnabled() const
{
    return m_summaryLogger.get_enable();
}

void AlarmDispatchTaskLogger::setDeviceLogsEnabled(bool enable)
{
    m_deviceLogsEnabled = enable;
    m_deviceLogger.set_enable(enable);
}

bool AlarmDispatchTaskLogger::isDeviceLogsEnabled() const
{
    return m_deviceLogsEnabled;
}

ILogger& AlarmDispatchTaskLogger::deviceLogger(const QString& qrCode)
{
    Q_UNUSED(qrCode)
    return m_deviceLogger;
}
