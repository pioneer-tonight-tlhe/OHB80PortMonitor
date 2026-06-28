#include "electric_cabinet_switch_control_task.h"

#include "app/shareddata.h"
#include "electriccabinetinfo.h"
#include "electriccabinetserialportconfig.h"
#include "electriccabinetserialportcontroller.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"

#include <QTimer>
#include <QtGlobal>

namespace {
constexpr quint8 kResponseHead = 0x03;
constexpr quint8 kSwitchControlRegister = 0x32;
constexpr int kExpectedResponseMinLength = 6;
}

ElectricCabinetSwitchControlTask::ElectricCabinetSwitchControlTask(quint8 mask,
                                                                   bool on,
                                                                   QObject* parent)
    : SchedulerTask(parent)
    , m_mask(mask)
    , m_on(on)
{
}

ElectricCabinetSwitchControlTask::~ElectricCabinetSwitchControlTask()
{
    disconnectAll();
}

void ElectricCabinetSwitchControlTask::start()
{
    disconnectAll();

    m_finished = false;
    m_txFrame.clear();
    setState(Running);

    ElectricCabinetSerialPortController* controller = currentController();
    if (!controller) {
        finishWithResult(false, QStringLiteral("ElectricCabinetSerialPortController is null"));
        return;
    }

    if (!controller->isConnected()) {
        finishWithResult(false, QStringLiteral("Electric cabinet serial port is not connected"));
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &ElectricCabinetSwitchControlTask::onSelfTimeout);
    }

    connectController(controller);

    m_txFrame = buildControlFrame();

    if (OperationDispatchTask* opTask = SharedData::getOperationDispatchTask()) {
        opTask->logMessage(
            QString("Electric cabinet switch control started: mask=0x%1, on=%2, tx=%3")
                .arg(QString("%1").arg(m_mask, 2, 16, QLatin1Char('0')).toUpper())
                .arg(m_on)
                .arg(frameToText(m_txFrame)));
    }

    controller->sendFrame(m_txFrame);

    const ElectricCabinetSwitchControlSettings settings =
        ElectricCabinetSerialPortConfig::getInstance().readSwitchControlSettings();
    m_timeoutTimer->start(qMax(1, settings.commandResponseTimeoutMs));

    emit progress(0, QStringLiteral("Electric cabinet switch control command sent"));
}

void ElectricCabinetSwitchControlTask::stop()
{
    if (m_finished) {
        return;
    }

    m_finished = true;
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("Electric cabinet switch control cancelled"));
}

ElectricCabinetSerialPortController* ElectricCabinetSwitchControlTask::currentController() const
{
    return SharedData::getElectricCabinetSerialPortController();
}

void ElectricCabinetSwitchControlTask::connectController(ElectricCabinetSerialPortController* controller)
{
    if (!controller) {
        return;
    }

    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::commandResponseReceived,
                                 this, &ElectricCabinetSwitchControlTask::onCommandResponseReceived,
                                 Qt::QueuedConnection));
}

void ElectricCabinetSwitchControlTask::disconnectAll()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

QByteArray ElectricCabinetSwitchControlTask::buildControlFrame() const
{
    quint8 switchStatusByte = currentSwitchStatusByte();
    if (m_on) {
        switchStatusByte |= m_mask;
    } else {
        switchStatusByte &= static_cast<quint8>(~m_mask);
    }

    QByteArray frame;
    frame.append(static_cast<char>(0x02));
    frame.append(static_cast<char>(0x05));

    QByteArray body;
    body.append(static_cast<char>(0x00));
    body.append(static_cast<char>(0x00));
    body.append(static_cast<char>(kSwitchControlRegister));
    body.append(static_cast<char>(0x01));
    body.append(static_cast<char>(switchStatusByte));

    frame.append(body);
    frame.append(calcChecksum(body));
    return frame;
}

