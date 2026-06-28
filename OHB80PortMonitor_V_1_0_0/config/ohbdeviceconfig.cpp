#include "ohbdeviceconfig.h"
#include "appconfig.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSettings>

namespace {
const char* const QRCodeKey = "QRCode";
const char* const IpKey = "Ip";
const char* const PortKey = "Port";
const char* const DeviceEnableKey = "Enable";

const char* const PurgeFlowKey = "PurgeFlow_l_min";
const char* const LegacyPurgeFlowKeyWithUnderscore = "purge_flow_l_min";
const char* const LegacyPurgeFlowKey = "PurgeFlow";

const char* const VppePressureKey = "VPPEPressure_bar";
const char* const LegacyVppePressureKeyWithUnderscore = "vppe_pressure_bar";
const char* const LegacyVppePressureKey = "VPPEPressure";

const char* const LogoTimeKey = "LogoTime_s";
const char* const LegacyLogoTimeKeyWithUnderscore = "logo_time_s";
const char* const LegacyLogoTimeKey = "LogoTime";

const char* const PageSwitchIntervalKey = "PageSwitchInterval_s";
const char* const LegacyPageSwitchIntervalKeyWithUnderscore = "page_switch_interval_s";
const char* const LegacyPageSwitchIntervalKey = "PageSwitchInterval";

const char* const PageTotalTimeKey = "PageTotalTime_s";
const char* const LegacyPageTotalTimeKeyWithUnderscore = "page_total_time_s";
const char* const LegacyPageTotalTimeKey = "PageTotalTime";

const char* const HumidityOffsetKey = "HumidityOffset_pct";
const char* const LegacyHumidityOffsetKeyWithUnderscore = "humidity_offset_pct";
const char* const LegacyHumidityOffsetKey = "HumidityOffset";

const char* const HumidityLowerLimitKey = "HumidityLowerLimit_pct";
const char* const LegacyHumidityLowerLimitKeyWithUnderscore = "humidity_lower_limit_pct";
const char* const LegacyHumidityLowerLimitKey = "HumidityLowerLimit";

const char* const FoupInAutoPurgeEnableKey = "FoupInAutoPurgeEnable";
const char* const LegacyFoupInAutoPurgeEnableKeyWithUnderscore = "foup_in_auto_purge_enable";

const char* const MasterDevicesGroup = "MasterDevices";
const char* const MasterDevicesKey = "List";
const char* const LegacyMasterDevicesKey = "list";

const char* const SH85SelfCheckTaskGroup = "SH85SelfCheckTask";
const char* const LegacySH85SelfCheckTaskGroup = "sh85selfchecktask";
const char* const EnabledKey = "Enabled";
const char* const LegacyEnabledKey = "enabled";
const char* const PeriodSecondsKey = "Period_s";
const char* const LegacyPeriodSecondsKey = "period_s";

const int DefaultPurgeFlowLitersPerMinute = 35;
const double DefaultVppePressureBar = 3.0;
const int DefaultLogoTimeSeconds = 5;
const int DefaultPageSwitchIntervalSeconds = 5;
const int DefaultPageTotalTimeSeconds = 5;
const double DefaultHumidityOffsetPercent = 0.0;
const double DefaultHumidityLowerLimitPercent = 5.0;
const int DefaultFoupInAutoPurgeEnable = 0;
const int DefaultSh85SelfCheckPeriodSeconds = 1800;

QVariant readConfigValue(QSettings &settings,
                         const QString &groupName,
                         const QString &keyName,
                         const QString &legacyGroupName,
                         const QString &legacyKeyName,
                         const QVariant &defaultValue)
{
    settings.beginGroup(groupName);
    if (settings.contains(keyName)) {
        const QVariant value = settings.value(keyName, defaultValue);
        settings.endGroup();
        return value;
    }
    settings.endGroup();

    settings.beginGroup(legacyGroupName);
    const QVariant value = settings.value(legacyKeyName, defaultValue);
    settings.endGroup();
    return value;
}

QVariant readCurrentGroupValue(const QSettings &settings,
                               const QStringList &keyNames,
                               const QVariant &defaultValue)
{
    for (const QString &keyName : keyNames) {
        if (settings.contains(keyName)) {
            return settings.value(keyName, defaultValue);
        }
    }
    return defaultValue;
}

OHBDeviceConfigInfo readDeviceInfoFromCurrentGroup(const QSettings &settings)
{
    const QString qrCode = readCurrentGroupValue(settings,
                                                 {QRCodeKey},
                                                 QString()).toString();
    const QString ip = readCurrentGroupValue(settings,
                                             {IpKey},
                                             QString()).toString();
    const quint16 port = static_cast<quint16>(readCurrentGroupValue(settings,
                                                                    {PortKey},
                                                                    0).toUInt());
    const bool enable = readCurrentGroupValue(settings,
                                              {DeviceEnableKey},
                                              true).toBool();
    const int purgeFlowLitersPerMinute = readCurrentGroupValue(
        settings,
        {PurgeFlowKey, LegacyPurgeFlowKeyWithUnderscore, LegacyPurgeFlowKey},
        DefaultPurgeFlowLitersPerMinute).toInt();
    const double vppePressureBar = readCurrentGroupValue(
        settings,
        {VppePressureKey, LegacyVppePressureKeyWithUnderscore, LegacyVppePressureKey},
        DefaultVppePressureBar).toDouble();
    const int logoTimeSeconds = readCurrentGroupValue(
        settings,
        {LogoTimeKey, LegacyLogoTimeKeyWithUnderscore, LegacyLogoTimeKey},
        DefaultLogoTimeSeconds).toInt();
    const int pageSwitchIntervalSeconds = readCurrentGroupValue(
        settings,
        {PageSwitchIntervalKey, LegacyPageSwitchIntervalKeyWithUnderscore, LegacyPageSwitchIntervalKey},
        DefaultPageSwitchIntervalSeconds).toInt();
    const int pageTotalTimeSeconds = readCurrentGroupValue(
        settings,
        {PageTotalTimeKey, LegacyPageTotalTimeKeyWithUnderscore, LegacyPageTotalTimeKey},
        DefaultPageTotalTimeSeconds).toInt();
    const double humidityOffsetPercent = readCurrentGroupValue(
        settings,
        {HumidityOffsetKey, LegacyHumidityOffsetKeyWithUnderscore, LegacyHumidityOffsetKey},
        DefaultHumidityOffsetPercent).toDouble();
    const double humidityLowerLimitPercent = readCurrentGroupValue(
        settings,
        {HumidityLowerLimitKey,
         LegacyHumidityLowerLimitKeyWithUnderscore,
         LegacyHumidityLowerLimitKey},
        DefaultHumidityLowerLimitPercent).toDouble();
    const int foupInAutoPurgeEnable = readCurrentGroupValue(
        settings,
        {FoupInAutoPurgeEnableKey, LegacyFoupInAutoPurgeEnableKeyWithUnderscore},
        DefaultFoupInAutoPurgeEnable).toInt();

    return OHBDeviceConfigInfo(qrCode,
                               ip,
                               port,
                               enable,
                               purgeFlowLitersPerMinute,
                               logoTimeSeconds,
                               pageSwitchIntervalSeconds,
                               pageTotalTimeSeconds,
                               humidityOffsetPercent,
                               humidityLowerLimitPercent,
                               vppePressureBar,
                               qBound(0, foupInAutoPurgeEnable, 1));
}
}

