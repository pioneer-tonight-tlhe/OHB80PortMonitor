#include "ohbdeviceconfiginfo.h"

OHBDeviceConfigInfo::OHBDeviceConfigInfo()
    : m_port(0)
    , m_enabled(true)
    , m_purgeFlowLitersPerMinute(35)
    , m_vppePressureBar(3.0)
    , m_logoTimeSeconds(5)
    , m_pageSwitchIntervalSeconds(5)
    , m_pageTotalTimeSeconds(5)
    , m_humidityOffsetPercent(0.0)
    , m_humidityLowerLimitPercent(5.0)
{
}

OHBDeviceConfigInfo::OHBDeviceConfigInfo(const QString& qrCode,
                                         const QString& ip,
                                         quint16 port,
                                         bool enabled,
                                         int purgeFlowLitersPerMinute,
                                         int logoTimeSeconds,
                                         int pageSwitchIntervalSeconds,
                                         int pageTotalTimeSeconds,
                                         double humidityOffsetPercent,
                                         double humidityLowerLimitPercent,
                                         double vppePressureBar)
    : m_qrCode(qrCode)
    , m_ip(ip)
    , m_port(port)
    , m_enabled(enabled)
    , m_purgeFlowLitersPerMinute(purgeFlowLitersPerMinute)
    , m_vppePressureBar(vppePressureBar)
    , m_logoTimeSeconds(logoTimeSeconds)
    , m_pageSwitchIntervalSeconds(pageSwitchIntervalSeconds)
    , m_pageTotalTimeSeconds(pageTotalTimeSeconds)
    , m_humidityOffsetPercent(humidityOffsetPercent)
    , m_humidityLowerLimitPercent(humidityLowerLimitPercent)
{
}

QString OHBDeviceConfigInfo::getQrCode() const
{
    return m_qrCode;
}

void OHBDeviceConfigInfo::setQrCode(const QString& qrCode)
{
    m_qrCode = qrCode;
}

QString OHBDeviceConfigInfo::getIp() const
{
    return m_ip;
}

void OHBDeviceConfigInfo::setIp(const QString& ip)
{
    m_ip = ip;
}

quint16 OHBDeviceConfigInfo::getPort() const
{
    return m_port;
}

void OHBDeviceConfigInfo::setPort(quint16 port)
{
    m_port = port;
}

bool OHBDeviceConfigInfo::isEnabled() const
{
    return m_enabled;
}

void OHBDeviceConfigInfo::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

int OHBDeviceConfigInfo::getPurgeFlowLitersPerMinute() const
{
    return m_purgeFlowLitersPerMinute;
}

void OHBDeviceConfigInfo::setPurgeFlowLitersPerMinute(int purgeFlowLitersPerMinute)
{
    m_purgeFlowLitersPerMinute = purgeFlowLitersPerMinute;
}

int OHBDeviceConfigInfo::getLogoTimeSeconds() const
{
    return m_logoTimeSeconds;
}

void OHBDeviceConfigInfo::setLogoTimeSeconds(int logoTimeSeconds)
{
    m_logoTimeSeconds = logoTimeSeconds;
}

int OHBDeviceConfigInfo::getPageSwitchIntervalSeconds() const
{
    return m_pageSwitchIntervalSeconds;
}

void OHBDeviceConfigInfo::setPageSwitchIntervalSeconds(int pageSwitchIntervalSeconds)
{
    m_pageSwitchIntervalSeconds = pageSwitchIntervalSeconds;
}

int OHBDeviceConfigInfo::getPageTotalTimeSeconds() const
{
    return m_pageTotalTimeSeconds;
}

void OHBDeviceConfigInfo::setPageTotalTimeSeconds(int pageTotalTimeSeconds)
{
    m_pageTotalTimeSeconds = pageTotalTimeSeconds;
}

double OHBDeviceConfigInfo::getHumidityOffsetPercent() const
{
    return m_humidityOffsetPercent;
}

void OHBDeviceConfigInfo::setHumidityOffsetPercent(double humidityOffsetPercent)
{
    m_humidityOffsetPercent = humidityOffsetPercent;
}

double OHBDeviceConfigInfo::getHumidityLowerLimitPercent() const
{
    return m_humidityLowerLimitPercent;
}

void OHBDeviceConfigInfo::setHumidityLowerLimitPercent(double humidityLowerLimitPercent)
{
    m_humidityLowerLimitPercent = humidityLowerLimitPercent;
}

double OHBDeviceConfigInfo::getVppePressureBar() const
{
    return m_vppePressureBar;
}

void OHBDeviceConfigInfo::setVppePressureBar(double vppePressureBar)
{
    m_vppePressureBar = vppePressureBar;
}
