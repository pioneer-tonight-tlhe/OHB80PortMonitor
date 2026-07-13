#include "set_config_file_task.h"

#include "app/appconfig.h"
#include "app/shareddata.h"
#include "ohbdeviceconfig.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"

#include <QDir>
#include <QSettings>

SetConfigFileTask::SetConfigFileTask(QObject *parent)
    : SchedulerTask(parent)
    , m_mode(Mode::None)
    , m_sh85Enabled(false)
    , m_sh85PeriodSeconds(0)
{
}

void SetConfigFileTask::setGenericIni(const QString &fileName,
                                      const QVector<IniEntry> &entries)
{
    m_mode = Mode::GenericIni;
    m_fileName = fileName;
    m_entries = entries;
}

void SetConfigFileTask::setOhbGlobal(bool sh85Enabled,
                                     int sh85PeriodSeconds,
                                     const QVector<QString> &masterDevices)
{
    m_mode = Mode::OhbGlobal;
    m_sh85Enabled = sh85Enabled;
    m_sh85PeriodSeconds = sh85PeriodSeconds;
    m_masterDevices = masterDevices;
}

void SetConfigFileTask::setOhbDevice(const QString &originalQrCode,
                                     const OHBDeviceConfigInfo &deviceInfo)
{
    m_mode = Mode::OhbDevice;
    m_originalQrCode = originalQrCode;
    m_deviceInfo = deviceInfo;
}

void SetConfigFileTask::start()
{
    setState(Running);

    QString errorMessage;
    bool success = false;

    switch (m_mode) {
    case Mode::GenericIni:
        success = writeGenericIni(&errorMessage);
        break;
    case Mode::OhbGlobal:
        success = writeOhbGlobal(&errorMessage);
        break;
    case Mode::OhbDevice:
        success = writeOhbDevice(&errorMessage);
        break;
    case Mode::None:
        errorMessage = QStringLiteral("no config write mode selected");
        break;
    }

    setState(success ? Finished : Failed);

    const QString message = success
        ? QStringLiteral("SetConfigFileTask completed")
        : QStringLiteral("SetConfigFileTask failed: ") + errorMessage;

    if (OperationDispatchTask *opTask = SharedData::getOperationDispatchTask()) {
        opTask->log(success ? OperationDispatchTask::MsgType::Message
                            : OperationDispatchTask::MsgType::Error,
                    message,
                    0);
    }

    emit finished(success, message);
}

void SetConfigFileTask::stop()
{
    setState(Cancelled);
    emit finished(false, QStringLiteral("SetConfigFileTask cancelled"));
}

bool SetConfigFileTask::writeGenericIni(QString *errorMessage)
{
    if (m_fileName.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("config file name is empty");
        return false;
    }

    QSettings settings(configPath(m_fileName), QSettings::IniFormat);
    for (const IniEntry &entry : m_entries) {
        if (entry.key.trimmed().isEmpty()) {
            continue;
        }

        const QString group = entry.group.trimmed();
        if (!group.isEmpty()) {
            settings.beginGroup(group);
            settings.setValue(entry.key.trimmed(), entry.value);
            settings.endGroup();
        } else {
            settings.setValue(entry.key.trimmed(), entry.value);
        }
    }

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        if (errorMessage) *errorMessage = QStringLiteral("write ") + configPath(m_fileName) + QStringLiteral(" failed");
        return false;
    }

    return true;
}

bool SetConfigFileTask::writeOhbGlobal(QString *errorMessage)
{
    OHBDeviceConfig &ohbConfig = OHBDeviceConfig::getInstance();

    bool success = ohbConfig.setSH85SelfCheckEnabled(m_sh85Enabled);
    success = ohbConfig.setSH85SelfCheckPeriodSeconds(m_sh85PeriodSeconds) && success;
    success = ohbConfig.writeMasterDevices(m_masterDevices) && success;

    if (!success && errorMessage) {
        *errorMessage = QStringLiteral("write ohb_device.ini global config failed");
    }

    return success;
}

bool SetConfigFileTask::writeOhbDevice(QString *errorMessage)
{
    if (m_originalQrCode.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("original QRCode is empty");
        return false;
    }

    QVector<OHBDeviceConfigInfo> devices = OHBDeviceConfig::getInstance().readDevices();
    int targetIndex = -1;
    for (int i = 0; i < devices.size(); ++i) {
        if (devices.at(i).getQrCode() == m_originalQrCode) {
            targetIndex = i;
        } else if (devices.at(i).getQrCode() == m_deviceInfo.getQrCode()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("QRCode already exists: ") + m_deviceInfo.getQrCode();
            }
            return false;
        }
    }

    if (targetIndex < 0) {
        if (errorMessage) *errorMessage = QStringLiteral("device not found: ") + m_originalQrCode;
        return false;
    }

    const QString oldQrCode = devices.at(targetIndex).getQrCode();
    devices[targetIndex] = m_deviceInfo;

    if (!OHBDeviceConfig::getInstance().writeDevices(devices)) {
        if (errorMessage) *errorMessage = QStringLiteral("write device list failed");
        return false;
    }

    if (oldQrCode != m_deviceInfo.getQrCode()) {
        QVector<QString> masterDevices = OHBDeviceConfig::getInstance().readMasterDevices();
        bool changed = false;
        for (QString &masterId : masterDevices) {
            if (masterId == oldQrCode) {
                masterId = m_deviceInfo.getQrCode();
                changed = true;
            }
        }
        if (changed && !OHBDeviceConfig::getInstance().writeMasterDevices(masterDevices)) {
            if (errorMessage) *errorMessage = QStringLiteral("write master device list failed");
            return false;
        }
    }

    SharedData::updateFoupDeviceInfoByQRCode(oldQrCode,
                                             m_deviceInfo.getQrCode(),
                                             m_deviceInfo.getIp(),
                                             m_deviceInfo.getPort());
    return true;
}

QString SetConfigFileTask::configPath(const QString &fileName) const
{
    return QDir(AppConfig::getInstance().getConfigDir()).filePath(fileName);
}
