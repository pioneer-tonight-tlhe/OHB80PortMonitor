#ifndef NETWORK_STATUS_TASK_LOGGER_H
#define NETWORK_STATUS_TASK_LOGGER_H

#include "ilogger.h"

#include <QString>

class NetworkStatusTaskLogger
{
public:
    NetworkStatusTaskLogger(bool summaryEnable = true, bool devicesEnable = true);

    ILogger& summaryLogger();
    ILogger& deviceLogger(const QString& deviceId);

    void setSummaryEnabled(bool enable);
    bool isSummaryEnabled() const;

    void setDeviceLogsEnabled(bool enable);
    bool isDeviceLogsEnabled() const;

    void summaryInfo(const QString& action, const QString& message);
    void summaryWarn(const QString& action, const QString& message);
    void summaryError(const QString& action, const QString& message);

    void deviceInfo(const QString& deviceId, const QString& action, const QString& message);
    void deviceWarn(const QString& deviceId, const QString& action, const QString& message);
    void deviceError(const QString& deviceId, const QString& action, const QString& message);

private:
    static QString normalizedDeviceId(const QString& deviceId);
    static QString summaryMessage(const QString& action, const QString& message);
    static QString deviceMessage(const QString& deviceId, const QString& action, const QString& message);

    ILogger m_summaryLogger;
    ILogger m_deviceLogger;
    bool m_deviceLogsEnabled = true;
};

#endif // NETWORK_STATUS_TASK_LOGGER_H
