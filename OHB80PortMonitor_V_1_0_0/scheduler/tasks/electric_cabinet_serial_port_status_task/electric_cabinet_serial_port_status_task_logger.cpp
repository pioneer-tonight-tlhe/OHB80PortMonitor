#include "electric_cabinet_serial_port_status_task_logger.h"

ElectricCabinetSerialPortStatusTaskLogger::ElectricCabinetSerialPortStatusTaskLogger(bool enable)
    : m_logger("scheduler/electric_cabinet_serial_port_status_task/task/task")
{
    m_logger.set_enable(enable);
}

void ElectricCabinetSerialPortStatusTaskLogger::info(const QString& action, const QString& message)
{
    m_logger.info(buildMessage(action, message).toStdString());
}

void ElectricCabinetSerialPortStatusTaskLogger::warn(const QString& action, const QString& message)
{
    m_logger.warn(buildMessage(action, message).toStdString());
}

void ElectricCabinetSerialPortStatusTaskLogger::error(const QString& action, const QString& message)
{
    m_logger.error(buildMessage(action, message).toStdString());
}

QString ElectricCabinetSerialPortStatusTaskLogger::buildMessage(const QString& action, const QString& message)
{
    return QString("[scheduler][ElectricCabinetSerialPortStatusTask][%1]: %2").arg(action, message);
}
