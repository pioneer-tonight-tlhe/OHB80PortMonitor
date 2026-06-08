#ifndef FIRMWARECONFIG_H
#define FIRMWARECONFIG_H

#include <QString>

class FirmwareConfig
{
public:
    static FirmwareConfig& getInstance();

    // 读取固件配置
    int prepareCmdTimeoutMs() const;
    int waitingForEquipmentReadyMs() const;
    int sendIntervalForDataMs() const;
    int transferResponseTimeoutMs() const;
    int postTransferWaitMs() const;

    // 写入固件配置
    bool setPrepareCmdTimeoutMs(int ms);
    bool setWaitingForEquipmentReadyMs(int ms);
    bool setSendIntervalForDataMs(int ms);
    bool setTransferResponseTimeoutMs(int ms);
    bool setPostTransferWaitMs(int ms);

    // 获取配置文件路径
    QString getFirmwareConfigPath() const;

private:
    FirmwareConfig();
    ~FirmwareConfig() = default;
    FirmwareConfig(const FirmwareConfig&) = delete;
    FirmwareConfig& operator=(const FirmwareConfig&) = delete;

    void loadConfig();
    bool saveConfig();

private:
    QString m_configFilePath;
    int m_prepareCmdTimeoutMs;
    int m_waitingForEquipmentReadyMs;
    int m_sendIntervalForDataMs;
    int m_transferResponseTimeoutMs;
    int m_postTransferWaitMs;
};

#endif // FIRMWARECONFIG_H
