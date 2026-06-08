#ifndef FOUPOFOHBINFO_H
#define FOUPOFOHBINFO_H

#include <QTime>
#include <QString>

class VEFCMonitorInfo;

// Idle 状态枚举
enum class IdleState : int {
    Stopped = 0,   // 停止工作
    Preparing = 1, // 准备阶段
    Purging = 2,   // 吹气
    Idle = 3       // 空闲
};

class FoupOfOHBInfo
{
public:
    FoupOfOHBInfo();
    FoupOfOHBInfo(const FoupOfOHBInfo &other);
    FoupOfOHBInfo &operator=(const FoupOfOHBInfo &other);
    ~FoupOfOHBInfo() = default;

    bool isVisibel();

    // Getter 方法
    QString qrCode() const;
    int portId() const;
    QString ip() const;
    quint16 port() const;
    double inletPressure() const;
    double negativePressure() const;
    double outletPressure() const;
    double inletFlow() const;
    double temperature() const;
    double RH() const;
    QTime startTime() const;
    quint32 purgeTimeSec() const;
    quint16 idleWorkingTimeSec() const;
    bool foupIn() const;
    bool oldFoupIn() const;
    bool idlePurgeEnabled() const;
    bool hasAlarm() const;
    QString alarmId() const;
    IdleState idleState() const;
    bool enable() const;
    VEFCMonitorInfo* vefcMonitorInfo() const;

    // Setter 方法
    void setQrCode(const QString &qrCode) { m_qrCode = qrCode; }
    void setPortId(int portId) { m_portId = portId; }
    void setIp(const QString &ip) { m_ip = ip; }
    void setPort(quint16 port) { m_port = port; }
    void setInletPressure(double inletPressure) { m_inletPressure = inletPressure; }
    void setNegativePressure(double negativePressure) { m_negativePressure = negativePressure; }
    void setOutletPressure(double outletPressure) { m_outletPressure = outletPressure; }
    void setInletFlow(double inletFlow) { m_inletFlow = inletFlow; }
    void setTemperature(double temperature) { m_temperature = temperature; }
    void setRH(double RH) { m_RH = RH; }
    void setStartTime(const QTime &startTime) { m_startTime = startTime; }
    void setPurgeTimeSec(quint32 purgeTimeSec) { m_purgeTimeSec = purgeTimeSec; }
    void setIdleWorkingTimeSec(quint16 idleWorkingTimeSec) { m_idleWorkingTimeSec = idleWorkingTimeSec; }
    void setFoupIn(bool foupIn) { m_foupIn = foupIn; }
    void setOldFoupIn(bool oldFoupIn) { m_oldFoupIn = oldFoupIn; }
    void setIdlePurgeEnabled(bool idlePurgeEnabled) { m_idlePurgeEnabled = idlePurgeEnabled; }
    void setHasAlarm(bool hasAlarm) { m_hasAlarm = hasAlarm; }
    void setAlarmId(const QString &alarmId) { m_alarmId = alarmId; }
    void setIdleState(IdleState idleState) { m_idleState = idleState; }
    void setEnable(bool enable) { m_enable = enable; }
    void setVEFCMonitorInfo(VEFCMonitorInfo* vefcMonitorInfo) { m_vefcMonitorInfo = vefcMonitorInfo; }

private:
    QString m_qrCode;           // 二维码（设备标识）
    int m_portId;               // 端口 ID
    QString m_ip;               // IP 地址
    quint16 m_port;             // 端口号
    double m_inletPressure;         // 进气主路气压值 (CH_1, 0~10 Bar)
    double m_negativePressure;      // 负压值 (CH_2)
    double m_outletPressure;        // 出口压力（暂未使用）
    double m_inletFlow;             // 进气主路流量值 (CH_3, 4~200 L/Min)
    double m_temperature;           // 真空回路气体温度 (CH_5, 0~100℃)
    double m_RH;                    // 真空回路相对湿度 (CH_4, 0~100%)
    QTime m_startTime;              // 开始时间
    quint32 m_purgeTimeSec;         // FOUP IN 充气秒数 (CH_7<<16|CH_8)
    quint16 m_idleWorkingTimeSec;   // IdlePurge 工作计时时间（秒）
    bool m_foupIn;                  // FOUP 是否在位 (CH_6)
    bool m_oldFoupIn;               // FOUP 上一次在位状态
    bool m_idlePurgeEnabled;        // IdlePurge 使能状态 (0=关闭, 1=开启)
    bool m_hasAlarm;                // 警报状态
    QString m_alarmId;              // 警报 ID
    IdleState m_idleState;          // IdlePurge 状态 (0=空闲,1=准备,2=充气,3=充气间隔)
    bool m_enable;                  // 使能状态 (true=可用, false=不可用)
    VEFCMonitorInfo* m_vefcMonitorInfo = nullptr;  // VEFC 监控信息
};


#endif // FOUPOFOHBINFO_H
