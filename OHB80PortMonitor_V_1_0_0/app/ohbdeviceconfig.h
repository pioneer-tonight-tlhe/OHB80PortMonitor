#ifndef OHBDEVICECONFIG_H
#define OHBDEVICECONFIG_H

#include <QString>
#include <QVector>

// 单个 OHB 设备的完整配置信息（QR 码 + 网络信息 + 可用性）
struct OHBDeviceInfo
{
    QString qrCode;     // 二维码
    QString ip;         // IP 地址
    quint16 port;       // 端口号
    bool enable;        // 设备可用性（true=启用，false=禁用）

    OHBDeviceInfo() : port(0), enable(true) {}
    OHBDeviceInfo(const QString& qr, const QString& ipAddr, quint16 portNum, bool en = true)
        : qrCode(qr), ip(ipAddr), port(portNum), enable(en) {}
};

// 合并 QRCodeConfig + NetworkConfig 的统一配置类
// 配置文件：ohb_device.ini
// 节结构：
//   [MasterDevices]
//     list=12001,12010,...
//   [OHB1]
//     QRCode=12001
//     Ip=127.0.0.1
//     Port=501
//   ...
class OHBDeviceConfig
{
public:
    static OHBDeviceConfig& getInstance();

    // 读取所有 OHB 设备配置 (按 OHB1..OHB80 顺序)
    QVector<OHBDeviceInfo> readDevices() const;
    // 写入所有 OHB 设备配置 (按 OHB1..OHB80 顺序)
    bool writeDevices(const QVector<OHBDeviceInfo>& devices);

    // 仅读取所有 QR 码 (按 OHB1..OHB80 顺序)
    QVector<QString> readQRCodes() const;

    // 主设备列表
    QVector<QString> readMasterDevices() const;
    bool writeMasterDevices(const QVector<QString>& masterDevices);

    // 通过 QR 码查找对应设备的网络信息
    OHBDeviceInfo getDeviceByQRCode(const QString& qrCode) const;

    // 通过 masterId (如 "OHB1") 查找设备
    OHBDeviceInfo getDeviceByMasterId(const QString& masterId) const;

    // 设置设备 enable 状态（持久化到配置文件）
    bool setDeviceEnable(const QString& qrCode, bool enable);
    bool updateDeviceInfoByQRCode(const QString& oldQrCode,
                                   const QString& newQrCode,
                                   const QString& ip,
                                   quint16 port);

    QString getConfigPath() const;

    // SH85 周期自检配置
    bool readSH85SelfCheckEnabled() const;
    int  readSH85SelfCheckPeriodSeconds() const;
    bool setSH85SelfCheckEnabled(bool enabled);
    bool setSH85SelfCheckPeriodSeconds(int seconds);

private:
    OHBDeviceConfig();
    ~OHBDeviceConfig() = default;
    OHBDeviceConfig(const OHBDeviceConfig&) = delete;
    OHBDeviceConfig& operator=(const OHBDeviceConfig&) = delete;

    QString m_configFilePath;
};

#endif // OHBDEVICECONFIG_H
