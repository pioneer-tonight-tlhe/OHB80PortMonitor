/*******************************************************************************************
 * @file electric_cabinet_set_temp_humi_task.h
 * @author Simon <工号：13> 2026-06-28
 *
 * @class ElectricCabinetSetTempHumiTask
 * @brief 负责向电控柜下发温湿度阈值设置帧并校验设备回显。
 *
 * 设计目标：
 *      1. 复用 SharedData 中唯一的电控柜串口控制器，避免重复创建串口连接。
 *      2. 按旧项目电控柜协议构建设置帧、校验回显帧并处理任务超时。
 *      3. 设置成功后将设备确认的阈值写回 electric_cabinet_serial_port.ini。
 *******************************************************************************************/
#ifndef ELECTRIC_CABINET_SET_TEMP_HUMI_TASK_H
#define ELECTRIC_CABINET_SET_TEMP_HUMI_TASK_H

#include "../../scheduler_task.h"

#include <QByteArray>
#include <QList>
#include <QMetaObject>

class QTimer;
class ElectricCabinetSerialPortController;

class ElectricCabinetSetTempHumiTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetSetTempHumiTask(double tempMax,
                                            double humiMax,
                                            QObject* parent = nullptr);
    ~ElectricCabinetSetTempHumiTask() override;

    // ============================ 基类 SchedulerTask 接口 ============================
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("ElectricCabinetSetTempHumiTask"); }

private:
    // ---- 串口控制器连接管理 ----
    ElectricCabinetSerialPortController* currentController() const;
    void connectController(ElectricCabinetSerialPortController* controller);
    void disconnectAll();

    // ---- 协议帧构建与日志文本转换 ----
    QByteArray buildFrame() const;
    static QByteArray calcChecksum(const QByteArray& body);
    static QString frameToText(const QByteArray& frame);

    // ---- 任务结束处理 ----
    void finishWithResult(bool success, const QString& message);

private slots:
    // ---- 控制器信号处理 ----
    void onCommandResponseReceived(const QByteArray& frame);
    void onSelfTimeout();

private:
    // ---- 任务输入参数 ----
    const double m_tempMax;
    const double m_humiMax;

    // ---- 任务运行状态 ----
    bool m_finished = false;
    QByteArray m_txFrame;
    QTimer* m_timeoutTimer = nullptr;
    QList<QMetaObject::Connection> m_connections;
};

#endif // ELECTRIC_CABINET_SET_TEMP_HUMI_TASK_H
