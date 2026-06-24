/*******************************************************************************************
 * @file modulepermissionconfig.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class ModulePermissionConfig
 * @brief 管理模块权限配置的读取、缓存与持久化写回。
 *
 * 设计目标：
 *      1. 统一管理 module_permission.ini 中页面模块的最低访问权限。
 *      2. 为界面层提供稳定的模块权限读取和写回入口。
 *      3. 将配置路径、默认权限和持久化细节封装在配置类内部。
 *******************************************************************************************/
#ifndef MODULEPERMISSIONCONFIG_H
#define MODULEPERMISSIONCONFIG_H

#include <QMap>
#include <QString>

class ModulePermissionConfig
{
public:
    // ============================ 单例访问 ============================
    static ModulePermissionConfig& getInstance();

    // ============================ 权限读取 ============================
    int getPermission(const QString& pageName, const QString& moduleName) const;
    QMap<QString, int> getPagePermissions(const QString& pageName) const;
    QMap<QString, QMap<QString, int>> getAllPermissions() const;
    QString getConfigPath() const;

    // ============================ 权限写入 ============================
    bool setPermission(const QString& pageName, const QString& moduleName, int permission);
    bool setPagePermissions(const QString& pageName, const QMap<QString, int>& permissions);
    bool setAllPermissions(const QMap<QString, QMap<QString, int>>& permissions);

    // ============================ 配置维护 ============================
    bool reload();
    static bool isValidPermission(int permission);

private:
    // ---- 构造函数 ----
    ModulePermissionConfig();
    ~ModulePermissionConfig() = default;
    ModulePermissionConfig(const ModulePermissionConfig&) = delete;
    ModulePermissionConfig& operator=(const ModulePermissionConfig&) = delete;

    // ---- 配置维护 ----
    bool loadConfig();
    bool saveConfig() const;
    static int normalizePermission(int permission);
    static QMap<QString, QMap<QString, int>> createDefaultPermissions();

private:
    // ---- 状态成员 ----
    QString m_configFilePath;                         // 模块权限配置文件路径。
    QMap<QString, QMap<QString, int>> m_permissionMap; // 页面模块最低权限缓存。
};

#endif // MODULEPERMISSIONCONFIG_H
