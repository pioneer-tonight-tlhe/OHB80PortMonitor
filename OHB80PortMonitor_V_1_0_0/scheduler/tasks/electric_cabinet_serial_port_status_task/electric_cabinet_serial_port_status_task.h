/*******************************************************************************************
 * @file electric_cabinet_serial_port_status_task.h
 * @author Simon <工号：13> 2026-06-28
 *
 * @class ElectricCabinetSerialPortStatusTask
 * @brief 负责常驻监听电控柜串口连接状态，并在状态变化时发出信号与告警。
 *
 * 设计目标：
 *      1. 以调度任务形式托管电控柜串口状态监听，保持任务常驻运行。
 *      2. 在串口连接状态切换时统一收口日志、告警与对外通知逻辑。
 *      3. 将控制器配置与信号绑定封装在任务内部，降低上层模块耦合。
 *******************************************************************************************/
#ifndef ELECTRIC_CABINET_SERIAL_PORT_STATUS_TASK_H
#define ELECTRIC_CABINET_SERIAL_PORT_STATUS_TASK_H

#include "../../scheduler_task.h"
#include "electric_cabinet_serial_port_status_task_logger.h"

#include <QList>
#include <QMetaObject>
#include <QString>

class ElectricCabinetSerialPortController;
struct ElectricCabinetSerialPortSettings;

class ElectricCabinetSerialPortStatusTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetSerialPortStatusTask(QObject* parent = nullptr);
    ~ElectricCabinetSerialPortStatusTask() override;

    // ============================ 基类 SchedulerTask 接口 ============================
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("ElectricCabinetSerialPortStatusTask"); }
    bool isPersistent() const override { return true; }

signals:
    // ---- 串口连接状态信号 ----
    void serialConnectionChanged(bool connected, const QString& reason);

private slots:
    // ---- 控制器信号处理 ----
    void onConnectedChanged(bool connected);
    void onPortError(const QString& message);
    void onCommandSendFailed(const QString& message);

private:
    // ---- 生命周期与连接管理 ----
    void disconnectAll();
    void initializeController(ElectricCabinetSerialPortController* controller,
                              const ElectricCabinetSerialPortSettings& settings);

    // ---- 状态收口与告警处理 ----
    void applyConnectionState(bool connected, const QString& reason);
    void submitDisconnectedAlarm(const QString& reason);
    void resolveDisconnectedAlarm(const QString& reason);

private:
    // ---- 静态常量 ----
    static const QString kAlarmSourceIdentifier;

    // ---- 功能模块成员 ----
    ElectricCabinetSerialPortStatusTaskLogger* m_logger = nullptr;
    QList<QMetaObject::Connection> m_connections;

    // ---- 任务运行状态 ----
    bool m_stopped = false;
    bool m_hasKnownStatus = false;
    bool m_lastConnected = false;
    bool m_disconnectedAlarmReported = false;
    QString m_portName;
};

#endif // ELECTRIC_CABINET_SERIAL_PORT_STATUS_TASK_H
