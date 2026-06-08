#include "customlogger.h"
#include "appconfig.h"
#include <QDir>

QString CustomLogger::FirmwareUpgradeLoggerPath()
{
    AppConfig &config = AppConfig::getInstance();
    QString logDir = config.getRootDir() + "/log/firmware_upgrade";

    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return logDir;
}

QString CustomLogger::FirmwareUpgradeCapturePath()
{
    AppConfig &config = AppConfig::getInstance();
    QString captureDir = config.getRootDir() + "/log/firmware_upgrade/capture";

    QDir dir(captureDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return captureDir;
}

QString CustomLogger::CommunicationLoggerPath()
{
    AppConfig &config = AppConfig::getInstance();
    QString logDir = config.getRootDir() + "/log/communicate_log";

    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return logDir;
}

QString CustomLogger::AlarmLoggerPath()
{
    AppConfig &config = AppConfig::getInstance();
    QString logDir = config.getRootDir() + "/log/alarms";

    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return logDir;
}

QString CustomLogger::RunningLoggerPath()
{
    AppConfig &config = AppConfig::getInstance();
    QString logDir = config.getRootDir() + "/log/runningLog";

    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    return logDir;
}
