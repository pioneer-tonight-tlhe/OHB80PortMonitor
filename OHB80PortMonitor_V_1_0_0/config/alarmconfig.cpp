#include "alarmconfig.h"
#include "appconfig.h"
#include "alarmtype.h"

#include <QDebug>
#include <QDir>
#include <QHash>
#include <QSettings>

namespace {

QString normalizeAlarmKeyText(const QString& value)
{
    QString normalized = value.trimmed().simplified();
    normalized.remove(' ');
    return normalized;
}

const QHash<QString, QString>& displayToConfigKeyMap()
{
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> result;
        const auto alarmTypes = alarmTypeList();
        for (const auto& alarmType : alarmTypes) {
            const QString displayName = alarmType.first.trimmed();
            if (!displayName.isEmpty()) {
                result.insert(displayName, normalizeAlarmKeyText(displayName));
            }
        }
        return result;
    }();
    return map;
}

const QHash<QString, QString>& configKeyToDisplayMap()
{
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> result;
        const auto alarmTypes = alarmTypeList();
        for (const auto& alarmType : alarmTypes) {
            const QString displayName = alarmType.first.trimmed();
            if (!displayName.isEmpty()) {
                result.insert(normalizeAlarmKeyText(displayName), displayName);
            }
        }
        return result;
    }();
    return map;
}

QString toConfigKey(const QString& alarmType)
{
    const QString trimmed = alarmType.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }

    const auto& displayToConfig = displayToConfigKeyMap();
    const auto displayIt = displayToConfig.constFind(trimmed);
    if (displayIt != displayToConfig.cend()) {
        return displayIt.value();
    }

    const QString normalized = normalizeAlarmKeyText(trimmed);
    if (configKeyToDisplayMap().contains(normalized)) {
        return normalized;
    }

    return normalized;
}

QString toDisplayName(const QString& alarmType)
{
    const QString trimmed = alarmType.trimmed();
    if (trimmed.isEmpty()) {
        return trimmed;
    }

    const auto& displayToConfig = displayToConfigKeyMap();
    if (displayToConfig.contains(trimmed)) {
        return trimmed;
    }

    const QString configKey = toConfigKey(trimmed);
    const auto& configToDisplay = configKeyToDisplayMap();
    const auto configIt = configToDisplay.constFind(configKey);
    if (configIt != configToDisplay.cend()) {
        return configIt.value();
    }

    return trimmed;
}

} // namespace

AlarmConfig& AlarmConfig::getInstance()
{
    static AlarmConfig instance;
    return instance;
}

AlarmConfig::AlarmConfig()
{
    const QString configDir = AppConfig::getInstance().getConfigDir();
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

    const QStringList keys = settings.childKeys();
    for (const QString& key : keys) {
        if (settings.value(key, false).toBool()) {
            blockedAlarms.insert(toDisplayName(key));
        }
    }

    settings.endGroup();

    qDebug() << "AlarmConfig: read" << blockedAlarms.size() << "blocked alarm types";
    return blockedAlarms;
}

bool AlarmConfig::setAlarmBlocked(const QString& alarmType, bool blocked)
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    const QString configKey = toConfigKey(alarmType);
    const QString rawKey = alarmType.trimmed();
    const QString displayKey = toDisplayName(alarmType);

    settings.beginGroup("Blocked");
    settings.setValue(configKey, blocked);

    // Keep one canonical key in the ini file while remaining backward compatible.
    if (!rawKey.isEmpty() && rawKey != configKey) {
        settings.remove(rawKey);
    }
    if (!displayKey.isEmpty() && displayKey != configKey && displayKey != rawKey) {
        settings.remove(displayKey);
    }

    settings.endGroup();
    settings.sync();

    qDebug() << "AlarmConfig: set" << alarmType << "as" << configKey << "blocked=" << blocked;
    return settings.status() == QSettings::NoError;
}

bool AlarmConfig::isAlarmBlocked(const QString& alarmType) const
{
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    const QString configKey = toConfigKey(alarmType);
    const QString rawKey = alarmType.trimmed();
    const QString displayKey = toDisplayName(alarmType);

    settings.beginGroup("Blocked");

    bool blocked = settings.value(configKey, false).toBool();
    if (!blocked && !rawKey.isEmpty() && rawKey != configKey) {
        blocked = settings.value(rawKey, false).toBool();
    }
    if (!blocked && !displayKey.isEmpty() && displayKey != configKey && displayKey != rawKey) {
        blocked = settings.value(displayKey, false).toBool();
    }

    settings.endGroup();
    return blocked;
}
