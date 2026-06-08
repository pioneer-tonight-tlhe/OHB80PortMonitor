#include "foupofohbinfo.h"

FoupOfOHBInfo::FoupOfOHBInfo()
    : m_qrCode(""),
    m_portId(0),
    m_ip("169.254.173.207"),
    m_port(8899),
    m_inletPressure(0),
    m_negativePressure(0),
    m_outletPressure(0),
    m_inletFlow(0),
    m_temperature(0),
    m_RH(0),
    m_startTime(QTime::currentTime()),
    m_purgeTimeSec(0),
    m_idleWorkingTimeSec(0),
    m_foupIn(false),
    m_oldFoupIn(false),
    m_idlePurgeEnabled(false),
    m_hasAlarm(false),
    m_alarmId(""),
    m_idleState(IdleState::Stopped),
    m_enable(true)
{

}

FoupOfOHBInfo::FoupOfOHBInfo(const FoupOfOHBInfo &other)
    : m_qrCode(other.m_qrCode),
      m_portId(other.m_portId),
      m_ip(other.m_ip),
      m_port(other.m_port),
      m_inletPressure(other.m_inletPressure),
      m_negativePressure(other.m_negativePressure),
      m_outletPressure(other.m_outletPressure),
      m_inletFlow(other.m_inletFlow),
      m_temperature(other.m_temperature),
      m_RH(other.m_RH),
      m_startTime(other.m_startTime),
      m_purgeTimeSec(other.m_purgeTimeSec),
      m_idleWorkingTimeSec(other.m_idleWorkingTimeSec),
      m_foupIn(other.m_foupIn),
      m_oldFoupIn(other.m_oldFoupIn),
      m_idlePurgeEnabled(other.m_idlePurgeEnabled),
      m_hasAlarm(other.m_hasAlarm),
      m_alarmId(other.m_alarmId),
      m_idleState(other.m_idleState),
      m_enable(other.m_enable)
{
}

FoupOfOHBInfo &FoupOfOHBInfo::operator=(const FoupOfOHBInfo &other)
{
    if (this != &other)
    {
        m_qrCode = other.m_qrCode;
        m_portId = other.m_portId;
        m_ip = other.m_ip;
        m_port = other.m_port;
        m_inletPressure = other.m_inletPressure;
        m_negativePressure = other.m_negativePressure;
        m_outletPressure = other.m_outletPressure;
        m_inletFlow = other.m_inletFlow;
        m_temperature = other.m_temperature;
        m_RH = other.m_RH;
        m_startTime = other.m_startTime;
        m_purgeTimeSec = other.m_purgeTimeSec;
        m_idleWorkingTimeSec = other.m_idleWorkingTimeSec;
        m_foupIn = other.m_foupIn;
        m_oldFoupIn = other.m_oldFoupIn;
        m_idlePurgeEnabled = other.m_idlePurgeEnabled;
        m_hasAlarm = other.m_hasAlarm;
        m_alarmId = other.m_alarmId;
        m_idleState = other.m_idleState;
        m_enable = other.m_enable;
    }
    return *this;
}

bool FoupOfOHBInfo::isVisibel()
{
    if (m_qrCode.isEmpty() || m_portId <= 0)
        return false;
    return true;
}

QString FoupOfOHBInfo::qrCode() const
{
    return m_qrCode;
}

int FoupOfOHBInfo::portId() const
{
    return m_portId;
}

QString FoupOfOHBInfo::ip() const
{
    return m_ip;
}

quint16 FoupOfOHBInfo::port() const
{
    return m_port;
}

double FoupOfOHBInfo::inletPressure() const
{
    return m_inletPressure;
}

double FoupOfOHBInfo::negativePressure() const
{
    return m_negativePressure;
}

double FoupOfOHBInfo::outletPressure() const
{
    return m_outletPressure;
}

double FoupOfOHBInfo::inletFlow() const
{
    return m_inletFlow;
}

double FoupOfOHBInfo::temperature() const
{
    return m_temperature;
}

double FoupOfOHBInfo::RH() const
{
    return m_RH;
}

QTime FoupOfOHBInfo::startTime() const
{
    return m_startTime;
}

quint32 FoupOfOHBInfo::purgeTimeSec() const
{
    return m_purgeTimeSec;
}

quint16 FoupOfOHBInfo::idleWorkingTimeSec() const
{
    return m_idleWorkingTimeSec;
}

bool FoupOfOHBInfo::foupIn() const
{
    return m_foupIn;
}

bool FoupOfOHBInfo::oldFoupIn() const
{
    return m_oldFoupIn;
}

bool FoupOfOHBInfo::idlePurgeEnabled() const
{
    return m_idlePurgeEnabled;
}

bool FoupOfOHBInfo::hasAlarm() const
{
    return m_hasAlarm;
}

QString FoupOfOHBInfo::alarmId() const
{
    return m_alarmId;
}

IdleState FoupOfOHBInfo::idleState() const
{
    return m_idleState;
}

bool FoupOfOHBInfo::enable() const
{
    return m_enable;
}

VEFCMonitorInfo* FoupOfOHBInfo::vefcMonitorInfo() const
{
    return m_vefcMonitorInfo;
}
