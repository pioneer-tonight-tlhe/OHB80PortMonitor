/*******************************************************************************************
 * @file electriccabinetserialportworker.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class ElectricCabinetSerialPortWorker
 * @brief 负责在工作线程内执行电控柜串口连接、收发、超时和自动重连逻辑。
 *
 * 设计目标：
 *      1. 将 QSerialPort 及相关定时器约束在同一工作线程中运行。
 *      2. 统一处理帧间隔、命令超时和待发送帧，保证请求响应顺序清晰。
 *      3. 支持自动重连策略，降低外部模块对串口异常恢复细节的依赖。
 *******************************************************************************************/
#ifndef ELECTRICCABINETSERIALPORTWORKER_H
#define ELECTRICCABINETSERIALPORTWORKER_H

#include <QByteArray>
#include <QObject>
#include <QSerialPort>

class QTimer;

class ElectricCabinetSerialPortWorker : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit ElectricCabinetSerialPortWorker(QObject *parent = nullptr);
    ~ElectricCabinetSerialPortWorker() override;

    // ============================ 串口参数配置 ============================
    Q_INVOKABLE bool setPortName(const QString &portName);
    Q_INVOKABLE void setBaudRate(qint32 baudRate);
    Q_INVOKABLE void setDataBits(QSerialPort::DataBits dataBits);
    Q_INVOKABLE void setParity(QSerialPort::Parity parity);
    Q_INVOKABLE void setStopBits(QSerialPort::StopBits stopBits);
    Q_INVOKABLE void setFlowControl(QSerialPort::FlowControl flowControl);
    Q_INVOKABLE void setAutoReconnect(bool enable, int intervalMs);
    Q_INVOKABLE void setCommandTimeoutMs(int timeoutMs);
    Q_INVOKABLE void setInterFrameTimeoutMs(int timeoutMs);

    // ============================ 串口通信 ============================
    Q_INVOKABLE void connectPort();
    Q_INVOKABLE void disconnectPort();
    Q_INVOKABLE void sendFrame(const QByteArray &frame);
    Q_INVOKABLE void shutdown();

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
    // ---- 串口事件处理 ----
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onInterFrameTimeout();
    void onCommandTimeout();
    void tryReconnect();

private:
    // ---- 串口校验 ----
    static bool isValidComPortName(const QString &portName);

    // ---- 生命周期控制 ----
    bool openInternal();
    void closeInternal();
    void startReconnectTimerIfNeeded();
    void stopReconnectTimer();

    // ---- 串口通信 ----
    void flushPendingFrame();

private:
    // ---- 功能模块成员 ----
    QSerialPort *m_serial = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QTimer *m_interFrameTimer = nullptr;
    QTimer *m_commandTimer = nullptr;

    // ---- 串口参数 ----
    QString m_portName;
    qint32 m_baudRate = 115200;
    QSerialPort::DataBits m_dataBits = QSerialPort::Data8;
    QSerialPort::Parity m_parity = QSerialPort::NoParity;
    QSerialPort::StopBits m_stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl m_flowControl = QSerialPort::NoFlowControl;

    bool m_autoReconnect = false;
    int m_reconnectIntervalMs = 3000;
    int m_commandTimeoutMs = 1000;
    int m_interFrameTimeoutMs = 30;

    // ---- 通信状态 ----
    QByteArray m_rxBuffer;
    QByteArray m_pendingTxFrame;
    bool m_waitingResponse = false;
};

#endif // ELECTRICCABINETSERIALPORTWORKER_H
