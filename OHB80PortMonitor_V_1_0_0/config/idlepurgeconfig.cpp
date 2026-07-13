#include "idlepurgeconfig.h"

#include "ohbdeviceconfig.h"

IdlePurgeConfig& IdlePurgeConfig::getInstance()
{
    static IdlePurgeConfig instance;
    return instance;
}

IdlePurgeConfig::IdlePurgeConfig()
    : m_configFilePath(OHBDeviceConfig::getInstance().getConfigPath())
{
}

bool IdlePurgeConfig::isEnabled(const QString &qrCode) const
{
    return OHBDeviceConfig::getInstance().getDeviceByQRCode(qrCode).isIdlePurgeEnabled();
}

int IdlePurgeConfig::getPurgeDurationSeconds(const QString &qrCode) const
{
    return OHBDeviceConfig::getInstance()
        .getDeviceByQRCode(qrCode)
        .getIdlePurgeDurationSeconds();
}

int IdlePurgeConfig::getPurgeIntervalSeconds(const QString &qrCode) const
{
    return OHBDeviceConfig::getInstance()
        .getDeviceByQRCode(qrCode)
        .getIdlePurgeIntervalSeconds();
}

bool IdlePurgeConfig::setEnabled(const QString &qrCode, bool enabled)
{
    return OHBDeviceConfig::getInstance().setIdlePurgeEnabledByQRCode(qrCode, enabled);
}

bool IdlePurgeConfig::setPurgeDurationSeconds(const QString &qrCode, int seconds)
{
    return OHBDeviceConfig::getInstance()
        .setIdlePurgeDurationSecondsByQRCode(qrCode, seconds);
}

bool IdlePurgeConfig::setPurgeIntervalSeconds(const QString &qrCode, int seconds)
{
    return OHBDeviceConfig::getInstance()
        .setIdlePurgeIntervalSecondsByQRCode(qrCode, seconds);
}

QString IdlePurgeConfig::getConfigPath() const
{
    return m_configFilePath;
}
