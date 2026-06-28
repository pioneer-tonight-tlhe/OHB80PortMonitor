#include "electriccabinetserialportconfig.h"

#include "appconfig.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace {
QString normalizedToken(const QString& value)
{
    QString token = value.trimmed();
    token.remove('_');
    token.remove('-');
    token.remove(' ');
    return token.toUpper();
}

int positiveOrDefault(int value, int fallback)
{
    return value > 0 ? value : fallback;
}
} // namespace

ElectricCabinetSerialPortConfig& ElectricCabinetSerialPortConfig::getInstance()
{
    static ElectricCabinetSerialPortConfig instance;
    return instance;
}

ElectricCabinetSerialPortConfig::ElectricCabinetSerialPortConfig()
{
    const QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/electric_cabinet_serial_port.ini";

    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }

    if (!QFileInfo::exists(m_configFilePath)) {
        writeSettings(ElectricCabinetSerialPortSettings());
        writePropertyMonitorSettings(ElectricCabinetPropertyMonitorSettings());
        writeTempHumiSettings(ElectricCabinetTempHumiSettings());
        writeSwitchControlSettings(ElectricCabinetSwitchControlSettings());
    }
}

QString ElectricCabinetSerialPortConfig::getConfigPath() const
{
    return m_configFilePath;
}

ElectricCabinetSerialPortSettings ElectricCabinetSerialPortConfig::readSettings() const
{
    ElectricCabinetSerialPortSettings result;
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetSerialPort");
    result.enabled = settings.value("Enabled", result.enabled).toBool();
    result.portName = settings.value("PortName", result.portName).toString().trimmed().toUpper();
    result.baudRate = settings.value("BaudRate", result.baudRate).toInt();
    result.dataBits = dataBitsFromString(settings.value("DataBits", dataBitsToString(result.dataBits)).toString(), result.dataBits);
    result.parity = parityFromString(settings.value("Parity", parityToString(result.parity)).toString(), result.parity);
    result.stopBits = stopBitsFromString(settings.value("StopBits", stopBitsToString(result.stopBits)).toString(), result.stopBits);
    result.flowControl = flowControlFromString(settings.value("FlowControl", flowControlToString(result.flowControl)).toString(), result.flowControl);
    result.autoReconnect = settings.value("AutoReconnect", result.autoReconnect).toBool();
    result.reconnectIntervalMs = positiveOrDefault(settings.value("ReconnectIntervalMs", result.reconnectIntervalMs).toInt(),
                                                   result.reconnectIntervalMs);
    result.commandTimeoutMs = positiveOrDefault(settings.value("CommandTimeoutMs", result.commandTimeoutMs).toInt(),
                                                result.commandTimeoutMs);
    result.interFrameTimeoutMs = positiveOrDefault(settings.value("InterFrameTimeoutMs", result.interFrameTimeoutMs).toInt(),
                                                   result.interFrameTimeoutMs);
    settings.endGroup();

    return result;
}

ElectricCabinetPropertyMonitorSettings ElectricCabinetSerialPortConfig::readPropertyMonitorSettings() const
{
    ElectricCabinetPropertyMonitorSettings result;
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetPropertyMonitor");
    result.enabled = settings.value("Enabled", result.enabled).toBool();
    result.pollIntervalMs = positiveOrDefault(settings.value("PollIntervalMs", result.pollIntervalMs).toInt(),
                                              result.pollIntervalMs);
    result.retryIntervalMs = positiveOrDefault(settings.value("RetryIntervalMs", result.retryIntervalMs).toInt(),
                                               result.retryIntervalMs);
    result.requestFrameHex = settings.value("RequestFrameHex", result.requestFrameHex).toString().trimmed();
    settings.endGroup();

    return result;
}

ElectricCabinetTempHumiSettings ElectricCabinetSerialPortConfig::readTempHumiSettings() const
{
    ElectricCabinetTempHumiSettings result;
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetTempHumi");
    result.tempMax = settings.value("TempMax", result.tempMax).toDouble();
    result.humiMax = settings.value("HumiMax", result.humiMax).toDouble();
    result.commandResponseTimeoutMs = positiveOrDefault(
        settings.value("CommandResponseTimeoutMs", result.commandResponseTimeoutMs).toInt(),
        result.commandResponseTimeoutMs);
    settings.endGroup();

    return result;
}

ElectricCabinetSwitchControlSettings ElectricCabinetSerialPortConfig::readSwitchControlSettings() const
{
    ElectricCabinetSwitchControlSettings result;
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetSwitchControl");
    result.commandResponseTimeoutMs = positiveOrDefault(
        settings.value("CommandResponseTimeoutMs", result.commandResponseTimeoutMs).toInt(),
        result.commandResponseTimeoutMs);
    settings.endGroup();

    return result;
}

