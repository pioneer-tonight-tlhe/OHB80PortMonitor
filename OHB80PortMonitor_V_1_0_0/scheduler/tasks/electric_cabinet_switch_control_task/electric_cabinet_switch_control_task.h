/*******************************************************************************************
 * @file electric_cabinet_switch_control_task.h
 * @author Simon <工号：13> 2026-06-28
 *
 * @class ElectricCabinetSwitchControlTask
 * @brief 负责向电控柜下发风扇、指示灯、电源等开关量控制帧并校验设备回显。
 *
 * 设计目标：
 *      1. 复用 SharedData 中唯一的电控柜串口控制器，避免重复创建串口连接。
 *      2. 按旧项目电控柜协议构建开关控制帧，并从设备回显中解析最新开关状态。
 *      3. 控制成功后将最新开关状态写回 ElectricCabinetInfo，供 UI 或其他业务模块读取。
 *******************************************************************************************/
#ifndef ELECTRIC_CABINET_SWITCH_CONTROL_TASK_H
#define ELECTRIC_CABINET_SWITCH_CONTROL_TASK_H

#include "../../scheduler_task.h"

#include <QByteArray>
#include <QList>
#include <QMetaObject>

class QTimer;
class ElectricCabinetInfo;
class ElectricCabinetSerialPortController;

class ElectricCabinetSwitchControlTask : public SchedulerTask
{
    Q_OBJECT

public:
    enum SwitchMask : quint8 {
        Fan1Mask = 0x01,
        Fan2Mask = 0x02,
        RedLightMask = 0x08,
        GreenLightMask = 0x10,
        PowerMask = 0x20
    };
    Q_ENUM(SwitchMask)

public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetSwitchControlTask(quint8 mask,
                                              bool on,
                                              QObject* parent = nullptr);
    ~ElectricCabinetSwitchControlTask() override;

    // ============================ 基类 SchedulerTask 接口 ============================
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("ElectricCabinetSwitchControlTask"); }

private:
    // ---- 串口控制器连接管理 ----
    ElectricCabinetSerialPortController* currentController() const;
    void connectController(ElectricCabinetSerialPortController* controller);
    void disconnectAll();

    // ---- 协议帧构建与状态解析 ----
    QByteArray buildControlFrame() const;
    quint8 currentSwitchStatusByte() const;
    static void updateInfoByStatusByte(ElectricCabinetInfo* info, quint8 statusByte);
    static QByteArray calcChecksum(const QByteArray& body);
    static QString frameToText(const QByteArray& frame);
    static QString statusByteText(quint8 statusByte);

    // ---- 任务结束处理 ----
    void finishWithResult(bool success, const QString& message);

private slots:
    // ---- 控制器信号处理 ----
    void onCommandResponseReceived(const QByteArray& frame);
    void onSelfTimeout();

private:
    // ---- 任务输入参数 ----
    const quint8 m_mask;
    const bool m_on;

    // ---- 任务运行状态 ----
    bool m_finished = false;
    QByteArray m_txFrame;
    QTimer* m_timeoutTimer = nullptr;
    QList<QMetaObject::Connection> m_connections;
};

#endif // ELECTRIC_CABINET_SWITCH_CONTROL_TASK_H
