#include "electric_cabinet_set_temp_humi_task.h"

#include "app/shareddata.h"
#include "electriccabinetserialportconfig.h"
#include "electriccabinetserialportcontroller.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"

#include <QTimer>
#include <QtGlobal>

namespace {
constexpr quint8 kResponseHead = 0x03;
constexpr quint8 kTempHumiRegister = 0x34;
constexpr int kExpectedResponseMinLength = 9;
}

ElectricCabinetSetTempHumiTask::ElectricCabinetSetTempHumiTask(double tempMax,
                                                               double humiMax,
                                                               QObject* parent)
    : SchedulerTask(parent)
    , m_tempMax(tempMax)
    , m_humiMax(humiMax)
{
}

ElectricCabinetSetTempHumiTask::~ElectricCabinetSetTempHumiTask()
{
    disconnectAll();
}

void ElectricCabinetSetTempHumiTask::start()
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
                this, &ElectricCabinetSetTempHumiTask::onSelfTimeout);
    }

    connectController(controller);

    m_txFrame = buildFrame();

    if (OperationDispatchTask* opTask = SharedData::getOperationDispatchTask()) {
        opTask->logMessage(
            QString("Electric cabinet temp/humi setting started: tempMax=%1, humiMax=%2, tx=%3")
                .arg(m_tempMax, 0, 'f', 2)
                .arg(m_humiMax, 0, 'f', 2)
                .arg(frameToText(m_txFrame)));
    }

    controller->sendFrame(m_txFrame);

    const ElectricCabinetTempHumiSettings settings =
        ElectricCabinetSerialPortConfig::getInstance().readTempHumiSettings();
    m_timeoutTimer->start(qMax(1, settings.commandResponseTimeoutMs));

    emit progress(0, QStringLiteral("Electric cabinet temp/humi command sent"));
}

void ElectricCabinetSetTempHumiTask::stop()
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
    emit finished(false, QStringLiteral("Electric cabinet temp/humi setting cancelled"));
}

ElectricCabinetSerialPortController* ElectricCabinetSetTempHumiTask::currentController() const
{
    return SharedData::getElectricCabinetSerialPortController();
}

void ElectricCabinetSetTempHumiTask::connectController(ElectricCabinetSerialPortController* controller)
{
    if (!controller) {
        return;
    }

    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::commandResponseReceived,
                                 this, &ElectricCabinetSetTempHumiTask::onCommandResponseReceived,
                                 Qt::QueuedConnection));
}

void ElectricCabinetSetTempHumiTask::disconnectAll()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

QByteArray ElectricCabinetSetTempHumiTask::buildFrame() const
{
    const quint16 tempReg = static_cast<quint16>(qBound<qint64>(
        0, qRound64(m_tempMax * 100.0), 0xFFFF));
    const quint16 humiReg = static_cast<quint16>(qBound<qint64>(
        0, qRound64(m_humiMax * 100.0), 0xFFFF));

    QByteArray frame;
    frame.append(static_cast<char>(0x02));
    frame.append(static_cast<char>(0x07));

    QByteArray body;
    body.append(static_cast<char>(0x00));
    body.append(static_cast<char>(0x00));
    body.append(static_cast<char>(kTempHumiRegister));
    body.append(static_cast<char>((tempReg >> 8) & 0xFF));
    body.append(static_cast<char>(tempReg & 0xFF));
    body.append(static_cast<char>((humiReg >> 8) & 0xFF));
    body.append(static_cast<char>(humiReg & 0xFF));

    frame.append(body);
    frame.append(calcChecksum(body));
    return frame;
}

QByteArray ElectricCabinetSetTempHumiTask::calcChecksum(const QByteArray& body)
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

QString ElectricCabinetSetTempHumiTask::frameToText(const QByteArray& frame)
{
    return frame.isEmpty() ? QStringLiteral("-") : QString(frame.toHex(' ').toUpper());
}

void ElectricCabinetSetTempHumiTask::finishWithResult(bool success, const QString& message)
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

void ElectricCabinetSetTempHumiTask::onCommandResponseReceived(const QByteArray& frame)
{
    if (m_finished) {
        return;
    }

    if (frame.size() < kExpectedResponseMinLength ||
        static_cast<quint8>(frame.at(0)) != kResponseHead ||
        static_cast<quint8>(frame.at(4)) != kTempHumiRegister) {
        return;
    }

    const quint16 tempReg = (static_cast<quint8>(frame.at(5)) << 8)
                            | static_cast<quint8>(frame.at(6));
    const quint16 humiReg = (static_cast<quint8>(frame.at(7)) << 8)
                            | static_cast<quint8>(frame.at(8));
    const double readTemp = tempReg / 100.0;
    const double readHumi = humiReg / 100.0;

    if (qAbs(readTemp - m_tempMax) < 0.01 && qAbs(readHumi - m_humiMax) < 0.01) {
        ElectricCabinetTempHumiSettings settings =
            ElectricCabinetSerialPortConfig::getInstance().readTempHumiSettings();
        settings.tempMax = readTemp;
        settings.humiMax = readHumi;
        ElectricCabinetSerialPortConfig::getInstance().writeTempHumiSettings(settings);

        finishWithResult(
            true,
            QString("Electric cabinet temp/humi setting succeeded: tempMax=%1, humiMax=%2, rx=%3")
                .arg(readTemp, 0, 'f', 2)
                .arg(readHumi, 0, 'f', 2)
                .arg(frameToText(frame)));
        return;
    }

    finishWithResult(
        false,
        QString("Electric cabinet temp/humi echo mismatch: expected tempMax=%1 humiMax=%2, actual tempMax=%3 humiMax=%4, rx=%5")
            .arg(m_tempMax, 0, 'f', 2)
            .arg(m_humiMax, 0, 'f', 2)
            .arg(readTemp, 0, 'f', 2)
            .arg(readHumi, 0, 'f', 2)
            .arg(frameToText(frame)));
}

void ElectricCabinetSetTempHumiTask::onSelfTimeout()
{
    if (m_finished) {
        return;
    }

    finishWithResult(
        false,
        QString("Electric cabinet temp/humi setting timeout: tx=%1")
            .arg(frameToText(m_txFrame)));
}
