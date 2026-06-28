#include "electriccabinetserialportworker.h"

#include <QRegularExpression>
#include <QTimer>

ElectricCabinetSerialPortWorker::ElectricCabinetSerialPortWorker(QObject *parent)
    : QObject(parent)
    , m_serial(new QSerialPort(this))
    , m_reconnectTimer(new QTimer(this))
    , m_interFrameTimer(new QTimer(this))
    , m_commandTimer(new QTimer(this))
{
    m_reconnectTimer->setSingleShot(true);
    m_interFrameTimer->setSingleShot(true);
    m_commandTimer->setSingleShot(true);

    connect(m_serial, &QSerialPort::readyRead, this, &ElectricCabinetSerialPortWorker::onReadyRead);
    connect(m_serial, &QSerialPort::errorOccurred, this, &ElectricCabinetSerialPortWorker::onErrorOccurred);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ElectricCabinetSerialPortWorker::tryReconnect);
    connect(m_interFrameTimer, &QTimer::timeout, this, &ElectricCabinetSerialPortWorker::onInterFrameTimeout);
    connect(m_commandTimer, &QTimer::timeout, this, &ElectricCabinetSerialPortWorker::onCommandTimeout);
}

ElectricCabinetSerialPortWorker::~ElectricCabinetSerialPortWorker()
{
    shutdown();
}

bool ElectricCabinetSerialPortWorker::setPortName(const QString &portName)
{
    const QString normalized = portName.trimmed().toUpper();
    if (!isValidComPortName(normalized)) {
        emit portError(QStringLiteral("Invalid port name: %1").arg(portName));
        return false;
    }
    if (m_serial->isOpen()) {
        emit portError(QStringLiteral("Cannot change port while connected"));
        return false;
    }
    m_portName = normalized;
    return true;
}

void ElectricCabinetSerialPortWorker::setBaudRate(qint32 baudRate)
{
    m_baudRate = baudRate;
    if (m_serial->isOpen()) {
        m_serial->setBaudRate(m_baudRate);
    }
}

void ElectricCabinetSerialPortWorker::setDataBits(QSerialPort::DataBits dataBits)
{
    m_dataBits = dataBits;
    if (m_serial->isOpen()) {
        m_serial->setDataBits(m_dataBits);
    }
}

void ElectricCabinetSerialPortWorker::setParity(QSerialPort::Parity parity)
{
    m_parity = parity;
    if (m_serial->isOpen()) {
        m_serial->setParity(m_parity);
    }
}

void ElectricCabinetSerialPortWorker::setStopBits(QSerialPort::StopBits stopBits)
{
    m_stopBits = stopBits;
    if (m_serial->isOpen()) {
        m_serial->setStopBits(m_stopBits);
    }
}

void ElectricCabinetSerialPortWorker::setFlowControl(QSerialPort::FlowControl flowControl)
{
    m_flowControl = flowControl;
    if (m_serial->isOpen()) {
        m_serial->setFlowControl(m_flowControl);
    }
}

void ElectricCabinetSerialPortWorker::setAutoReconnect(bool enable, int intervalMs)
{
    m_autoReconnect = enable;
    if (intervalMs > 0) {
        m_reconnectIntervalMs = intervalMs;
    }

    if (!m_autoReconnect) {
        stopReconnectTimer();
        return;
    }

    if (!m_serial->isOpen()) {
        startReconnectTimerIfNeeded();
    }
}

void ElectricCabinetSerialPortWorker::setCommandTimeoutMs(int timeoutMs)
{
    if (timeoutMs > 0) {
        m_commandTimeoutMs = timeoutMs;
    }
}

void ElectricCabinetSerialPortWorker::setInterFrameTimeoutMs(int timeoutMs)
{
    if (timeoutMs > 0) {
        m_interFrameTimeoutMs = timeoutMs;
    }
}

void ElectricCabinetSerialPortWorker::connectPort()
{
    if (m_serial->isOpen()) {
        emit connectedChanged(true);
        return;
    }
    if (!openInternal()) {
        startReconnectTimerIfNeeded();
    }
}

void ElectricCabinetSerialPortWorker::disconnectPort()
{
    m_autoReconnect = false;
    stopReconnectTimer();
    closeInternal();
}

