/*******************************************************************************************
 * @file ohbdeviceconfig.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class OHBDeviceConfig
 * @brief 管理 OHB 设备清单及设备级参数配置的读取与持久化写回。
 *
 * 设计目标：
 *      1. 统一管理 ohb_device.ini 中主设备列表、网络信息和设备级参数配置。
 *      2. 为界面层和调度层提供稳定的设备配置读取、查询与写回入口。
 *      3. 兼容历史配置键名，降低配置项命名调整带来的迁移成本。
 *******************************************************************************************/
#ifndef OHBDEVICECONFIG_H
#define OHBDEVICECONFIG_H

#include "ohbdeviceconfiginfo.h"

#include <QString>
#include <QVector>
#include <QtGlobal>

class OHBDeviceConfig
{
public:
    // ============================ 单例访问 ============================
    static OHBDeviceConfig& getInstance();

    // ============================ 设备配置读写 ============================
    QVector<OHBDeviceConfigInfo> readDevices() const;
    bool writeDevices(const QVector<OHBDeviceConfigInfo>& devices);
    QVector<QString> readQRCodes() const;
    QVector<QString> readMasterDevices() const;
    bool writeMasterDevices(const QVector<QString>& masterDevices);
    OHBDeviceConfigInfo getDeviceByQRCode(const QString& qrCode) const;
    OHBDeviceConfigInfo getDeviceByMasterId(const QString& masterId) const;
    bool setDeviceEnable(const QString& qrCode, bool enable);
    bool setVppePressureBarByQRCode(const QString& qrCode, double vppePressureBar);
    bool setFoupInAutoPurgeEnableByQRCode(const QString& qrCode, int enable);
    bool updateDeviceInfoByQRCode(const QString& oldQrCode,
                                  const QString& newQrCode,
                                  const QString& ip,
                                  quint16 port);
    QString getConfigPath() const;

    // ============================ 任务配置读写 ============================
    // 读取 SH85 周期自检任务的启用状态。
    bool readSH85SelfCheckEnabled() const;

    // 读取 SH85 周期自检任务的执行周期秒数。
    int readSH85SelfCheckPeriodSeconds() const;

    // 持久化 SH85 周期自检任务的启用状态。
    bool setSH85SelfCheckEnabled(bool enabled);

    // 持久化 SH85 周期自检任务的执行周期秒数。
    bool setSH85SelfCheckPeriodSeconds(int seconds);

private:
    // ============================ 构造函数 ============================
    OHBDeviceConfig();
    ~OHBDeviceConfig() = default;
    OHBDeviceConfig(const OHBDeviceConfig&) = delete;
    OHBDeviceConfig& operator=(const OHBDeviceConfig&) = delete;

private:
    // ---- 状态成员 ----
    QString m_configFilePath;
};

#endif // OHBDEVICECONFIG_H