OHBDeviceConfig& OHBDeviceConfig::getInstance()
{
    static OHBDeviceConfig instance;
    return instance;
}

OHBDeviceConfig::OHBDeviceConfig()
{
    QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/ohb_device.ini";

    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }
}

QString OHBDeviceConfig::getConfigPath() const
{
    return m_configFilePath;
}

QVector<OHBDeviceConfigInfo> OHBDeviceConfig::readDevices() const
{
    QVector<OHBDeviceConfigInfo> devices;
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        const QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        const OHBDeviceConfigInfo deviceInfo = readDeviceInfoFromCurrentGroup(settings);
        settings.endGroup();

        if (!deviceInfo.getQrCode().isEmpty()
                && !deviceInfo.getIp().isEmpty()
                && deviceInfo.getPort() > 0) {
            devices.append(deviceInfo);
        }
    }

    qDebug() << "OHBDeviceConfig: 读取了" << devices.size() << "个 OHB 设备配置";
    return devices;
}

bool OHBDeviceConfig::writeDevices(const QVector<OHBDeviceConfigInfo>& devices)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        settings.remove(QString("OHB%1").arg(i));
    }

    for (int i = 0; i < devices.size(); ++i) {
        const QString groupName = QString("OHB%1").arg(i + 1);
        const OHBDeviceConfigInfo &deviceInfo = devices.at(i);

        settings.beginGroup(groupName);
        settings.setValue(QRCodeKey, deviceInfo.getQrCode());
        settings.setValue(IpKey, deviceInfo.getIp());
        settings.setValue(PortKey, deviceInfo.getPort());
        settings.setValue(DeviceEnableKey, deviceInfo.isEnabled());
        settings.setValue(PurgeFlowKey, deviceInfo.getPurgeFlowLitersPerMinute());
        settings.setValue(VppePressureKey, deviceInfo.getVppePressureBar());
        settings.setValue(LogoTimeKey, deviceInfo.getLogoTimeSeconds());
        settings.setValue(PageSwitchIntervalKey, deviceInfo.getPageSwitchIntervalSeconds());
        settings.setValue(PageTotalTimeKey, deviceInfo.getPageTotalTimeSeconds());
        settings.setValue(HumidityOffsetKey, deviceInfo.getHumidityOffsetPercent());
        settings.setValue(HumidityLowerLimitKey, deviceInfo.getHumidityLowerLimitPercent());
        settings.setValue(FoupInAutoPurgeEnableKey, deviceInfo.getFoupInAutoPurgeEnable());
        settings.endGroup();
    }

    settings.sync();

    qDebug() << "OHBDeviceConfig: 写入了" << devices.size() << "个 OHB 设备到" << m_configFilePath;
    return settings.status() == QSettings::NoError;
}

