#include "electriccabinetinfo.h"

ElectricCabinetInfo::ElectricCabinetInfo()
    : m_serialPortConnected(false)
    , m_fan1Running(false)
    , m_fan2Running(false)
    , m_redLightOn(false)
    , m_greenLightOn(false)
    , m_powerOn(false)
    , m_emergencyStop1Active(false)
    , m_emergencyStop2Active(false)
    , m_smokeAlarmActive(false)
    , m_phaseAVoltage(0.0)
    , m_phaseBVoltage(0.0)
    , m_phaseACurrent(0.0)
    , m_phaseBCurrent(0.0)
    , m_temperature(0.0)
    , m_humidity(0.0)
{
}

ElectricCabinetInfo::ElectricCabinetInfo(const ElectricCabinetInfo& other)
    : m_serialPortConnected(other.m_serialPortConnected)
    , m_fan1Running(other.m_fan1Running)
    , m_fan2Running(other.m_fan2Running)
    , m_redLightOn(other.m_redLightOn)
    , m_greenLightOn(other.m_greenLightOn)
    , m_powerOn(other.m_powerOn)
    , m_emergencyStop1Active(other.m_emergencyStop1Active)
    , m_emergencyStop2Active(other.m_emergencyStop2Active)
    , m_smokeAlarmActive(other.m_smokeAlarmActive)
    , m_phaseAVoltage(other.m_phaseAVoltage)
    , m_phaseBVoltage(other.m_phaseBVoltage)
    , m_phaseACurrent(other.m_phaseACurrent)
    , m_phaseBCurrent(other.m_phaseBCurrent)
    , m_temperature(other.m_temperature)
    , m_humidity(other.m_humidity)
{
}

ElectricCabinetInfo& ElectricCabinetInfo::operator=(const ElectricCabinetInfo& other)
{
    if (this != &other) {
        m_serialPortConnected = other.m_serialPortConnected;
        m_fan1Running = other.m_fan1Running;
        m_fan2Running = other.m_fan2Running;
        m_redLightOn = other.m_redLightOn;
        m_greenLightOn = other.m_greenLightOn;
        m_powerOn = other.m_powerOn;
        m_emergencyStop1Active = other.m_emergencyStop1Active;
        m_emergencyStop2Active = other.m_emergencyStop2Active;
        m_smokeAlarmActive = other.m_smokeAlarmActive;
        m_phaseAVoltage = other.m_phaseAVoltage;
        m_phaseBVoltage = other.m_phaseBVoltage;
        m_phaseACurrent = other.m_phaseACurrent;
        m_phaseBCurrent = other.m_phaseBCurrent;
        m_temperature = other.m_temperature;
        m_humidity = other.m_humidity;
    }
    return *this;
}

bool ElectricCabinetInfo::serialPortConnected() const
{
    return m_serialPortConnected;
}

bool ElectricCabinetInfo::fan1Running() const
{
    return m_fan1Running;
}

bool ElectricCabinetInfo::fan2Running() const
{
    return m_fan2Running;
}

bool ElectricCabinetInfo::redLightOn() const
{
    return m_redLightOn;
}

bool ElectricCabinetInfo::greenLightOn() const
{
    return m_greenLightOn;
}

bool ElectricCabinetInfo::powerOn() const
{
    return m_powerOn;
}

bool ElectricCabinetInfo::emergencyStop1Active() const
{
    return m_emergencyStop1Active;
}

bool ElectricCabinetInfo::emergencyStop2Active() const
{
    return m_emergencyStop2Active;
}

bool ElectricCabinetInfo::smokeAlarmActive() const
{
    return m_smokeAlarmActive;
}

double ElectricCabinetInfo::phaseAVoltage() const
{
    return m_phaseAVoltage;
}

double ElectricCabinetInfo::phaseBVoltage() const
{
    return m_phaseBVoltage;
}

double ElectricCabinetInfo::phaseACurrent() const
{
    return m_phaseACurrent;
}

double ElectricCabinetInfo::phaseBCurrent() const
{
    return m_phaseBCurrent;
}

double ElectricCabinetInfo::temperature() const
{
    return m_temperature;
}

double ElectricCabinetInfo::humidity() const
{
    return m_humidity;
}
