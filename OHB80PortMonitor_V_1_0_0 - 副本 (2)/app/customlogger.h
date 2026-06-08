#ifndef CUSTOMLOGGER_H
#define CUSTOMLOGGER_H

#include <QString>

// ============================================================
// CustomLogger - 用户自定义日志路径
//
// 提供供用户使用的 UI 日志目录路径
// ============================================================
class CustomLogger
{
public:
    CustomLogger() = delete;
    ~CustomLogger() = delete;
    CustomLogger(const CustomLogger&) = delete;
    CustomLogger& operator=(const CustomLogger&) = delete;

    // 获取固件升级日志目录路径: bin/log/firmware_upgrade
    static QString FirmwareUpgradeLoggerPath();

    // 获取固件升级截图目录路径: bin/log/firmware_upgrade/capture
    static QString FirmwareUpgradeCapturePath();

    // 获取通讯日志目录路径: bin/log/communication
    static QString CommunicationLoggerPath();

    // 获取警报日志目录路径: bin/log/alarms
    static QString AlarmLoggerPath();

    // 获取运行日志目录路径: bin/log/runningLog
    static QString RunningLoggerPath();
};

#endif // CUSTOMLOGGER_H