bool ElectricCabinetSerialPortConfig::writeSettings(const ElectricCabinetSerialPortSettings& value)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetSerialPort");
    settings.setValue("Enabled", value.enabled);
    settings.setValue("PortName", value.portName.trimmed().toUpper());
    settings.setValue("BaudRate", value.baudRate);
    settings.setValue("DataBits", dataBitsToString(value.dataBits));
    settings.setValue("Parity", parityToString(value.parity));
    settings.setValue("StopBits", stopBitsToString(value.stopBits));
    settings.setValue("FlowControl", flowControlToString(value.flowControl));
    settings.setValue("AutoReconnect", value.autoReconnect);
    settings.setValue("ReconnectIntervalMs", positiveOrDefault(value.reconnectIntervalMs, 3000));
    settings.setValue("CommandTimeoutMs", positiveOrDefault(value.commandTimeoutMs, 1000));
    settings.setValue("InterFrameTimeoutMs", positiveOrDefault(value.interFrameTimeoutMs, 30));
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool ElectricCabinetSerialPortConfig::writePropertyMonitorSettings(
    const ElectricCabinetPropertyMonitorSettings& value)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetPropertyMonitor");
    settings.setValue("Enabled", value.enabled);
    settings.setValue("PollIntervalMs", positiveOrDefault(value.pollIntervalMs, 1000));
    settings.setValue("RetryIntervalMs", positiveOrDefault(value.retryIntervalMs, 3000));
    settings.setValue("RequestFrameHex", value.requestFrameHex.trimmed());
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool ElectricCabinetSerialPortConfig::writeTempHumiSettings(const ElectricCabinetTempHumiSettings& value)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetTempHumi");
    settings.setValue("TempMax", value.tempMax);
    settings.setValue("HumiMax", value.humiMax);
    settings.setValue("CommandResponseTimeoutMs",
                      positiveOrDefault(value.commandResponseTimeoutMs, 1500));
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}

bool ElectricCabinetSerialPortConfig::writeSwitchControlSettings(
    const ElectricCabinetSwitchControlSettings& value)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup("ElectricCabinetSwitchControl");
    settings.setValue("CommandResponseTimeoutMs",
                      positiveOrDefault(value.commandResponseTimeoutMs, 1500));
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}

QString ElectricCabinetSerialPortConfig::dataBitsToString(QSerialPort::DataBits value)
{
    switch (value) {
        case QSerialPort::Data5: return QStringLiteral("Data5");
        case QSerialPort::Data6: return QStringLiteral("Data6");
        case QSerialPort::Data7: return QStringLiteral("Data7");
        case QSerialPort::Data8: return QStringLiteral("Data8");
        default: return QStringLiteral("Data8");
    }
}

QString ElectricCabinetSerialPortConfig::parityToString(QSerialPort::Parity value)
{
    switch (value) {
        case QSerialPort::NoParity: return QStringLiteral("NoParity");
        case QSerialPort::EvenParity: return QStringLiteral("EvenParity");
        case QSerialPort::OddParity: return QStringLiteral("OddParity");
        case QSerialPort::SpaceParity: return QStringLiteral("SpaceParity");
        case QSerialPort::MarkParity: return QStringLiteral("MarkParity");
        default: return QStringLiteral("NoParity");
    }
}

QString ElectricCabinetSerialPortConfig::stopBitsToString(QSerialPort::StopBits value)
{
    switch (value) {
        case QSerialPort::OneStop: return QStringLiteral("OneStop");
        case QSerialPort::OneAndHalfStop: return QStringLiteral("OneAndHalfStop");
        case QSerialPort::TwoStop: return QStringLiteral("TwoStop");
        default: return QStringLiteral("OneStop");
    }
}

QString ElectricCabinetSerialPortConfig::flowControlToString(QSerialPort::FlowControl value)
{
    switch (value) {
        case QSerialPort::NoFlowControl: return QStringLiteral("NoFlowControl");
        case QSerialPort::HardwareControl: return QStringLiteral("HardwareControl");
        case QSerialPort::SoftwareControl: return QStringLiteral("SoftwareControl");
        default: return QStringLiteral("NoFlowControl");
    }
}

QSerialPort::DataBits ElectricCabinetSerialPortConfig::dataBitsFromString(const QString& value,
                                                                          QSerialPort::DataBits fallback)
{
    const QString token = normalizedToken(value);
    if (token == "5" || token == "DATA5") return QSerialPort::Data5;
    if (token == "6" || token == "DATA6") return QSerialPort::Data6;
    if (token == "7" || token == "DATA7") return QSerialPort::Data7;
    if (token == "8" || token == "DATA8") return QSerialPort::Data8;
    return fallback;
}

QSerialPort::Parity ElectricCabinetSerialPortConfig::parityFromString(const QString& value,
                                                                      QSerialPort::Parity fallback)
{
    const QString token = normalizedToken(value);
    if (token == "NONE" || token == "NO" || token == "NOPARITY") return QSerialPort::NoParity;
    if (token == "EVEN" || token == "EVENPARITY") return QSerialPort::EvenParity;
    if (token == "ODD" || token == "ODDPARITY") return QSerialPort::OddParity;
    if (token == "SPACE" || token == "SPACEPARITY") return QSerialPort::SpaceParity;
    if (token == "MARK" || token == "MARKPARITY") return QSerialPort::MarkParity;
    return fallback;
}

QSerialPort::StopBits ElectricCabinetSerialPortConfig::stopBitsFromString(const QString& value,
                                                                          QSerialPort::StopBits fallback)
{
    const QString token = normalizedToken(value);
    if (token == "1" || token == "ONESTOP") return QSerialPort::OneStop;
    if (token == "1.5" || token == "ONEANDHALFSTOP") return QSerialPort::OneAndHalfStop;
    if (token == "2" || token == "TWOSTOP") return QSerialPort::TwoStop;
    return fallback;
}

QSerialPort::FlowControl ElectricCabinetSerialPortConfig::flowControlFromString(const QString& value,
                                                                                QSerialPort::FlowControl fallback)
{
    const QString token = normalizedToken(value);
    if (token == "NONE" || token == "NO" || token == "NOFLOWCONTROL") return QSerialPort::NoFlowControl;
    if (token == "HARDWARE" || token == "HARDWARECONTROL") return QSerialPort::HardwareControl;
    if (token == "SOFTWARE" || token == "SOFTWARECONTROL") return QSerialPort::SoftwareControl;
    return fallback;
}
