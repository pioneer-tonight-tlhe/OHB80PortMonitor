#include "modulepermissionconfig.h"
#include "appconfig.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStringList>
#include <QtGlobal>

namespace {
const char* const ConfigFileName = "module_permission.ini";
const char* const ConfigPageGroup = "ConfigPage";
const char* const DebugPageGroup = "DebugPage";
const int GuestPermission = 0;
const int RootPermission = 4;

QString normalizedConfigName(const QString& name)
{
    return name.trimmed();
}
}

ModulePermissionConfig& ModulePermissionConfig::getInstance()
{
    // 使用函数内静态对象统一管理配置单例生命周期。
    static ModulePermissionConfig instance;
    return instance;
}

ModulePermissionConfig::ModulePermissionConfig()
{
    // 根据应用配置目录定位 module_permission.ini，并在构造时加载默认权限。
    const QString configDir = AppConfig::getInstance().getConfigDir();
    m_configFilePath = configDir + "/" + ConfigFileName;

    QDir dir;
    if (!dir.exists(configDir)) {
        dir.mkpath(configDir);
    }

    loadConfig();
}

int ModulePermissionConfig::getPermission(const QString& pageName,
                                          const QString& moduleName) const
{
    // 缺失或未知的模块按 Root 处理，避免误开放未配置功能。
    const QString normalizedPageName = normalizedConfigName(pageName);
    const QString normalizedModuleName = normalizedConfigName(moduleName);
    if (normalizedPageName.isEmpty() || normalizedModuleName.isEmpty()) {
        return RootPermission;
    }

    const QMap<QString, QMap<QString, int>> defaultMap = createDefaultPermissions();
    if (m_permissionMap.contains(normalizedPageName)
            && m_permissionMap.value(normalizedPageName).contains(normalizedModuleName)) {
        return m_permissionMap.value(normalizedPageName).value(normalizedModuleName);
    }

    if (defaultMap.contains(normalizedPageName)
            && defaultMap.value(normalizedPageName).contains(normalizedModuleName)) {
        return defaultMap.value(normalizedPageName).value(normalizedModuleName);
    }

    return RootPermission;
}

QMap<QString, int> ModulePermissionConfig::getPagePermissions(const QString& pageName) const
{
    // 返回指定页面的模块权限快照，页面不存在时返回空表。
    const QString normalizedPageName = normalizedConfigName(pageName);
    return m_permissionMap.value(normalizedPageName);
}

QMap<QString, QMap<QString, int>> ModulePermissionConfig::getAllPermissions() const
{
    // 返回当前缓存的全部页面模块权限。
    return m_permissionMap;
}

QString ModulePermissionConfig::getConfigPath() const
{
    // 返回 module_permission.ini 的实际落盘路径。
    return m_configFilePath;
}

bool ModulePermissionConfig::setPermission(const QString& pageName,
                                           const QString& moduleName,
                                           int permission)
{
    // 单项权限写入前先校验页面、模块和权限等级。
    const QString normalizedPageName = normalizedConfigName(pageName);
    const QString normalizedModuleName = normalizedConfigName(moduleName);
    if (normalizedPageName.isEmpty()
            || normalizedModuleName.isEmpty()
            || !isValidPermission(permission)) {
        return false;
    }

    m_permissionMap[normalizedPageName][normalizedModuleName] = permission;
    return saveConfig();
}

bool ModulePermissionConfig::setPagePermissions(const QString& pageName,
                                                const QMap<QString, int>& permissions)
{
    // 整页写入会替换该页面原有权限配置。
    const QString normalizedPageName = normalizedConfigName(pageName);
    if (normalizedPageName.isEmpty()) {
        return false;
    }

    QMap<QString, int> normalizedPermissions;
    for (auto it = permissions.constBegin(); it != permissions.constEnd(); ++it) {
        const QString normalizedModuleName = normalizedConfigName(it.key());
        if (normalizedModuleName.isEmpty() || !isValidPermission(it.value())) {
            return false;
        }

        normalizedPermissions.insert(normalizedModuleName, it.value());
    }

    m_permissionMap[normalizedPageName] = normalizedPermissions;
    return saveConfig();
}

bool ModulePermissionConfig::setAllPermissions(
        const QMap<QString, QMap<QString, int>>& permissions)
{
    // 全量写入会替换当前缓存，适合权限配置界面一次性保存。
    QMap<QString, QMap<QString, int>> normalizedPermissions;
    for (auto pageIt = permissions.constBegin(); pageIt != permissions.constEnd(); ++pageIt) {
        const QString normalizedPageName = normalizedConfigName(pageIt.key());
        if (normalizedPageName.isEmpty()) {
            return false;
        }

        QMap<QString, int> pagePermissions;
        for (auto moduleIt = pageIt.value().constBegin();
             moduleIt != pageIt.value().constEnd();
             ++moduleIt) {
            const QString normalizedModuleName = normalizedConfigName(moduleIt.key());
            if (normalizedModuleName.isEmpty() || !isValidPermission(moduleIt.value())) {
                return false;
            }

            pagePermissions.insert(normalizedModuleName, moduleIt.value());
        }

        normalizedPermissions.insert(normalizedPageName, pagePermissions);
    }

    m_permissionMap = normalizedPermissions;
    return saveConfig();
}

