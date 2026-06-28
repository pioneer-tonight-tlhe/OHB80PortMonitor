#include "electric_cabinet_property_monitor_task.h"

#include "app/shareddata.h"
#include "electriccabinetserialportconfig.h"
#include "electriccabinetserialportcontroller.h"

#include <QTimer>
#include <QtGlobal>

const int ElectricCabinetPropertyMonitorTask::DefaultPollIntervalMs = 1000;
const int ElectricCabinetPropertyMonitorTask::DefaultRetryIntervalMs = 3000;
const int ElectricCabinetPropertyMonitorTask::ExpectedResponseLength = 21;

ElectricCabinetPropertyMonitorTask::ElectricCabinetPropertyMonitorTask(QObject* parent)
    : SchedulerTask(parent)
    , m_logger(new ElectricCabinetPropertyMonitorTaskLogger(true))
    , m_statusRequestFrame(QByteArray::fromHex("02030000333333"))
{
}

ElectricCabinetPropertyMonitorTask::~ElectricCabinetPropertyMonitorTask()
{
    disconnectAll();
    delete m_logger;
    m_logger = nullptr;
}

void ElectricCabinetPropertyMonitorTask::start()
{
    if (m_running) {
        return;
    }

    setState(Running);
    m_running = true;
    m_waitingResponse = false;
    disconnectAll();

    const ElectricCabinetPropertyMonitorSettings settings =
        ElectricCabinetSerialPortConfig::getInstance().readPropertyMonitorSettings();
    initializeByConfig(settings);

    if (!settings.enabled) {
        m_logger->info("start", "Electric cabinet property monitor is disabled by config.");
        emit progress(0, QStringLiteral("Electric cabinet property monitor disabled"));
        return;
    }

    ElectricCabinetSerialPortController* controller =
        SharedData::getElectricCabinetSerialPortController();
    if (!controller) {
        setState(Failed);
        m_running = false;
        const QString reason = QStringLiteral("ElectricCabinetSerialPortController is null");
        m_logger->error("start", reason);
        emit finished(false, reason);
        return;
    }

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setSingleShot(true);
        connect(m_pollTimer, &QTimer::timeout,
                this, &ElectricCabinetPropertyMonitorTask::onPollTimer);
    }

    connectController(controller);
    scheduleNextPoll(m_retryIntervalMs);

    m_logger->info(
        "start",
        QString("Electric cabinet property monitor started. pollIntervalMs=%1 retryIntervalMs=%2 requestFrame=%3")
            .arg(m_pollIntervalMs)
            .arg(m_retryIntervalMs)
            .arg(frameToText(m_statusRequestFrame)));
    emit progress(0, QStringLiteral("Electric cabinet property monitor started"));
}

void ElectricCabinetPropertyMonitorTask::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    m_waitingResponse = false;
    if (m_pollTimer) {
        m_pollTimer->stop();
    }

    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("Electric cabinet property monitor stopped"));
    if (m_logger) {
        m_logger->info("stop", "Electric cabinet property monitor stopped.");
    }
}

void ElectricCabinetPropertyMonitorTask::connectController(ElectricCabinetSerialPortController* controller)
{
    if (!controller) {
        return;
    }

    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::commandResponseReceived,
                                 this, &ElectricCabinetPropertyMonitorTask::onCommandResponseReceived,
                                 Qt::QueuedConnection));
    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::commandTimeout,
                                 this, &ElectricCabinetPropertyMonitorTask::onCommandTimeout,
                                 Qt::QueuedConnection));
    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::commandSendFailed,
                                 this, &ElectricCabinetPropertyMonitorTask::onCommandSendFailed,
                                 Qt::QueuedConnection));
}

void ElectricCabinetPropertyMonitorTask::disconnectAll()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

void ElectricCabinetPropertyMonitorTask::initializeByConfig(
    const ElectricCabinetPropertyMonitorSettings& settings)
{
    m_pollIntervalMs = qMax(1, settings.pollIntervalMs);
    m_retryIntervalMs = qMax(1, settings.retryIntervalMs);

    const QByteArray requestFrame = normalizedRequestFrame(settings.requestFrameHex);
    if (requestFrame.isEmpty()) {
        m_statusRequestFrame = QByteArray::fromHex("02030000333333");
        m_logger->warn("initializeByConfig",
                       QString("invalid request frame config=%1, fallback=%2")
                           .arg(settings.requestFrameHex)
                           .arg(frameToText(m_statusRequestFrame)));
        return;
    }

    m_statusRequestFrame = requestFrame;
}

void ElectricCabinetPropertyMonitorTask::scheduleNextPoll(int intervalMs)
{
    if (m_running && m_pollTimer) {
        m_pollTimer->start(qMax(1, intervalMs));
    }
}

void ElectricCabinetPropertyMonitorTask::sendStatusRequest()
{
    ElectricCabinetSerialPortController* controller =
        SharedData::getElectricCabinetSerialPortController();
    if (!controller || !controller->isConnected()) {
        scheduleNextPoll(m_retryIntervalMs);
        return;
    }

    if (m_statusRequestFrame.isEmpty()) {
        m_logger->error("sendStatusRequest", "status request frame is empty.");
        scheduleNextPoll(m_retryIntervalMs);
        return;
    }

    m_waitingResponse = true;
    controller->sendFrame(m_statusRequestFrame);
    m_logger->debug("sendStatusRequest", QString("tx=%1").arg(frameToText(m_statusRequestFrame)));
}

