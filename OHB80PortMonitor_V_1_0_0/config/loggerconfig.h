#ifndef LOGGERCONFIG_H
#define LOGGERCONFIG_H

#include <QString>
#include "../singleton.h"

// 【调度任务】日志配置
struct SchedulerLoggerConfig
{
    bool monitorDataTaskSummary = true;
    bool monitorDataTaskDevices = true;
    bool alarmDispatchTaskSummary = true;
    bool alarmDispatchTaskDevices = true;
    bool vefcSensorMonitorTaskSummary = true;
    bool vefcSensorMonitorTaskDevices = true;
    int  alarmDispatchTaskSummaryPeriodMs = 60000; // AlarmDispatchTask 汇总输出周期（毫秒）

    SchedulerLoggerConfig() = default;
};

class LoggerConfig : public Singleton<LoggerConfig>
{
    friend class Singleton<LoggerConfig>;

public:
    // 读取日志配置
    SchedulerLoggerConfig readSchedulerLoggerConfig() const;

    // 写入日志配置
    bool writeSchedulerLoggerConfig(const SchedulerLoggerConfig& config);

    // 获取配置文件路径
    QString getLoggerConfigPath() const;

    // ========== MonitorDataTask 任务日志 ==========
    // 获取 MonitorDataTask 汇总日志开关
    bool isMonitorDataTaskSummaryEnabled() const;

    // 获取 MonitorDataTask 设备日志开关
    bool isMonitorDataTaskDevicesEnabled() const;

    // 设置 MonitorDataTask 汇总日志开关
    bool setMonitorDataTaskSummaryEnabled(bool enabled);

    // 设置 MonitorDataTask 设备日志开关
    bool setMonitorDataTaskDevicesEnabled(bool enabled);

    // ========== AlarmDispatchTask 任务日志 ==========
    // 获取 AlarmDispatchTask 汇总日志开关
    bool isAlarmDispatchTaskSummaryEnabled() const;

    // 获取 AlarmDispatchTask 设备日志开关
    bool isAlarmDispatchTaskDevicesEnabled() const;

    // 设置 AlarmDispatchTask 汇总日志开关
    bool setAlarmDispatchTaskSummaryEnabled(bool enabled);

    // 设置 AlarmDispatchTask 设备日志开关
    bool setAlarmDispatchTaskDevicesEnabled(bool enabled);

    // 获取 AlarmDispatchTask 汇总日志输出周期（毫秒）
    int getAlarmDispatchTaskSummaryPeriodMs() const;

    // ========== VEFCSensorMonitorTask 任务日志 ==========
    // 获取 VEFCSensorMonitorTask 汇总日志开关
    bool isVEFCSensorMonitorTaskSummaryEnabled() const;

    // 获取 VEFCSensorMonitorTask 设备日志开关
    bool isVEFCSensorMonitorTaskDevicesEnabled() const;

    // 设置 VEFCSensorMonitorTask 汇总日志开关
    bool setVEFCSensorMonitorTaskSummaryEnabled(bool enabled);

    // 设置 VEFCSensorMonitorTask 设备日志开关
    bool setVEFCSensorMonitorTaskDevicesEnabled(bool enabled);

private:
    LoggerConfig();

public:
    ~LoggerConfig() = default;

private:
    QString m_configFilePath;
};

#endif // LOGGERCONFIG_H