void ElectricCabinetSerialPortWorker::sendFrame(const QByteArray &frame)
{
    if (m_waitingResponse) {
        m_pendingTxFrame = frame;
        return;
    }

    if (!m_serial->isOpen()) {
        const QString msg = QStringLiteral("Serial port is not connected");
        emit portError(msg);
        emit commandSendFailed(msg);
        startReconnectTimerIfNeeded();
        return;
    }

    m_rxBuffer.clear();
    m_waitingResponse = true;
    m_commandTimer->start(m_commandTimeoutMs);

    const qint64 bytes = m_serial->write(frame);
    if (bytes < 0) {
        const QString msg = m_serial->errorString();
        emit portError(msg);
        emit commandSendFailed(msg);
        m_commandTimer->stop();
        m_waitingResponse = false;
        return;
    }
    if (!m_serial->waitForBytesWritten(50)) {
        const QString msg = QStringLiteral("Write timeout");
        emit portError(msg);
        emit commandSendFailed(msg);
        m_commandTimer->stop();
        m_waitingResponse = false;
    }
}

void ElectricCabinetSerialPortWorker::shutdown()
{
    stopReconnectTimer();
    m_interFrameTimer->stop();
    m_commandTimer->stop();
    m_pendingTxFrame.clear();
    m_rxBuffer.clear();
    m_waitingResponse = false;
    closeInternal();
}

void ElectricCabinetSerialPortWorker::onReadyRead()
{
    const QByteArray data = m_serial->readAll();
    if (data.isEmpty()) {
        return;
    }

    m_rxBuffer.append(data);
    m_interFrameTimer->start(m_interFrameTimeoutMs);
}

void ElectricCabinetSerialPortWorker::onErrorOccurred(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    if (error == QSerialPort::ResourceError ||
        error == QSerialPort::DeviceNotFoundError ||
        error == QSerialPort::PermissionError) {
        const QString msg = m_serial->errorString();
        closeInternal();
        emit portError(msg);
        startReconnectTimerIfNeeded();
        return;
    }

    emit portError(m_serial->errorString());
}

void ElectricCabinetSerialPortWorker::onInterFrameTimeout()
{
    if (m_rxBuffer.isEmpty()) {
        return;
    }

    const QByteArray frame = m_rxBuffer;
    m_rxBuffer.clear();
    emit rawFrameReceived(frame);

    if (m_waitingResponse) {
        m_waitingResponse = false;
        m_commandTimer->stop();
        emit commandResponseReceived(frame);
        flushPendingFrame();
    }
}

void ElectricCabinetSerialPortWorker::onCommandTimeout()
{
    if (!m_waitingResponse) {
        return;
    }

    m_waitingResponse = false;
    m_rxBuffer.clear();
    emit commandTimeout();
    flushPendingFrame();
}

void ElectricCabinetSerialPortWorker::tryReconnect()
{
    if (!m_autoReconnect || m_serial->isOpen()) {
        return;
    }

    if (!openInternal()) {
        startReconnectTimerIfNeeded();
    }
}

bool ElectricCabinetSerialPortWorker::isValidComPortName(const QString &portName)
{
    static const QRegularExpression re(QStringLiteral("^COM([1-9]|[1-9][0-9]|[1-9][0-9][0-9])$"));
    return re.match(portName).hasMatch();
}

bool ElectricCabinetSerialPortWorker::openInternal()
{
    if (m_serial->isOpen()) {
        return true;
    }

    if (!isValidComPortName(m_portName)) {
        emit portError(QStringLiteral("Invalid port name: %1").arg(m_portName));
        return false;
    }

    m_serial->setPortName(m_portName);
    m_serial->setBaudRate(m_baudRate);
    m_serial->setDataBits(m_dataBits);
    m_serial->setParity(m_parity);
    m_serial->setStopBits(m_stopBits);
    m_serial->setFlowControl(m_flowControl);

    if (!m_serial->open(QIODevice::ReadWrite)) {
        emit connectedChanged(false);
        emit portError(m_serial->errorString());
        return false;
    }

    emit connectedChanged(true);
    return true;
}

void ElectricCabinetSerialPortWorker::closeInternal()
{
    const bool wasOpen = m_serial->isOpen();
    m_interFrameTimer->stop();
    m_commandTimer->stop();
    m_pendingTxFrame.clear();
    m_rxBuffer.clear();
    m_waitingResponse = false;

    if (wasOpen) {
        m_serial->close();
        emit connectedChanged(false);
    }
}

void ElectricCabinetSerialPortWorker::startReconnectTimerIfNeeded()
{
    if (!m_autoReconnect || m_reconnectTimer->isActive()) {
        return;
    }

    m_reconnectTimer->start(m_reconnectIntervalMs);
}

void ElectricCabinetSerialPortWorker::stopReconnectTimer()
{
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
}

void ElectricCabinetSerialPortWorker::flushPendingFrame()
{
    if (m_pendingTxFrame.isEmpty()) {
        return;
    }

    const QByteArray frame = m_pendingTxFrame;
    m_pendingTxFrame.clear();
    sendFrame(frame);
}
