/*******************************************************************************************
 * @file app.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class App
 * @brief 管理应用启动、全局资源初始化与模块权限注册。
 *
 * 设计目标：
 *      1. 统一封装应用启动、日志、共享数据和调度器初始化流程。
 *      2. 为界面层提供全局配置、日志和显示信息访问入口。
 *      3. 集中处理 module_permission.ini 驱动的模块权限注册逻辑。
 *******************************************************************************************/
#ifndef APP_H
#define APP_H

#include "appconfig.h"
#include "applogger.h"

#include <QApplication>
#include <QObject>
#include <QString>

class QWidget;

class App
{
public:
    // ============================ 生命周期 ============================
    static bool initialize();
    static void cleanup();

    // ============================ 全局资源访问 ============================
    static AppConfig* getConfig() { return &AppConfig::getInstance(); }
    static AppLogger* getLogger() { return s_logger; }
    static void getSharedData();
    static void initScheduler();

    // ============================ 权限注册 ============================
    static void registerModulePermission(QWidget* moduleWidget,
                                         QWidget* navWidget,
                                         const QString& pageName,
                                         const QString& moduleName);

    // ============================ 应用信息 ============================
    static QString getAppName();
    static QString getAppVersion();
    static QString getDisplayName();

private:
    // ---- 生命周期 ----
    static void onAboutToQuit();
    static bool initializeLogging();

private:
    // ---- 状态成员 ----
    static bool s_initialized;
    static QString s_appVersion;
    static AppLogger* s_logger;
};

#endif // APP_H