QVector<QString> OHBDeviceConfig::readQRCodes() const
{
    QVector<QString> qrCodes;
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        const QString groupName = QString("OHB%1").arg(i);
        const QString qrCode = settings.value(groupName + "/" + QRCodeKey, "").toString();
        if (!qrCode.isEmpty()) {
            qrCodes.append(qrCode);
        }
    }

    qDebug() << "OHBDeviceConfig: 读取了" << qrCodes.size() << "个 QR 码";
    return qrCodes;
}

QVector<QString> OHBDeviceConfig::readMasterDevices() const
{
    QVector<QString> masterDevices;
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup(MasterDevicesGroup);
    QStringList items;
    if (settings.contains(MasterDevicesKey)) {
        items = settings.value(MasterDevicesKey).toStringList();
    } else {
        items = settings.value(LegacyMasterDevicesKey).toStringList();
    }
    settings.endGroup();

    qDebug() << "OHBDeviceConfig::readMasterDevices: 文件路径=" << m_configFilePath
             << "文件存在=" << QFile::exists(m_configFilePath)
             << "解析条数=" << items.size();

    for (const QString &item : items) {
        const QStringList subItems = QString(item).replace(QString::fromUtf8("，"), ",")
                                         .split(',', Qt::SkipEmptyParts);
        for (const QString &subItem : subItems) {
            const QString deviceId = subItem.trimmed();
            if (!deviceId.isEmpty()) {
                masterDevices.append(deviceId);
            }
        }
    }

    qDebug() << "OHBDeviceConfig: 读取了" << masterDevices.size() << "个主设备";
    return masterDevices;
}

bool OHBDeviceConfig::writeMasterDevices(const QVector<QString>& masterDevices)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    QStringList items;
    for (const QString &deviceId : masterDevices) {
        items.append(deviceId);
    }

    settings.beginGroup(MasterDevicesGroup);
    settings.setValue(MasterDevicesKey, items.join(","));
    settings.endGroup();

    settings.sync();

    qDebug() << "OHBDeviceConfig: 写入了" << masterDevices.size() << "个主设备到" << m_configFilePath;
    return settings.status() == QSettings::NoError;
}

OHBDeviceConfigInfo OHBDeviceConfig::getDeviceByQRCode(const QString& qrCode) const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        const QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        const QString currentQrCode = readCurrentGroupValue(settings,
                                                            {QRCodeKey},
                                                            QString()).toString();
        if (currentQrCode == qrCode) {
            const OHBDeviceConfigInfo deviceInfo = readDeviceInfoFromCurrentGroup(settings);
            settings.endGroup();
            return deviceInfo;
        }
        settings.endGroup();
    }

    return OHBDeviceConfigInfo();
}

OHBDeviceConfigInfo OHBDeviceConfig::getDeviceByMasterId(const QString& masterId) const
{
    return getDeviceByQRCode(masterId);
}

bool OHBDeviceConfig::setDeviceEnable(const QString& qrCode, bool enable)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        const QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        const QString currentQrCode = settings.value(QRCodeKey, "").toString();
        if (currentQrCode == qrCode) {
            settings.setValue(DeviceEnableKey, enable);
            settings.endGroup();
            settings.sync();
            qDebug() << "OHBDeviceConfig: 设置设备" << qrCode << "enable=" << enable;
            return settings.status() == QSettings::NoError;
        }
        settings.endGroup();
    }

    qWarning() << "OHBDeviceConfig: 未找到 QRCode=" << qrCode << "的设备";
    return false;
}

bool OHBDeviceConfig::setVppePressureBarByQRCode(const QString& qrCode, double vppePressureBar)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        const QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        const QString currentQrCode = settings.value(QRCodeKey, "").toString();
        if (currentQrCode == qrCode) {
            settings.setValue(VppePressureKey, vppePressureBar);
            settings.endGroup();
            settings.sync();
            qDebug() << "OHBDeviceConfig: 设置设备" << qrCode
                     << "VPPEPressure_bar=" << vppePressureBar;
            return settings.status() == QSettings::NoError;
        }
        settings.endGroup();
    }

    qWarning() << "OHBDeviceConfig: 未找到 QRCode=" << qrCode << "的设备，无法写入 VPPEPressure_bar";
    return false;
}

