#include "ohbdeviceconfig.h"
#include "appconfig.h"
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QDebug>

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

QVector<OHBDeviceInfo> OHBDeviceConfig::readDevices() const
{
    QVector<OHBDeviceInfo> devices;

    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        QString qrCode = settings.value("QRCode", "").toString();
        QString ip = settings.value("Ip", "").toString();
        quint16 port = settings.value("Port", 0).toUInt();
        bool enable = settings.value("Enable", true).toBool();
        settings.endGroup();

        if (!qrCode.isEmpty() && !ip.isEmpty() && port > 0) {
            devices.append(OHBDeviceInfo(qrCode, ip, port, enable));
        }
    }

    qDebug() << "OHBDeviceConfig: 读取了" << devices.size() << "个 OHB 设备配置";
    return devices;
}

bool OHBDeviceConfig::writeDevices(const QVector<OHBDeviceInfo>& devices)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    // 先备份 MasterDevices 节，clear 时会被清除
    QStringList masterList = settings.value("MasterDevices/list").toStringList();

    settings.clear();

    if (!masterList.isEmpty()) {
        settings.setValue("MasterDevices/list", masterList.join(","));
    }

    for (int i = 0; i < devices.size(); ++i) {
        QString groupName = QString("OHB%1").arg(i + 1);
        settings.beginGroup(groupName);
        settings.setValue("QRCode", devices[i].qrCode);
        settings.setValue("Ip", devices[i].ip);
        settings.setValue("Port", devices[i].port);
        settings.setValue("Enable", devices[i].enable);
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
        QString groupName = QString("OHB%1").arg(i);
        QString qrCode = settings.value(groupName + "/QRCode", "").toString();
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

    settings.beginGroup("MasterDevices");
    // QSettings IniFormat 会将逗号分隔值自动解析为 QStringList，
    // 直接使用 toStringList() 同时兼容单值和多值两种情况
    QStringList items = settings.value("list").toStringList();
    settings.endGroup();

    qDebug() << "OHBDeviceConfig::readMasterDevices: 文件路径=" << m_configFilePath
             << "文件存在=" << QFile::exists(m_configFilePath)
             << "解析条数=" << items.size();

    for (const QString& item : items) {
        // 兼容中文逗号混入同一条目的情况
        const QStringList subItems = QString(item).replace(QString::fromUtf8("，"), ",")
                                         .split(',', Qt::SkipEmptyParts);
        for (const QString& sub : subItems) {
            QString deviceId = sub.trimmed();
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
    for (const QString& device : masterDevices) {
        items.append(device);
    }
    QString listStr = items.join(",");

    settings.beginGroup("MasterDevices");
    settings.setValue("list", listStr);
    settings.endGroup();

    settings.sync();

    qDebug() << "OHBDeviceConfig: 写入了" << masterDevices.size() << "个主设备到" << m_configFilePath;
    return settings.status() == QSettings::NoError;
}

OHBDeviceInfo OHBDeviceConfig::getDeviceByQRCode(const QString& qrCode) const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        QString qr = settings.value("QRCode", "").toString();
        if (qr == qrCode) {
            QString ip = settings.value("Ip", "").toString();
            quint16 port = settings.value("Port", 0).toUInt();
            bool enable = settings.value("Enable", true).toBool();
            settings.endGroup();
            return OHBDeviceInfo(qr, ip, port, enable);
        }
        settings.endGroup();
    }

    return OHBDeviceInfo();
}

OHBDeviceInfo OHBDeviceConfig::getDeviceByMasterId(const QString& masterId) const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.beginGroup(masterId);
    QString qrCode = settings.value("QRCode", "").toString();
    QString ip = settings.value("Ip", "").toString();
    quint16 port = settings.value("Port", 0).toUInt();
    bool enable = settings.value("Enable", true).toBool();
    settings.endGroup();

    return OHBDeviceInfo(qrCode, ip, port, enable);
}

bool OHBDeviceConfig::setDeviceEnable(const QString& qrCode, bool enable)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    for (int i = 1; i <= 80; ++i) {
        QString groupName = QString("OHB%1").arg(i);
        settings.beginGroup(groupName);
        QString qr = settings.value("QRCode", "").toString();
        if (qr == qrCode) {
            settings.setValue("Enable", enable);
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

bool OHBDeviceConfig::updateDeviceInfoByQRCode(const QString& oldQrCode,
                                               const QString& newQrCode,
                                               const QString& ip,
                                               quint16 port)
{
    QVector<OHBDeviceInfo> devices = readDevices();
    int targetIndex = -1;

    for (int i = 0; i < devices.size(); ++i) {
        if (devices.at(i).qrCode == oldQrCode) {
            targetIndex = i;
        } else if (devices.at(i).qrCode == newQrCode) {
            qWarning() << "OHBDeviceConfig: QRCode already exists:" << newQrCode;
            return false;
        }
    }

    if (targetIndex < 0) {
        qWarning() << "OHBDeviceConfig: device not found, QRCode=" << oldQrCode;
        return false;
    }

    devices[targetIndex].qrCode = newQrCode;
    devices[targetIndex].ip = ip;
    devices[targetIndex].port = port;

    if (!writeDevices(devices)) {
        return false;
    }

    QVector<QString> masterDevices = readMasterDevices();
    bool masterListChanged = false;
    for (QString& masterId : masterDevices) {
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
    settings.beginGroup("sh85selfchecktask");
    const bool enabled = settings.value("enabled", true).toBool();
    settings.endGroup();
    qDebug() << "OHBDeviceConfig: SH85SelfCheck enabled=" << enabled;
    return enabled;
}

int OHBDeviceConfig::readSH85SelfCheckPeriodSeconds() const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.beginGroup("sh85selfchecktask");
    // period_s 直接为秒数，默认 1800s（30 分钟）
    const int seconds = settings.value("period_s", 1800).toInt();
    settings.endGroup();
    qDebug() << "OHBDeviceConfig: SH85SelfCheck period_s=" << seconds;
    return seconds > 0 ? seconds : 1800;
}

bool OHBDeviceConfig::setSH85SelfCheckEnabled(bool enabled)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.beginGroup("sh85selfchecktask");
    settings.setValue("enabled", enabled);
    settings.endGroup();
    settings.sync();
    qDebug() << "OHBDeviceConfig: set SH85SelfCheck enabled=" << enabled;
    return settings.status() == QSettings::NoError;
}

bool OHBDeviceConfig::setSH85SelfCheckPeriodSeconds(int seconds)
{
    if (seconds <= 0) seconds = 1800; // 合理下限保护（默认 30 分钟）
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.beginGroup("sh85selfchecktask");
    settings.setValue("period_s", seconds);
    settings.endGroup();
    settings.sync();
    qDebug() << "OHBDeviceConfig: set SH85SelfCheck period_s=" << seconds;
    return settings.status() == QSettings::NoError;
}
