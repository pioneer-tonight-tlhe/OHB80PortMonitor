#include "alarmconfig.h"
#include "appconfig.h"
#include <QSettings>
#include <QDir>
#include <QDebug>

AlarmConfig& AlarmConfig::getInstance()
{
    static AlarmConfig instance;
    return instance;
}

AlarmConfig::AlarmConfig()
{
    QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/alarm.ini";
    
    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }
}

QString AlarmConfig::getAlarmConfigPath() const
{
    return m_configFilePath;
}

QSet<QString> AlarmConfig::readBlockedAlarms() const
{
    QSet<QString> blockedAlarms;
    
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    
    settings.beginGroup("Blocked");
    QStringList keys = settings.childKeys();
    
    for (const QString& key : keys) {
        bool blocked = settings.value(key, false).toBool();
        if (blocked) {
            blockedAlarms.insert(key);
        }
    }
    
    settings.endGroup();
    
    qDebug() << "AlarmConfig: 读取了" << blockedAlarms.size() << "个被屏蔽的警报类型";
    return blockedAlarms;
}

bool AlarmConfig::setAlarmBlocked(const QString& alarmType, bool blocked)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    
    settings.beginGroup("Blocked");
    settings.setValue(alarmType, blocked);
    settings.endGroup();
    
    settings.sync();
    
    qDebug() << "AlarmConfig: 设置警报类型" << alarmType << "为" << (blocked ? "屏蔽" : "不屏蔽");
    
    return settings.status() == QSettings::NoError;
}

bool AlarmConfig::isAlarmBlocked(const QString& alarmType) const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    
    settings.beginGroup("Blocked");
    bool blocked = settings.value(alarmType, false).toBool();
    settings.endGroup();
    
    return blocked;
}
