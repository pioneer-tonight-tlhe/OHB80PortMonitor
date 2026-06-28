#include "electriccabinetserialportcontroller.h"

#include "electriccabinetserialportworker.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>

ElectricCabinetSerialPortController::ElectricCabinetSerialPortController(QObject *parent)
    : QObject(parent)
{
}

ElectricCabinetSerialPortController::~ElectricCabinetSerialPortController()
{
    stopThreadAndMoveWorkerToMainThread();
    delete m_worker;
    m_worker = nullptr;
}

void ElectricCabinetSerialPortController::start()
{
    if (m_started) {
        return;
    }

    static bool s_metaTypesRegistered = false;
    if (!s_metaTypesRegistered) {
        qRegisterMetaType<QSerialPort::DataBits>("QSerialPort::DataBits");
        qRegisterMetaType<QSerialPort::Parity>("QSerialPort::Parity");
        qRegisterMetaType<QSerialPort::StopBits>("QSerialPort::StopBits");
        qRegisterMetaType<QSerialPort::FlowControl>("QSerialPort::FlowControl");
        qRegisterMetaType<QSerialPort::SerialPortError>("QSerialPort::SerialPortError");
        s_metaTypesRegistered = true;
    }

    m_thread = new QThread();
    m_worker = new ElectricCabinetSerialPortWorker();
    m_worker->moveToThread(m_thread);

    connect(m_worker, &ElectricCabinetSerialPortWorker::connectedChanged,
            this, &ElectricCabinetSerialPortController::onWorkerConnectedChanged);
    connect(m_worker, &ElectricCabinetSerialPortWorker::portError,
            this, &ElectricCabinetSerialPortController::onWorkerPortError);
    connect(m_worker, &ElectricCabinetSerialPortWorker::rawFrameReceived,
            this, &ElectricCabinetSerialPortController::onWorkerRawFrameReceived);
    connect(m_worker, &ElectricCabinetSerialPortWorker::commandResponseReceived,
            this, &ElectricCabinetSerialPortController::onWorkerCommandResponseReceived);
    connect(m_worker, &ElectricCabinetSerialPortWorker::commandTimeout,
            this, &ElectricCabinetSerialPortController::onWorkerCommandTimeout);
    connect(m_worker, &ElectricCabinetSerialPortWorker::commandSendFailed,
            this, &ElectricCabinetSerialPortController::onWorkerCommandSendFailed);

    m_thread->setObjectName("ElectricCabinetSerialPortThread");
    m_thread->start();
    m_started = true;
}

void ElectricCabinetSerialPortController::setPortName(const QString &portName)
{
    ensureStarted();
    m_portName = portName.trimmed().toUpper();
    QMetaObject::invokeMethod(m_worker, "setPortName", Qt::QueuedConnection, Q_ARG(QString, m_portName));
}

void ElectricCabinetSerialPortController::setBaudRate(qint32 baudRate)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setBaudRate", Qt::QueuedConnection, Q_ARG(qint32, baudRate));
}

void ElectricCabinetSerialPortController::setDataBits(QSerialPort::DataBits dataBits)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setDataBits", Qt::QueuedConnection, Q_ARG(QSerialPort::DataBits, dataBits));
}

void ElectricCabinetSerialPortController::setParity(QSerialPort::Parity parity)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setParity", Qt::QueuedConnection, Q_ARG(QSerialPort::Parity, parity));
}

void ElectricCabinetSerialPortController::setStopBits(QSerialPort::StopBits stopBits)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setStopBits", Qt::QueuedConnection, Q_ARG(QSerialPort::StopBits, stopBits));
}

void ElectricCabinetSerialPortController::setFlowControl(QSerialPort::FlowControl flowControl)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setFlowControl", Qt::QueuedConnection, Q_ARG(QSerialPort::FlowControl, flowControl));
}

void ElectricCabinetSerialPortController::connectPort()
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "connectPort", Qt::QueuedConnection);
}

void ElectricCabinetSerialPortController::disconnectPort()
{
    if (!m_worker) {
        return;
    }

    QMetaObject::invokeMethod(m_worker, "disconnectPort", Qt::QueuedConnection);
}

void ElectricCabinetSerialPortController::setAutoReconnect(bool enable, int intervalMs)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setAutoReconnect", Qt::QueuedConnection,
                              Q_ARG(bool, enable), Q_ARG(int, intervalMs));
}

void ElectricCabinetSerialPortController::setCommandTimeoutMs(int timeoutMs)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setCommandTimeoutMs", Qt::QueuedConnection, Q_ARG(int, timeoutMs));
}

void ElectricCabinetSerialPortController::setInterFrameTimeoutMs(int timeoutMs)
{
    ensureStarted();
    QMetaObject::invokeMethod(m_worker, "setInterFrameTimeoutMs", Qt::QueuedConnection, Q_ARG(int, timeoutMs));
}

bool ElectricCabinetSerialPortController::isConnected() const
{
    return m_connected;
}

void ElectricCabinetSerialPortController::sendFrame(const QByteArray &frame)
{
    ensureStarted();
    qDebug() << "[ElectricCabinetSerialPortController] TX port=" << m_portName
             << "data=" << frame.toHex(' ').toUpper();
    QMetaObject::invokeMethod(m_worker, "sendFrame", Qt::QueuedConnection, Q_ARG(QByteArray, frame));
}

void ElectricCabinetSerialPortController::stopThreadAndMoveWorkerToMainThread()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            stopThreadAndMoveWorkerToMainThread();
        }, Qt::BlockingQueuedConnection);
        return;
    }

    if (!m_thread || !m_worker) {
        m_started = false;
        return;
    }

    QThread *mainThread = QCoreApplication::instance()->thread();
    QMetaObject::invokeMethod(m_worker, [this, mainThread]() {
        m_worker->shutdown();
        m_worker->setParent(nullptr);
        m_worker->moveToThread(mainThread);
    }, Qt::BlockingQueuedConnection);

    m_thread->quit();
    m_thread->wait();

    m_worker->setParent(this);
    delete m_thread;
    m_thread = nullptr;
    m_started = false;
}

void ElectricCabinetSerialPortController::onWorkerConnectedChanged(bool connected)
{
    m_connected = connected;
    emit connectedChanged(connected);
}

void ElectricCabinetSerialPortController::onWorkerPortError(const QString &message)
{
    qWarning() << "[ElectricCabinetSerialPortController] port error:" << message;
    emit portError(message);
}

void ElectricCabinetSerialPortController::onWorkerRawFrameReceived(const QByteArray &frame)
{
    emit rawFrameReceived(frame);
}

void ElectricCabinetSerialPortController::onWorkerCommandResponseReceived(const QByteArray &frame)
{
    qDebug() << "[ElectricCabinetSerialPortController] RX port=" << m_portName
             << "data=" << frame.toHex(' ').toUpper();
    emit commandResponseReceived(frame);
}

void ElectricCabinetSerialPortController::onWorkerCommandTimeout()
{
    emit commandTimeout();
}

void ElectricCabinetSerialPortController::onWorkerCommandSendFailed(const QString &message)
{
    emit commandSendFailed(message);
}

void ElectricCabinetSerialPortController::ensureStarted()
{
    if (!m_started) {
        start();
    }
}