quint8 ElectricCabinetSwitchControlTask::currentSwitchStatusByte() const
{
    quint8 switchStatusByte = 0;
    const ElectricCabinetInfo* info = SharedData::getElectricCabinetInfo();
    if (!info) {
        return switchStatusByte;
    }

    if (info->fan1Running()) switchStatusByte |= Fan1Mask;
    if (info->fan2Running()) switchStatusByte |= Fan2Mask;
    if (info->redLightOn()) switchStatusByte |= RedLightMask;
    if (info->greenLightOn()) switchStatusByte |= GreenLightMask;
    if (info->powerOn()) switchStatusByte |= PowerMask;
    return switchStatusByte;
}

void ElectricCabinetSwitchControlTask::updateInfoByStatusByte(ElectricCabinetInfo* info, quint8 statusByte)
{
    if (!info) {
        return;
    }

    info->setFan1Running((statusByte & Fan1Mask) != 0);
    info->setFan2Running((statusByte & Fan2Mask) != 0);
    info->setRedLightOn((statusByte & RedLightMask) != 0);
    info->setGreenLightOn((statusByte & GreenLightMask) != 0);
    info->setPowerOn((statusByte & PowerMask) != 0);
}

QByteArray ElectricCabinetSwitchControlTask::calcChecksum(const QByteArray& body)
{
    quint32 sum = 0;
    for (char value : body) {
        sum += static_cast<quint8>(value);
    }

    const quint8 lowByte = static_cast<quint8>(sum & 0xFF);
    const quint8 hiNibble = static_cast<quint8>((lowByte >> 4) & 0x0F);
    const quint8 loNibble = static_cast<quint8>(lowByte & 0x0F);

    auto toAsciiHex = [](quint8 nibble) -> char {
        return nibble < 10 ? static_cast<char>('0' + nibble)
                           : static_cast<char>('A' + (nibble - 10));
    };

    QByteArray checksum;
    checksum.append(toAsciiHex(hiNibble));
    checksum.append(toAsciiHex(loNibble));
    return checksum;
}

QString ElectricCabinetSwitchControlTask::frameToText(const QByteArray& frame)
{
    return frame.isEmpty() ? QStringLiteral("-") : QString(frame.toHex(' ').toUpper());
}

QString ElectricCabinetSwitchControlTask::statusByteText(quint8 statusByte)
{
    return QStringLiteral("0b%1").arg(QString::number(statusByte, 2).rightJustified(8, QLatin1Char('0')));
}

void ElectricCabinetSwitchControlTask::finishWithResult(bool success, const QString& message)
{
    if (m_finished) {
        return;
    }

    m_finished = true;
    if (m_timeoutTimer) {
        m_timeoutTimer->stop();
    }

    disconnectAll();
    setState(success ? Finished : Failed);

    if (OperationDispatchTask* opTask = SharedData::getOperationDispatchTask()) {
        if (success) {
            opTask->logMessage(message);
        } else {
            opTask->logError(message);
        }
    }

    emit finished(success, message);
}

void ElectricCabinetSwitchControlTask::onCommandResponseReceived(const QByteArray& frame)
{
    if (m_finished) {
        return;
    }

    if (frame.size() < kExpectedResponseMinLength ||
        static_cast<quint8>(frame.at(0)) != kResponseHead ||
        static_cast<quint8>(frame.at(4)) != kSwitchControlRegister) {
        return;
    }

    const quint8 statusByte = static_cast<quint8>(frame.at(5));
    updateInfoByStatusByte(SharedData::getElectricCabinetInfo(), statusByte);

    finishWithResult(
        true,
        QString("Electric cabinet switch control succeeded: mask=0x%1, on=%2, status=%3, rx=%4")
            .arg(QString("%1").arg(m_mask, 2, 16, QLatin1Char('0')).toUpper())
            .arg(m_on)
            .arg(statusByteText(statusByte))
            .arg(frameToText(frame)));
}

void ElectricCabinetSwitchControlTask::onSelfTimeout()
{
    if (m_finished) {
        return;
    }

    finishWithResult(
        false,
        QString("Electric cabinet switch control timeout: tx=%1")
            .arg(frameToText(m_txFrame)));
}
