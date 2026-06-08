#include "app.h"
#include "appconfig.h"
#include "applogger.h"
#include "shareddata.h"
#include "qthelper.h"
#include "metatypes.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "scheduler/scheduler.h"
#include "logdatabases/databasemanager.h"
#include <qdir>
#include <qstandardpaths>
#include <qdebug>
#include <qfile>
#include <qtextstream>
#include <qdatetime>
#include <qicon>

// 静态成员变量定义
bool App::s_initialized = false;
QString App::s_appVersion = "1.0.0";
AppLogger* App::s_logger = nullptr;

bool App::initialize()
{
    if (s_initialized) {
        return true;
    }
    
    qDebug() << "Initializing application...";
    
    // 注册元类型
    MetaTypes::registerTypes();

    // 单实例检查
    if (!QtHelper::checkSingleInstance(getAppName())) {
        return false;
    }
    
    // 连接应用程序退出信号
    QObject::connect(qApp, &QApplication::aboutToQuit, []() {
        App::onAboutToQuit();
    });
    
    // 初始化日志
    if (!initializeLogging()) {
        qCritical() << "Failed to initialize logging";
        return false;
    }
    
    // ModbusTcpMasterManager加载配置文件
    if (!ModbusTcpMasterManager::instance().loadConfig(AppConfig::getInstance().getModbusConfigPath())) {
        qWarning() << "ModbusTcpMasterConfig.xml 配置文件加载失败，使用默认配置";
    }

    // 初始化日志数据库（所有 LogDB::*LogDBCon 的入口）
    if (!LogDB::DatabaseManager::instance().initialize(AppConfig::getInstance().getDatabaseDir())) {
        qWarning() << "LogDB::DatabaseManager 初始化失败，日志数据库不可用";
    }

    // 初始化共享数据
    getSharedData();

    // 初始化并启动调度器线程
    initScheduler();

    s_initialized = true;
    
    qDebug() << "Application initialized successfully";
    qDebug() << "App Name:" << getAppName();
    qDebug() << "App Version:" << getAppVersion();
    qDebug() << "Config Dir:" << getConfig()->getConfigDir();
    qDebug() << "Debug Log Dir:" << getConfig()->getDebugLogDir();
    qDebug() << "User Log Dir:" << getConfig()->getUserLogDir();
    
    return true;
}

void App::cleanup()
{
    qDebug() << "Cleaning up application...";

    // 优雅关闭 Modbus 主控池（停止所有 Master、quit/wait 工作线程）
    // 必须在 QApplication 销毁前、日志关闭前执行
    ModbusTcpMasterManager::instance().shutdown();

    // 关闭日志数据库（必须在 spdlog 关闭前，避免 worker 线程里的日志调用 use-after-free）
    LogDB::DatabaseManager::instance().cleanup();

    // 关闭日志系统
    if (s_logger) {
        s_logger->shutdown();
        delete s_logger;
        s_logger = nullptr;
    }
    
    // 释放单实例共享内存
    QtHelper::releaseInstance();
    
    // 保存配置
    AppConfig::getInstance().reload();
    
    s_initialized = false;
}

bool App::initializeLogging()
{
    // 创建并初始化日志管理器
    if (!s_logger) {
        s_logger = new AppLogger();
    }
    
    // 初始化日志系统（使用调试日志目录）
    if (!s_logger->initialize(getConfig()->getDebugLogDir())) {
        return false;
    }

    // 注册崩溃信号处理器
    s_logger->registerCrashHandler();
    
    return true;
}

QString App::getAppName()
{
    return AppConfig::getInstance().getAppName();
}

QString App::getAppVersion()
{
    return AppConfig::getInstance().getAppVersion();
}

QString App::getDisplayName()
{
    QString appName = getAppName();
    QString appVersion = getAppVersion();
    
    if (!appVersion.isEmpty()) {
        return appName + " V" + appVersion;
    }
    
    return appName;
}

void App::getSharedData()
{
    // 创建静态实例以触发 SharedData 的构造函数初始化
    static SharedData sharedDataInstance;
    qDebug() << "SharedData initialized successfully";
}

void App::initScheduler()
{
    // 调用 SharedData 的调度器初始化方法
    SharedData::initScheduler();
}

void App::onAboutToQuit()
{
    // 此处只负责停后台任务（aboutToQuit 触发时 a.exec() 尚未返回，widgets 还未销毁）
    // 必须在 UI 析构前完成，防止信号发往已销毁的控件
    // 日志系统此时仍可用，stop() 内部的日志调用是安全的
    // ⚠ 禁止在此调用 App::cleanup()：会过早销毁 spdlog 线程池，
    //    导致 Scheduler::stop() 及后续 widget 析构里的日志调用 use-after-free
    Scheduler::instance()->stop();
}
