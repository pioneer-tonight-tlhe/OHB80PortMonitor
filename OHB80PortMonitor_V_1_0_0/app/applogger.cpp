#include "applogger.h"
#include "loggermanager.h"
#include <csignal>
#include <QDebug>

AppLogger::AppLogger()
    : m_initialized(false)
{
}

AppLogger::~AppLogger()
{
    shutdown();
}

bool AppLogger::initialize(const QString& logDir)
{
    if (m_initialized) {
        return true;
    }
    
    // 保存日志目录
    m_logDir = logDir;
    
    qDebug() << "Initializing logger system...";
    
    auto logger_mgr = LoggerManager::getInstance();
    
    // 设置日志根目录
    logger_mgr->set_root_dir(m_logDir.toStdString());
    
    // 设置日志回滚参数：单文件10MB，保留5个文件
    logger_mgr->set_rotation(10 * 1024 * 1024, 5);
    
    // 设置日志保留天数：7天
    logger_mgr->set_retention_days(7);
    
    // 启用自动trace日志
    logger_mgr->enable_auto_trace(true);
    
    // 启动自动分离warn日志
    logger_mgr->enable_warn_error_split(true);
    
    // 记录系统启动日志
    logger_mgr->log("system", Level::INFO, "日志系统初始化完成");
    
    m_initialized = true;
    
    qDebug() << "Logger system initialized successfully";
    qDebug() << "Log directory:" << m_logDir;
    
    return true;
}

void AppLogger::shutdown()
{
    if (!m_initialized) {
        return;
    }
    
    qDebug() << "Shutting down logger system...";
    
    LoggerManager::getInstance()->shutdown();
    
    m_initialized = false;
}

void AppLogger::registerCrashHandler()
{
    std::signal(SIGSEGV, AppLogger::crashHandler);
    std::signal(SIGABRT, AppLogger::crashHandler);
    std::signal(SIGFPE, AppLogger::crashHandler);
    std::signal(SIGILL, AppLogger::crashHandler);
    
    qDebug() << "Crash handlers registered";
}

void AppLogger::crashHandler(int sig)
{
    const char *sigName = "UNKNOWN";
    switch (sig) {
    case SIGSEGV: sigName = "SIGSEGV (段错误/非法内存访问)"; break;
    case SIGABRT: sigName = "SIGABRT (异常终止)"; break;
    case SIGFPE:  sigName = "SIGFPE (浮点异常)"; break;
    case SIGILL:  sigName = "SIGILL (非法指令)"; break;
    }
    
    LoggerManager::getInstance()->log("system", Level::ERROR,
                                  "[CRASH] 程序崩溃! 信号: {} ({})", sigName, sig);
    LoggerManager::getInstance()->shutdown();
    
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}

QString AppLogger::ModbusMasterLoggerPath(const QString& id)
{
    return QString("data/modbustcpmaster/deviceid_%1").arg(id);
}

QString AppLogger::CraneMapLoggerPath()
{
    return QString("ui/cranemap");
}

QString AppLogger::SystemLoggerPath()
{
    return QString("system");
}

QString AppLogger::SH85SelfCheckLoggerPath(const QString& id)
{
    return QString("sh85selfcheck/%1").arg(id);
}
