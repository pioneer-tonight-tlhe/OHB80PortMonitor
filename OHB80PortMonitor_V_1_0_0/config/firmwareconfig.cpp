#include "firmwareconfig.h"
#include "appconfig.h"
#include "applogger.h"
#include "loggermanager.h"

#include <QSettings>
#include <QDir>
#include <QDebug>

FirmwareConfig& FirmwareConfig::getInstance()
{
    static FirmwareConfig instance;
    return instance;
}

void FirmwareConfig::reloadConfig()
{
    loadConfig();
}

FirmwareConfig::FirmwareConfig()
    : m_prepareCmdTimeoutMs(1000)
    , m_waitingForEquipmentReadyMs(1000)
    , m_sendIntervalForDataMs(100)
    , m_transferResponseTimeoutMs(3000)
    , m_postTransferWaitMs(2000)
{
    QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/firmware.ini";
    
    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }
    
    loadConfig();
    
    qDebug() << "[app][FirmwareConfig][FirmwareConfig]：固件配置管理器已创建";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[app][FirmwareConfig][FirmwareConfig]：固件配置管理器已创建");
}

QString FirmwareConfig::getFirmwareConfigPath() const
{
    return m_configFilePath;
}

void FirmwareConfig::loadConfig()
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    
    settings.beginGroup("Firmware");
    m_prepareCmdTimeoutMs = settings.value("PrepareCmdTimeoutTimeMs", 1000).toInt();
    m_waitingForEquipmentReadyMs = settings.value("WaitingForEquipmentReadyTimeMs", 1000).toInt();
    m_sendIntervalForDataMs = settings.value("SendIntervalForDataTimeMs", 100).toInt();
    m_transferResponseTimeoutMs = settings.value("TransferResponseTimeoutTimeMs", 3000).toInt();
    m_postTransferWaitMs = settings.value("PostTransferWaitTimeMs", 2000).toInt();
    settings.endGroup();
    
    qDebug() << "[app][FirmwareConfig][loadConfig]：加载固件配置 -"
             << "PrepareCmdTimeout:" << m_prepareCmdTimeoutMs << "ms,"
             << "WaitingTime:" << m_waitingForEquipmentReadyMs << "ms,"
             << "SendInterval:" << m_sendIntervalForDataMs << "ms,"
             << "TransferTimeout:" << m_transferResponseTimeoutMs << "ms";
    
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[app][FirmwareConfig][loadConfig]：加载固件配置 - PrepareCmdTimeout: %1ms, WaitingTime: %2ms, SendInterval: %3ms, TransferTimeout: %4ms")
            .arg(m_prepareCmdTimeoutMs).arg(m_waitingForEquipmentReadyMs).arg(m_sendIntervalForDataMs).arg(m_transferResponseTimeoutMs).toStdString());
}

bool FirmwareConfig::saveConfig()
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    
    settings.beginGroup("Firmware");
    settings.setValue("PrepareCmdTimeoutTimeMs", m_prepareCmdTimeoutMs);
    settings.setValue("WaitingForEquipmentReadyTimeMs", m_waitingForEquipmentReadyMs);
    settings.setValue("SendIntervalForDataTimeMs", m_sendIntervalForDataMs);
    settings.setValue("TransferResponseTimeoutTimeMs", m_transferResponseTimeoutMs);
    settings.setValue("PostTransferWaitTimeMs", m_postTransferWaitMs);
    settings.endGroup();
    
    settings.sync();
    
    bool success = settings.status() == QSettings::NoError;
    
    qDebug() << "[app][FirmwareConfig][saveConfig]：保存固件配置" << (success ? "成功" : "失败");
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), success ? Level::INFO : Level::WARN,
        QString("[app][FirmwareConfig][saveConfig]：保存固件配置%1").arg(success ? "成功" : "失败").toStdString());
    
    return success;
}

int FirmwareConfig::prepareCmdTimeoutMs() const
{
    return m_prepareCmdTimeoutMs;
}

int FirmwareConfig::waitingForEquipmentReadyMs() const
{
    return m_waitingForEquipmentReadyMs;
}

int FirmwareConfig::sendIntervalForDataMs() const
{
    return m_sendIntervalForDataMs;
}

int FirmwareConfig::transferResponseTimeoutMs() const
{
    return m_transferResponseTimeoutMs;
}

int FirmwareConfig::postTransferWaitMs() const
{
    return m_postTransferWaitMs;
}

bool FirmwareConfig::setPrepareCmdTimeoutMs(int ms)
{
    m_prepareCmdTimeoutMs = ms;
    qDebug() << "[app][FirmwareConfig][setPrepareCmdTimeoutMs]：设置准备指令超时为" << ms << "ms";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[app][FirmwareConfig][setPrepareCmdTimeoutMs]：设置准备指令超时为 %1ms").arg(ms).toStdString());
    return saveConfig();
}

bool FirmwareConfig::setWaitingForEquipmentReadyMs(int ms)
{
    m_waitingForEquipmentReadyMs = ms;
    qDebug() << "[app][FirmwareConfig][setWaitingForEquipmentReadyMs]：设置等待设备就绪时间为" << ms << "ms";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[app][FirmwareConfig][setWaitingForEquipmentReadyMs]：设置等待设备就绪时间为 %1ms").arg(ms).toStdString());
    return saveConfig();
}

bool FirmwareConfig::setSendIntervalForDataMs(int ms)
{
    m_sendIntervalForDataMs = ms;
    qDebug() << "[app][FirmwareConfig][setSendIntervalForDataMs]：设置发送间隔为" << ms << "ms";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[app][FirmwareConfig][setSendIntervalForDataMs]：设置发送间隔为 %1ms").arg(ms).toStdString());
    return saveConfig();
}

bool FirmwareConfig::setTransferResponseTimeoutMs(int ms)
{
    m_transferResponseTimeoutMs = ms;
    qDebug() << "[app][FirmwareConfig][setTransferResponseTimeoutMs]：设置传输响应超时为" << ms << "ms";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[app][FirmwareConfig][setTransferResponseTimeoutMs]：设置传输响应超时为 %1ms").arg(ms).toStdString());
    return saveConfig();
}

bool FirmwareConfig::setPostTransferWaitMs(int ms)
{
    m_postTransferWaitMs = ms;
    qDebug() << "[app][FirmwareConfig][setPostTransferWaitMs]：设置数据传输后等待时间为" << ms << "ms";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[app][FirmwareConfig][setPostTransferWaitMs]：设置数据传输后等待时间为 %1ms").arg(ms).toStdString());
    return saveConfig();
}
