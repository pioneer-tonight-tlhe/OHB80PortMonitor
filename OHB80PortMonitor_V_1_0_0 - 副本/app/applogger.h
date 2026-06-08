#ifndef APPLOGGER_H
#define APPLOGGER_H

#include <QString>

class AppLogger
{
public:
    AppLogger();
    ~AppLogger();
    
    // 初始化日志系统
    bool initialize(const QString& logDir);
    
    // 关闭日志系统
    void shutdown();
    
    // 注册崩溃信号处理器
    void registerCrashHandler();
    
    // 崩溃信号处理器（静态方法，用于信号处理）
    static void crashHandler(int sig);
    
    // 获取日志目录
    QString getLogDir() const { return m_logDir; }
    
    // 日志路径静态方法（简短命名）
    // 获取 ModbusMaster 日志路径: modbustcpmaster/deviceid_{id}
    static QString ModbusMasterLoggerPath(const QString& id);

    // 获取天车轨道布局日志路径: cranemap
    static QString CraneMapLoggerPath();

    // 获取系统日志路径: system（UI层、调度层、数据层完整日志）
    static QString SystemLoggerPath();

    // 获取 SH85 自检日志路径: sh85selfcheck/{id}
    static QString SH85SelfCheckLoggerPath(const QString& id);

private:
    bool m_initialized;
    QString m_logDir;           // 日志目录
};

#endif // APPLOGGER_H
