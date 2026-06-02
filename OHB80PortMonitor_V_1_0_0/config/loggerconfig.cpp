#include "loggerconfig.h"
#include "appconfig.h"
#include <QSettings>
#include <QDir>

LoggerConfig::LoggerConfig()
{
    QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/logger_config.ini";

    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }
}

QString LoggerConfig::getLoggerConfigPath() const
{
    return m_configFilePath;
}

SchedulerLoggerConfig LoggerConfig::readSchedulerLoggerConfig() const
{
    SchedulerLoggerConfig config;

    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("scheduler");
    config.monitorDataTaskSummary = settings.value("monitor_data_task.summary", true).toBool();
    config.monitorDataTaskDevices = settings.value("monitor_data_task.devices", true).toBool();
    config.alarmDispatchTaskSummary = settings.value("alarm_dispatch_task.summary", true).toBool();
    config.alarmDispatchTaskDevices = settings.value("alarm_dispatch_task.devices", true).toBool();
    config.vefcSensorMonitorTaskSummary = settings.value("vefc_sensor_monitor_task.summary", true).toBool();
    config.vefcSensorMonitorTaskDevices = settings.value("vefc_sensor_monitor_task.devices", true).toBool();
    settings.endGroup();

    return config;
}

bool LoggerConfig::writeSchedulerLoggerConfig(const SchedulerLoggerConfig& config)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("scheduler");
    settings.setValue("monitor_data_task.summary", config.monitorDataTaskSummary);
    settings.setValue("monitor_data_task.devices", config.monitorDataTaskDevices);
    settings.setValue("alarm_dispatch_task.summary", config.alarmDispatchTaskSummary);
    settings.setValue("alarm_dispatch_task.devices", config.alarmDispatchTaskDevices);
    settings.setValue("vefc_sensor_monitor_task.summary", config.vefcSensorMonitorTaskSummary);
    settings.setValue("vefc_sensor_monitor_task.devices", config.vefcSensorMonitorTaskDevices);
    settings.endGroup();

    settings.sync();

    return settings.status() == QSettings::NoError;
}

bool LoggerConfig::isMonitorDataTaskSummaryEnabled() const
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    return config.monitorDataTaskSummary;
}

bool LoggerConfig::isMonitorDataTaskDevicesEnabled() const
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    return config.monitorDataTaskDevices;
}

bool LoggerConfig::setMonitorDataTaskSummaryEnabled(bool enabled)
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    config.monitorDataTaskSummary = enabled;
    return writeSchedulerLoggerConfig(config);
}

bool LoggerConfig::setMonitorDataTaskDevicesEnabled(bool enabled)
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    config.monitorDataTaskDevices = enabled;
    return writeSchedulerLoggerConfig(config);
}

bool LoggerConfig::isAlarmDispatchTaskSummaryEnabled() const
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    return config.alarmDispatchTaskSummary;
}

bool LoggerConfig::isAlarmDispatchTaskDevicesEnabled() const
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    return config.alarmDispatchTaskDevices;
}

bool LoggerConfig::setAlarmDispatchTaskSummaryEnabled(bool enabled)
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    config.alarmDispatchTaskSummary = enabled;
    return writeSchedulerLoggerConfig(config);
}

bool LoggerConfig::setAlarmDispatchTaskDevicesEnabled(bool enabled)
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    config.alarmDispatchTaskDevices = enabled;
    return writeSchedulerLoggerConfig(config);
}

bool LoggerConfig::isVEFCSensorMonitorTaskSummaryEnabled() const
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    return config.vefcSensorMonitorTaskSummary;
}

bool LoggerConfig::isVEFCSensorMonitorTaskDevicesEnabled() const
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    return config.vefcSensorMonitorTaskDevices;
}

bool LoggerConfig::setVEFCSensorMonitorTaskSummaryEnabled(bool enabled)
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    config.vefcSensorMonitorTaskSummary = enabled;
    return writeSchedulerLoggerConfig(config);
}

bool LoggerConfig::setVEFCSensorMonitorTaskDevicesEnabled(bool enabled)
{
    SchedulerLoggerConfig config = readSchedulerLoggerConfig();
    config.vefcSensorMonitorTaskDevices = enabled;
    return writeSchedulerLoggerConfig(config);
}
