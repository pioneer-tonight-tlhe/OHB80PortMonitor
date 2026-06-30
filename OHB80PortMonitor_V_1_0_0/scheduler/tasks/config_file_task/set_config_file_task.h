#ifndef SET_CONFIG_FILE_TASK_H
#define SET_CONFIG_FILE_TASK_H

#include "scheduler/scheduler_task.h"
#include "ohbdeviceconfiginfo.h"

#include <QString>
#include <QStringList>
#include <QVector>

class SetConfigFileTask : public SchedulerTask
{
    Q_OBJECT

public:
    struct IniEntry {
        QString group;
        QString key;
        QString value;
    };

    enum class Mode {
        None,
        GenericIni,
        OhbGlobal,
        OhbDevice
    };

    explicit SetConfigFileTask(QObject *parent = nullptr);

    QString taskType() const override { return QStringLiteral("SetConfigFileTask"); }
    void start() override;
    void stop() override;

    void setGenericIni(const QString &fileName, const QVector<IniEntry> &entries);
    void setOhbGlobal(bool idleEnabled,
                      int purgeDurationSeconds,
                      int purgeIntervalSeconds,
                      bool sh85Enabled,
                      int sh85PeriodSeconds,
                      const QVector<QString> &masterDevices);
    void setOhbDevice(const QString &originalQrCode, const OHBDeviceConfigInfo &deviceInfo);

private:
    bool writeGenericIni(QString *errorMessage);
    bool writeOhbGlobal(QString *errorMessage);
    bool writeOhbDevice(QString *errorMessage);
    QString configPath(const QString &fileName) const;

private:
    Mode m_mode;

    QString m_fileName;
    QVector<IniEntry> m_entries;

    bool m_idleEnabled;
    int m_purgeDurationSeconds;
    int m_purgeIntervalSeconds;
    bool m_sh85Enabled;
    int m_sh85PeriodSeconds;
    QVector<QString> m_masterDevices;

    QString m_originalQrCode;
    OHBDeviceConfigInfo m_deviceInfo;
};

#endif // SET_CONFIG_FILE_TASK_H
