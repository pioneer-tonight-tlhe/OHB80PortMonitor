#ifndef MODBUSLOGGER_H
#define MODBUSLOGGER_H

#include "app/applogger.h"
#include "ilogger.h"

#include <QString>

namespace ModbusLogger {

inline QString format(const QString& module, const QString& action, const QString& message)
{
    return QString("[data][%1][%2]：%3").arg(module, action, message);
}

inline QString format(const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    return QString("[data][%1][%2][%3]：%4").arg(module, subModule, action, message);
}

inline QString deviceMessage(const QString& masterId, const QString& message)
{
    return QString("设备ID=%1 %2").arg(masterId, message);
}

inline ILogger system()
{
    return ILogger(AppLogger::SystemLoggerPath().toStdString());
}

inline ILogger master(const QString& masterId)
{
    return ILogger(AppLogger::ModbusMasterLoggerPath(masterId).toStdString());
}

inline ILogger sh85SelfCheck(const QString& masterId)
{
    return ILogger(AppLogger::SH85SelfCheckLoggerPath(masterId).toStdString());
}

inline void systemTrace(const QString& message)
{
    system().trace(message.toStdString());
}

inline void systemDebug(const QString& message)
{
    system().debug(message.toStdString());
}

inline void systemInfo(const QString& message)
{
    system().info(message.toStdString());
}

inline void systemWarn(const QString& message)
{
    system().warn(message.toStdString());
}

inline void systemError(const QString& message)
{
    system().error(message.toStdString());
}

inline void systemCritical(const QString& message)
{
    system().critical(message.toStdString());
}

inline void systemTrace(const QString& module, const QString& action, const QString& message)
{
    systemTrace(format(module, action, message));
}

inline void systemDebug(const QString& module, const QString& action, const QString& message)
{
    systemDebug(format(module, action, message));
}

inline void systemInfo(const QString& module, const QString& action, const QString& message)
{
    systemInfo(format(module, action, message));
}

inline void systemWarn(const QString& module, const QString& action, const QString& message)
{
    systemWarn(format(module, action, message));
}

inline void systemError(const QString& module, const QString& action, const QString& message)
{
    systemError(format(module, action, message));
}

inline void systemCritical(const QString& module, const QString& action, const QString& message)
{
    systemCritical(format(module, action, message));
}

inline void systemTrace(const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    systemTrace(format(module, subModule, action, message));
}

inline void systemDebug(const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    systemDebug(format(module, subModule, action, message));
}

inline void systemInfo(const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    systemInfo(format(module, subModule, action, message));
}

inline void systemWarn(const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    systemWarn(format(module, subModule, action, message));
}

inline void systemError(const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    systemError(format(module, subModule, action, message));
}

inline void systemCritical(const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    systemCritical(format(module, subModule, action, message));
}

inline void masterTrace(const QString& masterId, const QString& message)
{
    master(masterId).trace(message.toStdString());
}

inline void masterDebug(const QString& masterId, const QString& message)
{
    master(masterId).debug(message.toStdString());
}

inline void masterInfo(const QString& masterId, const QString& message)
{
    master(masterId).info(message.toStdString());
}

inline void masterWarn(const QString& masterId, const QString& message)
{
    master(masterId).warn(message.toStdString());
}

inline void masterError(const QString& masterId, const QString& message)
{
    master(masterId).error(message.toStdString());
}

inline void masterCritical(const QString& masterId, const QString& message)
{
    master(masterId).critical(message.toStdString());
}

inline void masterTrace(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    masterTrace(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void masterDebug(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    masterDebug(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void masterInfo(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    masterInfo(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void masterWarn(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    masterWarn(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void masterError(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    masterError(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void masterCritical(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    masterCritical(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void masterTrace(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    masterTrace(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void masterDebug(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    masterDebug(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void masterInfo(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    masterInfo(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void masterWarn(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    masterWarn(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void masterError(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    masterError(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void masterCritical(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    masterCritical(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckInfo(const QString& masterId, const QString& message)
{
    sh85SelfCheck(masterId).info(message.toStdString());
}

inline void sh85SelfCheckWarn(const QString& masterId, const QString& message)
{
    sh85SelfCheck(masterId).warn(message.toStdString());
}

inline void sh85SelfCheckError(const QString& masterId, const QString& message)
{
    sh85SelfCheck(masterId).error(message.toStdString());
}

inline void sh85SelfCheckCritical(const QString& masterId, const QString& message)
{
    sh85SelfCheck(masterId).critical(message.toStdString());
}

inline void sh85SelfCheckInfo(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    sh85SelfCheckInfo(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckWarn(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    sh85SelfCheckWarn(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckError(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    sh85SelfCheckError(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckCritical(const QString& masterId, const QString& module, const QString& action, const QString& message)
{
    sh85SelfCheckCritical(masterId, format(module, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckInfo(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    sh85SelfCheckInfo(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckWarn(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    sh85SelfCheckWarn(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckError(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    sh85SelfCheckError(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

inline void sh85SelfCheckCritical(const QString& masterId, const QString& module, const QString& subModule, const QString& action, const QString& message)
{
    sh85SelfCheckCritical(masterId, format(module, subModule, action, deviceMessage(masterId, message)));
}

} // namespace ModbusLogger

#endif // MODBUSLOGGER_H
