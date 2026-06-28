/*******************************************************************************************
 * @file electric_cabinet_property_monitor_task.h
 * @author Simon <工号：13> 2026-06-28
 *
 * @class ElectricCabinetPropertyMonitorTask
 * @brief 负责常驻轮询电控柜属性帧并更新共享状态快照。
 *
 * 设计目标：
 *      1. 复用 SharedData 中唯一的电控柜串口控制器，避免调度层重复创建串口连接。
 *      2. 按配置中的轮询参数发送状态读取帧，并统一解析电控柜属性数据。
 *      3. 将解析后的属性快照写入 ElectricCabinetInfo，供 UI 或其他业务模块读取。
 *******************************************************************************************/
#ifndef ELECTRIC_CABINET_PROPERTY_MONITOR_TASK_H
#define ELECTRIC_CABINET_PROPERTY_MONITOR_TASK_H

#include "../../scheduler_task.h"
#include "electric_cabinet_property_monitor_task_logger.h"
#include "electriccabinetinfo.h"

#include <QByteArray>
#include <QList>
#include <QMetaObject>
#include <QString>

class QTimer;
class ElectricCabinetSerialPortController;
struct ElectricCabinetPropertyMonitorSettings;

class ElectricCabinetPropertyMonitorTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetPropertyMonitorTask(QObject* parent = nullptr);
    ~ElectricCabinetPropertyMonitorTask() override;

    // ============================ 基类 SchedulerTask 接口 ============================
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("ElectricCabinetPropertyMonitorTask"); }
    bool isPersistent() const override { return true; }

private:
    // ---- 生命周期与连接管理 ----
    void connectController(ElectricCabinetSerialPortController* controller);
    void disconnectAll();
    void initializeByConfig(const ElectricCabinetPropertyMonitorSettings& settings);

    // ---- 轮询控制 ----
    void scheduleNextPoll(int intervalMs);
    void sendStatusRequest();

    // ---- 协议解析 ----
    bool parseStatusFrame(const QByteArray& frame, ElectricCabinetInfo* info) const;
    static double readUInt16Scaled(const QByteArray& frame, int offset);
    static QString frameToText(const QByteArray& frame);
    static QByteArray normalizedRequestFrame(const QString& frameHex);
    QString propertyInfoText(const ElectricCabinetInfo& info) const;

private slots:
    // ---- 控制器信号处理 ----
    void onCommandResponseReceived(const QByteArray& frame);
    void onCommandTimeout();
    void onCommandSendFailed(const QString& message);
    void onPollTimer();

private:
    // ---- 静态常量 ----
    static const int DefaultPollIntervalMs;
    static const int DefaultRetryIntervalMs;
    static const int ExpectedResponseLength;

    // ---- 功能模块成员 ----
    ElectricCabinetPropertyMonitorTaskLogger* m_logger = nullptr;
    QTimer* m_pollTimer = nullptr;
    QList<QMetaObject::Connection> m_connections;

    // ---- 任务运行状态 ----
    bool m_running = false;
    bool m_waitingResponse = false;
    int m_pollIntervalMs = 1000;
    int m_retryIntervalMs = 3000;
    QByteArray m_statusRequestFrame;
};

#endif // ELECTRIC_CABINET_PROPERTY_MONITOR_TASK_H
