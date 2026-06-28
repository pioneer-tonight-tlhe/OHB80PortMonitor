#include "idlepurgeconfig.h"
#include "appconfig.h"

#include <QDir>
#include <QSettings>

namespace {
const char* const IdleConfigGroup = "IdleConfig";
const char* const LegacyIdleConfigGroup = "idleconfig";
const char* const EnabledKey = "Enabled";
const char* const LegacyEnabledKey = "enabled";
const char* const PurgeDurationSecondsKey = "PurgeDuration_s";
const char* const LegacyPurgeDurationSecondsKey = "purge_duration_s";
const char* const PurgeIntervalSecondsKey = "PurgeInterval_s";
const char* const LegacyPurgeIntervalSecondsKey = "purge_interval_s";
const int DefaultPurgeDurationSeconds = 10;
const int DefaultPurgeIntervalSeconds = 5;

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
}

IdlePurgeConfig& IdlePurgeConfig::getInstance()
{
    // 使用函数内静态对象统一管理单例生命周期。
    static IdlePurgeConfig instance;
    return instance;
}

IdlePurgeConfig::IdlePurgeConfig()
    : m_enabled(true)
    , m_purgeDurationSeconds(DefaultPurgeDurationSeconds)
    , m_purgeIntervalSeconds(DefaultPurgeIntervalSeconds)
{
    // 统一定位 Idle Purge 配置文件，并在构造时完成首次缓存加载。
    QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/ohb_device.ini";

    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }

    loadConfig();
}

bool IdlePurgeConfig::isEnabled() const
{
    // 返回当前缓存的 Idle Purge 使能状态。
    return m_enabled;
}

int IdlePurgeConfig::getPurgeDurationSeconds() const
{
    // 返回当前缓存的 Purge Duration 秒数。
    return m_purgeDurationSeconds;
}

int IdlePurgeConfig::getPurgeIntervalSeconds() const
{
    // 返回当前缓存的 Purge Interval 秒数。
    return m_purgeIntervalSeconds;
}

bool IdlePurgeConfig::setEnabled(bool enabled)
{
    // 先更新内存缓存，再统一写回配置文件。
    m_enabled = enabled;
    return saveConfig();
}

bool IdlePurgeConfig::setPurgeDurationSeconds(int seconds)
{
    // 对外部传入值做兜底，避免配置文件中写入非法时长。
    m_purgeDurationSeconds = seconds > 0 ? seconds : DefaultPurgeDurationSeconds;
    return saveConfig();
}

bool IdlePurgeConfig::setPurgeIntervalSeconds(int seconds)
{
    // 对外部传入值做兜底，避免配置文件中写入非法周期。
    m_purgeIntervalSeconds = seconds > 0 ? seconds : DefaultPurgeIntervalSeconds;
    return saveConfig();
}

QString IdlePurgeConfig::getConfigPath() const
{
    // 返回当前 Idle Purge 配置实际落盘的 ini 路径。
    return m_configFilePath;
}

void IdlePurgeConfig::loadConfig()
{
    // 从 [IdleConfig] 配置段读取缓存值，并兼容旧版配置名。
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    m_enabled = readConfigValue(settings,
                                IdleConfigGroup,
                                EnabledKey,
                                LegacyIdleConfigGroup,
                                LegacyEnabledKey,
                                true).toBool();
    m_purgeDurationSeconds = readConfigValue(settings,
                                             IdleConfigGroup,
                                             PurgeDurationSecondsKey,
                                             LegacyIdleConfigGroup,
                                             LegacyPurgeDurationSecondsKey,
                                             DefaultPurgeDurationSeconds).toInt();
    m_purgeIntervalSeconds = readConfigValue(settings,
                                             IdleConfigGroup,
                                             PurgeIntervalSecondsKey,
                                             LegacyIdleConfigGroup,
                                             LegacyPurgeIntervalSecondsKey,
                                             DefaultPurgeIntervalSeconds).toInt();

    // 对读取到的异常秒数做兜底，避免后续界面和任务层拿到非法值。
    if (m_purgeDurationSeconds <= 0) {
        m_purgeDurationSeconds = DefaultPurgeDurationSeconds;
    }
    if (m_purgeIntervalSeconds <= 0) {
        m_purgeIntervalSeconds = DefaultPurgeIntervalSeconds;
    }
}

bool IdlePurgeConfig::saveConfig()
{
    // 将当前内存缓存一次性写回 [IdleConfig] 配置段并同步落盘。
    QSettings settings(m_configFilePath, QSettings::IniFormat);

    settings.beginGroup(IdleConfigGroup);
    settings.setValue(EnabledKey, m_enabled);
    settings.setValue(PurgeDurationSecondsKey, m_purgeDurationSeconds);
    settings.setValue(PurgeIntervalSecondsKey, m_purgeIntervalSeconds);
    settings.endGroup();

    settings.sync();
    return settings.status() == QSettings::NoError;
}