bool ElectricCabinetPropertyMonitorTask::parseStatusFrame(const QByteArray& frame,
                                                          ElectricCabinetInfo* info) const
{
    if (!info || frame.size() != ExpectedResponseLength) {
        return false;
    }

    if (static_cast<quint8>(frame.at(0)) != 0x03 ||
        static_cast<quint8>(frame.at(4)) != 0x33) {
        return false;
    }

    const quint8 doByte = static_cast<quint8>(frame.at(5));
    const quint8 diByte = static_cast<quint8>(frame.at(6));

    info->setFan1Running((doByte & 0x01) != 0);
    info->setFan2Running((doByte & 0x02) != 0);
    info->setRedLightOn((doByte & 0x08) != 0);
    info->setGreenLightOn((doByte & 0x10) != 0);
    info->setPowerOn((doByte & 0x20) != 0);

    info->setEmergencyStop1Active((diByte & 0x01) != 0);
    info->setEmergencyStop2Active((diByte & 0x02) != 0);
    info->setSmokeAlarmActive(false);

    info->setPhaseAVoltage(readUInt16Scaled(frame, 7));
    info->setPhaseBVoltage(readUInt16Scaled(frame, 9));
    info->setPhaseACurrent(readUInt16Scaled(frame, 11));
    info->setPhaseBCurrent(readUInt16Scaled(frame, 13));
    info->setTemperature(readUInt16Scaled(frame, 15));
    info->setHumidity(readUInt16Scaled(frame, 17));
    return true;
}

double ElectricCabinetPropertyMonitorTask::readUInt16Scaled(const QByteArray& frame, int offset)
{
    const quint8 hi = static_cast<quint8>(frame.at(offset));
    const quint8 lo = static_cast<quint8>(frame.at(offset + 1));
    return (256.0 * hi + lo) / 100.0;
}

QString ElectricCabinetPropertyMonitorTask::frameToText(const QByteArray& frame)
{
    return frame.isEmpty() ? QStringLiteral("-") : QString(frame.toHex(' ').toUpper());
}

QByteArray ElectricCabinetPropertyMonitorTask::normalizedRequestFrame(const QString& frameHex)
{
    QByteArray text = frameHex.toLatin1();
    text.replace(" ", "");
    text.replace("-", "");
    text.replace(":", "");
    return QByteArray::fromHex(text);
}

QString ElectricCabinetPropertyMonitorTask::propertyInfoText(const ElectricCabinetInfo& info) const
{
    return QString("fan1Running=%1, fan2Running=%2, redLightOn=%3, greenLightOn=%4, powerOn=%5, "
                   "emergencyStop1Active=%6, emergencyStop2Active=%7, smokeAlarmActive=%8, "
                   "phaseAVoltage=%9, phaseBVoltage=%10, phaseACurrent=%11, phaseBCurrent=%12, "
                   "temperature=%13, humidity=%14")
        .arg(info.fan1Running())
        .arg(info.fan2Running())
        .arg(info.redLightOn())
        .arg(info.greenLightOn())
        .arg(info.powerOn())
        .arg(info.emergencyStop1Active())
        .arg(info.emergencyStop2Active())
        .arg(info.smokeAlarmActive())
        .arg(info.phaseAVoltage(), 0, 'f', 2)
        .arg(info.phaseBVoltage(), 0, 'f', 2)
        .arg(info.phaseACurrent(), 0, 'f', 2)
        .arg(info.phaseBCurrent(), 0, 'f', 2)
        .arg(info.temperature(), 0, 'f', 2)
        .arg(info.humidity(), 0, 'f', 2);
}

void ElectricCabinetPropertyMonitorTask::onCommandResponseReceived(const QByteArray& frame)
{
    if (!m_running || !m_waitingResponse) {
        return;
    }

    m_waitingResponse = false;

    ElectricCabinetInfo* info = SharedData::getElectricCabinetInfo();
    if (!parseStatusFrame(frame, info)) {
        m_logger->warn("onCommandResponseReceived",
                       QString("invalid status frame. rx=%1").arg(frameToText(frame)));
        scheduleNextPoll(m_pollIntervalMs);
        return;
    }

    m_logger->debug("onCommandResponseReceived",
                    QString("rx=%1, info=%2").arg(frameToText(frame), propertyInfoText(*info)));

    scheduleNextPoll(m_pollIntervalMs);
}

void ElectricCabinetPropertyMonitorTask::onCommandTimeout()
{
    if (!m_running || !m_waitingResponse) {
        return;
    }

    m_waitingResponse = false;
    m_logger->warn("onCommandTimeout", QString("request timeout. tx=%1").arg(frameToText(m_statusRequestFrame)));
    scheduleNextPoll(m_pollIntervalMs);
}

void ElectricCabinetPropertyMonitorTask::onCommandSendFailed(const QString& message)
{
    if (!m_running || !m_waitingResponse) {
        return;
    }

    m_waitingResponse = false;
    m_logger->warn("onCommandSendFailed", message);
    scheduleNextPoll(m_retryIntervalMs);
}

void ElectricCabinetPropertyMonitorTask::onPollTimer()
{
    if (!m_running) {
        return;
    }

    if (m_waitingResponse) {
        scheduleNextPoll(m_retryIntervalMs);
        return;
    }

    sendStatusRequest();
}
