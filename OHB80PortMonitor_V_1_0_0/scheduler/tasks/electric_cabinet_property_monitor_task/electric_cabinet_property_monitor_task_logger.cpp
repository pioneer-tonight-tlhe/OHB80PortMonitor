#include "electric_cabinet_property_monitor_task_logger.h"

ElectricCabinetPropertyMonitorTaskLogger::ElectricCabinetPropertyMonitorTaskLogger(bool enable)
    : m_logger("scheduler/electric_cabinet_property_monitor_task/task/task")
{
    m_logger.set_enable(enable);
}

void ElectricCabinetPropertyMonitorTaskLogger::debug(const QString& action, const QString& message)
{
    m_logger.debug(buildMessage(action, message).toStdString());
}

void ElectricCabinetPropertyMonitorTaskLogger::info(const QString& action, const QString& message)
{
    m_logger.info(buildMessage(action, message).toStdString());
}

void ElectricCabinetPropertyMonitorTaskLogger::warn(const QString& action, const QString& message)
{
    m_logger.warn(buildMessage(action, message).toStdString());
}

void ElectricCabinetPropertyMonitorTaskLogger::error(const QString& action, const QString& message)
{
    m_logger.error(buildMessage(action, message).toStdString());
}

QString ElectricCabinetPropertyMonitorTaskLogger::buildMessage(const QString& action,
                                                               const QString& message)
{
    return QString("[scheduler][ElectricCabinetPropertyMonitorTask][%1]: %2").arg(action, message);
}
