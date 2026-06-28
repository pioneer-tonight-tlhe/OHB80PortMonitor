/*******************************************************************************************
 * @file idlepurgeconfig.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class IdlePurgeConfig
 * @brief 管理 Idle Purge 配置项的读取、缓存与持久化写回。
 *
 * 设计目标：
 *      1. 独立管理 ohb_device.ini 中 [IdleConfig] 配置段的默认值、读取与写回逻辑。
 *      2. 为界面层和调度任务层提供统一、稳定的 Idle Purge 配置访问入口。
 *      3. 将配置文件路径、内存缓存和持久化细节封装在配置类内部。
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
    bool isEnabled() const;
    int getPurgeDurationSeconds() const;
    int getPurgeIntervalSeconds() const;

    // ============================ 配置写入 ============================
    bool setEnabled(bool enabled);
    bool setPurgeDurationSeconds(int seconds);
    bool setPurgeIntervalSeconds(int seconds);

    // ============================ 配置路径 ============================
    QString getConfigPath() const;

private:
    // ============================ 构造与持久化 ============================
    IdlePurgeConfig();
    ~IdlePurgeConfig() = default;
    IdlePurgeConfig(const IdlePurgeConfig&) = delete;
    IdlePurgeConfig& operator=(const IdlePurgeConfig&) = delete;

    void loadConfig();
    bool saveConfig();

private:
    // ---- 配置状态 ----
    QString m_configFilePath;
    bool m_enabled;
    int m_purgeDurationSeconds;
    int m_purgeIntervalSeconds;
};

#endif // IDLEPURGECONFIG_H
