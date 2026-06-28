/*******************************************************************************************
 * @file electriccabinetinfo.h
 * @author Simon <工号：13> 2026-06-28
 *
 * @class ElectricCabinetInfo
 * @brief 保存电控柜串口连接状态和最新属性监控值的共享数据类。
 *
 * 设计目标：
 *      1. 作为 SharedData 中的全局静态数据对象，供调度层直接写入最新电控柜属性。
 *      2. 参考 FoupOfOHBInfo 的数据类写法，只保存业务数据，不承担信号通知职责。
 *      3. 通过明确的 Getter/Setter 与字段注释表达每个属性含义，便于 UI 层后续读取。
 *******************************************************************************************/
#ifndef ELECTRICCABINETINFO_H
#define ELECTRICCABINETINFO_H

class ElectricCabinetInfo
{
public:
    // ============================ 构造函数 ============================
    ElectricCabinetInfo();
    ElectricCabinetInfo(const ElectricCabinetInfo& other);
    ElectricCabinetInfo& operator=(const ElectricCabinetInfo& other);
    ~ElectricCabinetInfo() = default;

    // ============================ Getter 方法 ============================
    bool serialPortConnected() const;
    bool fan1Running() const;
    bool fan2Running() const;
    bool redLightOn() const;
    bool greenLightOn() const;
    bool powerOn() const;
    bool emergencyStop1Active() const;
    bool emergencyStop2Active() const;
    bool smokeAlarmActive() const;
    double phaseAVoltage() const;
    double phaseBVoltage() const;
    double phaseACurrent() const;
    double phaseBCurrent() const;
    double temperature() const;
    double humidity() const;

    // ============================ Setter 方法 ============================
    void setSerialPortConnected(bool serialPortConnected) { m_serialPortConnected = serialPortConnected; }
    void setFan1Running(bool fan1Running) { m_fan1Running = fan1Running; }
    void setFan2Running(bool fan2Running) { m_fan2Running = fan2Running; }
    void setRedLightOn(bool redLightOn) { m_redLightOn = redLightOn; }
    void setGreenLightOn(bool greenLightOn) { m_greenLightOn = greenLightOn; }
    void setPowerOn(bool powerOn) { m_powerOn = powerOn; }
    void setEmergencyStop1Active(bool emergencyStop1Active) { m_emergencyStop1Active = emergencyStop1Active; }
    void setEmergencyStop2Active(bool emergencyStop2Active) { m_emergencyStop2Active = emergencyStop2Active; }
    void setSmokeAlarmActive(bool smokeAlarmActive) { m_smokeAlarmActive = smokeAlarmActive; }
    void setPhaseAVoltage(double phaseAVoltage) { m_phaseAVoltage = phaseAVoltage; }
    void setPhaseBVoltage(double phaseBVoltage) { m_phaseBVoltage = phaseBVoltage; }
    void setPhaseACurrent(double phaseACurrent) { m_phaseACurrent = phaseACurrent; }
    void setPhaseBCurrent(double phaseBCurrent) { m_phaseBCurrent = phaseBCurrent; }
    void setTemperature(double temperature) { m_temperature = temperature; }
    void setHumidity(double humidity) { m_humidity = humidity; }

private:
    bool m_serialPortConnected;      // 电控柜串口是否已连接，连接状态监听任务负责更新。

    bool m_fan1Running;              // 1 号风扇运行状态，true=运行，false=停止。
    bool m_fan2Running;              // 2 号风扇运行状态，true=运行，false=停止。
    bool m_redLightOn;               // 红色指示灯点亮状态，true=点亮，false=熄灭。
    bool m_greenLightOn;             // 绿色指示灯点亮状态，true=点亮，false=熄灭。
    bool m_powerOn;                  // 电控柜电源输出状态，true=已上电，false=未上电。

    bool m_emergencyStop1Active;     // 1 号急停输入状态，true=急停触发，false=未触发。
    bool m_emergencyStop2Active;     // 2 号急停输入状态，true=急停触发，false=未触发。
    bool m_smokeAlarmActive;         // 烟雾报警输入状态，true=报警触发，false=未触发。

    double m_phaseAVoltage;          // A 相电压值，单位 V。
    double m_phaseBVoltage;          // B 相电压值，单位 V。
    double m_phaseACurrent;          // A 相电流值，单位 A。
    double m_phaseBCurrent;          // B 相电流值，单位 A。
    double m_temperature;            // 电控柜温度值，单位摄氏度。
    double m_humidity;               // 电控柜湿度值，单位 %RH。
};

#endif // ELECTRICCABINETINFO_H