bool OHBDeviceConfig::setFoupInAutoPurgeEnableByQRCode(const QString& qrCode, int enable)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    const int normalizedEnable = qBound(0, enable, 1);

    for (int i = 1; i <= 80; ++i) {
        const QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        const QString currentQrCode = settings.value(QRCodeKey, "").toString();
        if (currentQrCode == qrCode) {
            settings.setValue(FoupInAutoPurgeEnableKey, normalizedEnable);
            settings.endGroup();
            settings.sync();
            qDebug() << "OHBDeviceConfig: 设置设备" << qrCode
                     << "FoupInAutoPurgeEnable=" << normalizedEnable;
            return settings.status() == QSettings::NoError;
        }
        settings.endGroup();
    }

    qWarning() << "OHBDeviceConfig: 未找到 QRCode=" << qrCode
               << "的设备，无法写入 FoupInAutoPurgeEnable";
    return false;
}

bool OHBDeviceConfig::updateDeviceInfoByQRCode(const QString& oldQrCode,
                                               const QString& newQrCode,
                                               const QString& ip,
                                               quint16 port)
{
    QVector<OHBDeviceConfigInfo> devices = readDevices();
    int targetIndex = -1;

    for (int i = 0; i < devices.size(); ++i) {
        if (devices.at(i).getQrCode() == oldQrCode) {
            targetIndex = i;
        } else if (devices.at(i).getQrCode() == newQrCode) {
            qWarning() << "OHBDeviceConfig: QRCode already exists:" << newQrCode;
            return false;
        }
    }

    if (targetIndex < 0) {
        qWarning() << "OHBDeviceConfig: device not found, QRCode=" << oldQrCode;
        return false;
    }

    devices[targetIndex].setQrCode(newQrCode);
    devices[targetIndex].setIp(ip);
    devices[targetIndex].setPort(port);

    if (!writeDevices(devices)) {
        return false;
    }

    QVector<QString> masterDevices = readMasterDevices();
    bool masterListChanged = false;
    for (QString &masterId : masterDevices) {
        if (masterId == oldQrCode) {
            masterId = newQrCode;
            masterListChanged = true;
        }
    }

    if (masterListChanged && !writeMasterDevices(masterDevices)) {
        return false;
    }

    qDebug() << "OHBDeviceConfig: updated device info"
             << "oldQrCode=" << oldQrCode
             << "newQrCode=" << newQrCode
             << "ip=" << ip
             << "port=" << port;
    return true;
}

bool OHBDeviceConfig::readSH85SelfCheckEnabled() const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    const bool enabled = readConfigValue(settings,
                                         SH85SelfCheckTaskGroup,
                                         EnabledKey,
                                         LegacySH85SelfCheckTaskGroup,
                                         LegacyEnabledKey,
                                         true).toBool();
    qDebug() << "OHBDeviceConfig: SH85SelfCheck enabled=" << enabled;
    return enabled;
}

int OHBDeviceConfig::readSH85SelfCheckPeriodSeconds() const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    const int seconds = readConfigValue(settings,
                                        SH85SelfCheckTaskGroup,
                                        PeriodSecondsKey,
                                        LegacySH85SelfCheckTaskGroup,
                                        LegacyPeriodSecondsKey,
                                        DefaultSh85SelfCheckPeriodSeconds).toInt();
    qDebug() << "OHBDeviceConfig: SH85SelfCheck period_s=" << seconds;
    return seconds > 0 ? seconds : DefaultSh85SelfCheckPeriodSeconds;
}

bool OHBDeviceConfig::setSH85SelfCheckEnabled(bool enabled)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.beginGroup(SH85SelfCheckTaskGroup);
    settings.setValue(EnabledKey, enabled);
    settings.endGroup();
    settings.sync();
    qDebug() << "OHBDeviceConfig: set SH85SelfCheck enabled=" << enabled;
    return settings.status() == QSettings::NoError;
}

bool OHBDeviceConfig::setSH85SelfCheckPeriodSeconds(int seconds)
{
    if (seconds <= 0) {
        seconds = DefaultSh85SelfCheckPeriodSeconds;
    }

    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.beginGroup(SH85SelfCheckTaskGroup);
    settings.setValue(PeriodSecondsKey, seconds);
    settings.endGroup();
    settings.sync();
    qDebug() << "OHBDeviceConfig: set SH85SelfCheck period_s=" << seconds;
    return settings.status() == QSettings::NoError;
}
