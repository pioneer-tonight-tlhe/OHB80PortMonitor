/*******************************************************************************************
 * @file electriccabinetserialportconfig.h
 * @author Codex 2026-06-28
 *
 * @class ElectricCabinetSerialPortConfig
 * @brief 负责电控柜串口连接配置的读取、默认值初始化与写回。
 *
 * 设计目标：
 *      1. 为电控柜串口模块提供独立的 .ini 配置文件与统一访问入口。
 *      2. 对串口参数提供稳定默认值，并兼容字符串与枚举之间的转换。
 *      3. 在配置缺失或取值异常时回退到安全默认值，降低部署与升级风险。
 *******************************************************************************************/
#ifndef ELECTRICCABINETSERIALPORTCONFIG_H
#define ELECTRICCABINETSERIALPORTCONFIG_H

#include <QSerialPort>
#include <QString>

// ---- 电控柜串口参数结构 ----
struct ElectricCabinetSerialPortSettings
{
    bool enabled = true;
    QString portName = QStringLiteral("COM1");
    qint32 baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
    bool autoReconnect = true;
    int reconnectIntervalMs = 3000;
    int commandTimeoutMs = 1000;
    int interFrameTimeoutMs = 30;
};

// ---- 电控柜属性监控参数结构 ----
struct ElectricCabinetPropertyMonitorSettings
{
    bool enabled = true;
    int pollIntervalMs = 1000;
    int retryIntervalMs = 3000;
    QString requestFrameHex = QStringLiteral("02 03 00 00 33 33 33");
};

struct ElectricCabinetTempHumiSettings
{
    double tempMax = 60.0;
    double humiMax = 80.0;
    int commandResponseTimeoutMs = 1500;
};

// ---- 电控柜开关控制参数结构 ----
struct ElectricCabinetSwitchControlSettings
{
    int commandResponseTimeoutMs = 1500;
};

class ElectricCabinetSerialPortConfig
{
public:
    // ============================ 单例访问 ============================
    static ElectricCabinetSerialPortConfig& getInstance();

    // ============================ 配置读写 ============================
    ElectricCabinetSerialPortSettings readSettings() const;
    bool writeSettings(const ElectricCabinetSerialPortSettings& settings);
    ElectricCabinetPropertyMonitorSettings readPropertyMonitorSettings() const;
    bool writePropertyMonitorSettings(const ElectricCabinetPropertyMonitorSettings& settings);
    ElectricCabinetTempHumiSettings readTempHumiSettings() const;
    bool writeTempHumiSettings(const ElectricCabinetTempHumiSettings& settings);
    ElectricCabinetSwitchControlSettings readSwitchControlSettings() const;
    bool writeSwitchControlSettings(const ElectricCabinetSwitchControlSettings& settings);

    // ============================ 查询辅助 ============================
    QString getConfigPath() const;

private:
    // ---- 构造与禁拷贝 ----
    ElectricCabinetSerialPortConfig();
    ~ElectricCabinetSerialPortConfig() = default;
    ElectricCabinetSerialPortConfig(const ElectricCabinetSerialPortConfig&) = delete;
    ElectricCabinetSerialPortConfig& operator=(const ElectricCabinetSerialPortConfig&) = delete;

    // ---- 枚举与字符串转换 ----
    static QString dataBitsToString(QSerialPort::DataBits value);
    static QString parityToString(QSerialPort::Parity value);
    static QString stopBitsToString(QSerialPort::StopBits value);
    static QString flowControlToString(QSerialPort::FlowControl value);

    static QSerialPort::DataBits dataBitsFromString(const QString& value, QSerialPort::DataBits fallback);
    static QSerialPort::Parity parityFromString(const QString& value, QSerialPort::Parity fallback);
    static QSerialPort::StopBits stopBitsFromString(const QString& value, QSerialPort::StopBits fallback);
    static QSerialPort::FlowControl flowControlFromString(const QString& value, QSerialPort::FlowControl fallback);

private:
    // ---- 配置文件路径 ----
    QString m_configFilePath;
};

#endif // ELECTRICCABINETSERIALPORTCONFIG_H