bool ModulePermissionConfig::reload()
{
    // 重新从 ini 文件加载权限配置，用于外部修改文件后的刷新。
    return loadConfig();
}

bool ModulePermissionConfig::isValidPermission(int permission)
{
    // 权限等级与 UserPermission 枚举值保持一致：0 到 4。
    return permission >= GuestPermission && permission <= RootPermission;
}

bool ModulePermissionConfig::loadConfig()
{
    // 先加载默认权限，再用 ini 文件中存在的配置项覆盖默认值。
    const bool configFileExists = QFileInfo::exists(m_configFilePath);
    m_permissionMap = createDefaultPermissions();

    QSettings settings(m_configFilePath, QSettings::IniFormat);
    const QStringList groups = settings.childGroups();
    for (const QString& groupName : groups) {
        settings.beginGroup(groupName);

        const QString normalizedPageName = normalizedConfigName(groupName);
        if (!m_permissionMap.contains(normalizedPageName)) {
            m_permissionMap.insert(normalizedPageName, QMap<QString, int>());
        }

        const QStringList keys = settings.childKeys();
        for (const QString& keyName : keys) {
            const QString normalizedModuleName = normalizedConfigName(keyName);
            if (normalizedModuleName.isEmpty()) {
                continue;
            }

            const int permission = normalizePermission(settings.value(keyName,
                                                                      RootPermission).toInt());
            m_permissionMap[normalizedPageName].insert(normalizedModuleName, permission);
        }

        settings.endGroup();
    }

    if (!configFileExists) {
        return saveConfig();
    }

    return settings.status() == QSettings::NoError;
}

bool ModulePermissionConfig::saveConfig() const
{
    // 将缓存中的全部页面模块权限一次性写回 ini 文件。
    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.clear();

    for (auto pageIt = m_permissionMap.constBegin(); pageIt != m_permissionMap.constEnd(); ++pageIt) {
        settings.beginGroup(pageIt.key());

        for (auto moduleIt = pageIt.value().constBegin();
             moduleIt != pageIt.value().constEnd();
             ++moduleIt) {
            settings.setValue(moduleIt.key(), moduleIt.value());
        }

        settings.endGroup();
    }

    settings.sync();
    return settings.status() == QSettings::NoError;
}

int ModulePermissionConfig::normalizePermission(int permission)
{
    // 文件中出现越界值时钳制到 0 到 4，避免异常配置继续扩散。
    return qBound(GuestPermission, permission, RootPermission);
}

QMap<QString, QMap<QString, int>> ModulePermissionConfig::createDefaultPermissions()
{
    // 默认权限与 module_permission.ini 当前版本保持一致。
    QMap<QString, QMap<QString, int>> permissions;

    QMap<QString, int> configPagePermissions;
    configPagePermissions.insert("IdlePurgeConfiguration", 1);
    configPagePermissions.insert("IdlePurgeConfiguration.PreparationTime", 3);
    configPagePermissions.insert("PneumaticValvePressureConfiguration", 1);
    configPagePermissions.insert("SH85PeriodicSelfCheckConfiguration", 1);
    configPagePermissions.insert("SH85SelfCheckConfiguration", 1);
    configPagePermissions.insert("PurgeFlowConfiguration", 1);
    configPagePermissions.insert("DeviceEnableConfiguration", 1);
    configPagePermissions.insert("DeviceInfoConfiguration", 1);
    configPagePermissions.insert("DeviceInfoConfiguration.ViewDeviceInformation", 1);
    configPagePermissions.insert("DeviceInfoConfiguration.CurrentQRCode", 2);
    configPagePermissions.insert("DeviceInfoConfiguration.NewQRCode", 2);
    configPagePermissions.insert("DeviceInfoConfiguration.IPPort", 2);
    permissions.insert(ConfigPageGroup, configPagePermissions);

    QMap<QString, int> debugPagePermissions;
    debugPagePermissions.insert("FirmwareConfig", 2);
    debugPagePermissions.insert("FirmwareUpdate", 2);
    debugPagePermissions.insert("VEFCGasTypeConfiguration", 3);
    debugPagePermissions.insert("UIRefreshTimeConfiguration", 3);
    debugPagePermissions.insert("HumidityOffsetConfiguration", 3);
    debugPagePermissions.insert("VEFCFlowUnitMediumStatus", 3);
    debugPagePermissions.insert("VEFCSensorMonitor", 3);
    debugPagePermissions.insert("FreeRTOSTaskStackMonitor", 3);
    debugPagePermissions.insert("BoardEnableStatus", 3);
    debugPagePermissions.insert("FoupInVacuumExtractionEnable", 3);
    debugPagePermissions.insert("FoupInAutoPurgeEnable", 3);
    debugPagePermissions.insert("ConfigFiles", 3);
    permissions.insert(DebugPageGroup, debugPagePermissions);

    return permissions;
}
