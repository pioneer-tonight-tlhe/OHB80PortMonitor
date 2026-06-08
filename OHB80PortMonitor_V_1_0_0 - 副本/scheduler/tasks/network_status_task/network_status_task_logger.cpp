#include "network_status_task_logger.h"

#include <QtGlobal>

NetworkStatusTaskLogger::NetworkStatusTaskLogger(bool summaryEnable, bool devicesEnable)
    : m_summaryLogger("scheduler/network_status_task/summary/summary")
    , m_deviceLogger("scheduler/network_status_task/devices/devices")
    , m_deviceLogsEnabled(devicesEnable)
{
    m_summaryLogger.set_enable(summaryEnable);
    m_deviceLogger.set_enable(devicesEnable);
}

ILogger& NetworkStatusTaskLogger::summaryLogger()
{
    return m_summaryLogger;
}

ILogger& NetworkStatusTaskLogger::deviceLogger(const QString& deviceId)
{
    Q_UNUSED(deviceId)
    return m_deviceLogger;
}

void NetworkStatusTaskLogger::setSummaryEnabled(bool enable)
{
    m_summaryLogger.set_enable(enable);
}

bool NetworkStatusTaskLogger::isSummaryEnabled() const
{
    return m_summaryLogger.get_enable();
}

void NetworkStatusTaskLogger::setDeviceLogsEnabled(bool enable)
{
    m_deviceLogsEnabled = enable;
    m_deviceLogger.set_enable(enable);
}

bool NetworkStatusTaskLogger::isDeviceLogsEnabled() const
{
    return m_deviceLogsEnabled;
}

void NetworkStatusTaskLogger::summaryInfo(const QString& action, const QString& message)
{
    summaryLogger().info(summaryMessage(action, message).toStdString());
}

void NetworkStatusTaskLogger::summaryWarn(const QString& action, const QString& message)
{
    summaryLogger().warn(summaryMessage(action, message).toStdString());
}

void NetworkStatusTaskLogger::summaryError(const QString& action, const QString& message)
{
    summaryLogger().error(summaryMessage(action, message).toStdString());
}

void NetworkStatusTaskLogger::deviceInfo(const QString& deviceId, const QString& action, const QString& message)
{
    deviceLogger(deviceId).info(deviceMessage(deviceId, action, message).toStdString());
}

void NetworkStatusTaskLogger::deviceWarn(const QString& deviceId, const QString& action, const QString& message)
{
    deviceLogger(deviceId).warn(deviceMessage(deviceId, action, message).toStdString());
}

void NetworkStatusTaskLogger::deviceError(const QString& deviceId, const QString& action, const QString& message)
{
    deviceLogger(deviceId).error(deviceMessage(deviceId, action, message).toStdString());
}

QString NetworkStatusTaskLogger::normalizedDeviceId(const QString& deviceId)
{
    const QString trimmed = deviceId.trimmed();
    return trimmed.isEmpty() ? QStringLiteral("unknown") : trimmed;
}

QString NetworkStatusTaskLogger::summaryMessage(const QString& action, const QString& message)
{
    return QString("[scheduler][NetworkStatusTask][Summary][%1]：%2").arg(action, message);
}

QString NetworkStatusTaskLogger::deviceMessage(const QString& deviceId, const QString& action, const QString& message)
{
    return QString("[scheduler][NetworkStatusTask][Device][%1]：设备ID=%2 %3")
        .arg(action, normalizedDeviceId(deviceId), message);
}
