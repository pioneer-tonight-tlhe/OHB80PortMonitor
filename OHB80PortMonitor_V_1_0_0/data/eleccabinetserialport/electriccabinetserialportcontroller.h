/*******************************************************************************************
 * @file electriccabinetserialportcontroller.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class ElectricCabinetSerialPortController
 * @brief 负责在主线程侧管理电控柜串口工作线程并对外提供串口收发接口。
 *
 * 设计目标：
 *      1. 将串口阻塞操作隔离到工作线程，避免影响业务线程响应。
 *      2. 统一转发串口连接状态、错误和命令响应信号，降低调用方与 Worker 的耦合。
 *      3. 在生命周期结束时安全停止工作线程并回收串口工作对象。
 *******************************************************************************************/
#ifndef ELECTRICCABINETSERIALPORTCONTROLLER_H
#define ELECTRICCABINETSERIALPORTCONTROLLER_H

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QThread>

class ElectricCabinetSerialPortWorker;

class ElectricCabinetSerialPortController : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetSerialPortController(QObject *parent = nullptr);
    ~ElectricCabinetSerialPortController() override;

    // ============================ 生命周期控制 ============================
    void start();
    void stopThreadAndMoveWorkerToMainThread();

    // ============================ 串口参数配置 ============================
    void setPortName(const QString &portName);
    void setBaudRate(qint32 baudRate);
    void setDataBits(QSerialPort::DataBits dataBits);
    void setParity(QSerialPort::Parity parity);
    void setStopBits(QSerialPort::StopBits stopBits);
    void setFlowControl(QSerialPort::FlowControl flowControl);
    void setAutoReconnect(bool enable, int intervalMs);
    void setCommandTimeoutMs(int timeoutMs);
    void setInterFrameTimeoutMs(int timeoutMs);

    // ============================ 串口通信 ============================
    void connectPort();
    void disconnectPort();
    bool isConnected() const;
    void sendFrame(const QByteArray &frame);

signals:
    // ---- 串口状态 ----
    void connectedChanged(bool connected);
    void portError(const QString &message);

    // ---- 串口通信 ----
    void rawFrameReceived(const QByteArray &frame);
    void commandResponseReceived(const QByteArray &frame);
    void commandTimeout();
    void commandSendFailed(const QString &message);

private slots:
    // ---- Worker 信号转发 ----
    void onWorkerConnectedChanged(bool connected);
    void onWorkerPortError(const QString &message);
    void onWorkerRawFrameReceived(const QByteArray &frame);
    void onWorkerCommandResponseReceived(const QByteArray &frame);
    void onWorkerCommandTimeout();
    void onWorkerCommandSendFailed(const QString &message);

private:
    // ---- 生命周期控制 ----
    void ensureStarted();

private:
    // ---- 功能模块成员 ----
    QThread *m_thread = nullptr;
    ElectricCabinetSerialPortWorker *m_worker = nullptr;

    // ---- 状态成员 ----
    bool m_started = false;
    bool m_connected = false;
    QString m_portName;
};

#endif // ELECTRICCABINETSERIALPORTCONTROLLER_H
