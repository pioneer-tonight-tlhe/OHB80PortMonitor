/*******************************************************************************************
 * @file idlepurgeconfig.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class IdlePurgeConfig
 * @brief 按设备二维码管理 Idle Purge 配置项的读取与持久化写回。
 *
 * 设计目标：
 *      1. 按 QRCode 管理 ohb_device.ini 中各 OHB 设备段的 IdleP_ 配置项。
 *      2. 为界面层和调度任务层提供统一、稳定的 Idle Purge 配置访问入口。
 *      3. 将配置文件路径和持久化细节封装在配置类内部。
 *******************************************************************************************/
#ifndef IDLEPURGECONFIG_H
#define IDLEPURGECONFIG_H

#include <QString>

class IdlePurgeConfig
{
public:
    // ============================ 单例访问 ============================
    static IdlePurgeConfig& getInstance();

    // ============================ 配置读取 ============================
    bool isEnabled(const QString &qrCode) const;
    int getPurgeDurationSeconds(const QString &qrCode) const;
    int getPurgeIntervalSeconds(const QString &qrCode) const;

    // ============================ 配置写入 ============================
    bool setEnabled(const QString &qrCode, bool enabled);
    bool setPurgeDurationSeconds(const QString &qrCode, int seconds);
    bool setPurgeIntervalSeconds(const QString &qrCode, int seconds);

    // ============================ 配置路径 ============================
    QString getConfigPath() const;
private:
    // ============================ 构造与持久化 ============================
    IdlePurgeConfig();
    ~IdlePurgeConfig() = default;
    IdlePurgeConfig(const IdlePurgeConfig&) = delete;
    IdlePurgeConfig& operator=(const IdlePurgeConfig&) = delete;


private:
    // ---- 配置状态 ----
    QString m_configFilePath;
};

#endif // IDLEPURGECONFIG_H
