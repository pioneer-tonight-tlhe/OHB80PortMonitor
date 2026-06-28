/*******************************************************************************************
 * @file ohbdeviceconfiginfo.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class OHBDeviceConfigInfo
 * @brief 保存单个 OHB 设备的配置数据。
 *
 * 设计目标：
 *      1. 统一描述 OHB 设备的二维码、网络地址和设备级配置参数。
 *      2. 为配置层与业务层提供共享的轻量配置对象。
 *      3. 将设备配置数据与配置文件读写逻辑解耦。
 *******************************************************************************************/
#ifndef OHBDEVICECONFIGINFO_H
#define OHBDEVICECONFIGINFO_H

#include <QString>
#include <QtGlobal>

class OHBDeviceConfigInfo
{
public:
    // ============================ 构造函数 ============================
    OHBDeviceConfigInfo();
    OHBDeviceConfigInfo(const QString& qrCode,
                        const QString& ip,
                        quint16 port,
                        bool enabled = true,
                        int purgeFlowLitersPerMinute = 35,
                        int logoTimeSeconds = 5,
                        int pageSwitchIntervalSeconds = 5,
                        int pageTotalTimeSeconds = 5,
                        double humidityOffsetPercent = 0.0,
                        double humidityLowerLimitPercent = 5.0,
                        double vppePressureBar = 3.0,
                        int foupInAutoPurgeEnable = 0);
    
    // ============================ 业务功能 ============================
    // 获取设备二维码。
    QString getQrCode() const;

    // 设置设备二维码。
    void setQrCode(const QString& qrCode);

    // 获取设备 IP 地址。
    QString getIp() const;

    // 设置设备 IP 地址。
    void setIp(const QString& ip);

    // 获取设备端口号。
    quint16 getPort() const;

    // 设置设备端口号。
    void setPort(quint16 port);

    // 获取设备启用状态。
    bool isEnabled() const;

    // 设置设备启用状态。
    void setEnabled(bool enabled);

    // 获取充气流量。
    int getPurgeFlowLitersPerMinute() const;

    // 设置充气流量。
    void setPurgeFlowLitersPerMinute(int purgeFlowLitersPerMinute);

    // 获取 Logo 显示时长。
    int getLogoTimeSeconds() const;

    // 设置 Logo 显示时长。
    void setLogoTimeSeconds(int logoTimeSeconds);

    // 获取页面切换间隔。
    int getPageSwitchIntervalSeconds() const;

    // 设置页面切换间隔。
    void setPageSwitchIntervalSeconds(int pageSwitchIntervalSeconds);

    // 获取页面总显示时长。
    int getPageTotalTimeSeconds() const;

    // 设置页面总显示时长。
    void setPageTotalTimeSeconds(int pageTotalTimeSeconds);

    // 获取湿度偏移量。
    double getHumidityOffsetPercent() const;

    // 设置湿度偏移量。
    void setHumidityOffsetPercent(double humidityOffsetPercent);

    // 获取湿度下限值。
    double getHumidityLowerLimitPercent() const;

    // 设置湿度下限值。
    void setHumidityLowerLimitPercent(double humidityLowerLimitPercent);

    // 获取 VPPE 比例阀压力。
    double getVppePressureBar() const;

    // 设置 VPPE 比例阀压力。
    void setVppePressureBar(double vppePressureBar);

    // 获取 FOUPIN 自动充气使能。
    int getFoupInAutoPurgeEnable() const;

    // 设置 FOUPIN 自动充气使能。
    void setFoupInAutoPurgeEnable(int enable);

private:
    // ---- 状态成员 ----
    QString m_qrCode;                      // 设备二维码。
    QString m_ip;                          // 设备 IP 地址。
    quint16 m_port;                        // 设备端口号。
    bool m_enabled;                        // 用于初始化 ConfigPage 的设备启用状态。
    int m_purgeFlowLitersPerMinute;        // 用于初始化 ConfigPage 的充气流量，单位 L/Min。
    double m_vppePressureBar;              // 用于初始化 ConfigPage 的 VPPE 比例阀压力，单位 bar。
    int m_logoTimeSeconds;                 // 用于初始化 DebugPage 的 Logo 显示时长，单位 s。
    int m_pageSwitchIntervalSeconds;       // 用于初始化 DebugPage 的页面切换间隔，单位 s。
    int m_pageTotalTimeSeconds;            // 用于初始化 DebugPage 的页面总显示时长，单位 s。
    double m_humidityOffsetPercent;        // 用于初始化 DebugPage 的湿度偏移量，单位 %。
    double m_humidityLowerLimitPercent;    // 用于初始化 DebugPage 的湿度下限值，单位 %。
    int m_foupInAutoPurgeEnable;           // FOUPIN 自动充气使能，0=默认关闭，1=开启。
};

#endif // OHBDEVICECONFIGINFO_H
