#include "electric_cabinet_serial_port_status_task.h"

#include "app/alarmtype.h"
#include "app/shareddata.h"
#include "electriccabinetserialportconfig.h"
#include "electriccabinetserialportcontroller.h"
#include "electriccabinetinfo.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"

#include <QtGlobal>

const QString ElectricCabinetSerialPortStatusTask::kAlarmSourceIdentifier = QStringLiteral("ElectricCabinet");

ElectricCabinetSerialPortStatusTask::ElectricCabinetSerialPortStatusTask(QObject* parent)
    : SchedulerTask(parent)
    , m_logger(new ElectricCabinetSerialPortStatusTaskLogger(true))
{
}

ElectricCabinetSerialPortStatusTask::~ElectricCabinetSerialPortStatusTask()
{
    disconnectAll();
    delete m_logger;
    m_logger = nullptr;
}

void ElectricCabinetSerialPortStatusTask::start()
{
    setState(Running);
    m_stopped = false;
    m_hasKnownStatus = false;
    m_lastConnected = false;
    m_disconnectedAlarmReported = false;
    disconnectAll();

    const ElectricCabinetSerialPortSettings settings =
        ElectricCabinetSerialPortConfig::getInstance().readSettings();
    m_portName = settings.portName;

    ElectricCabinetSerialPortController* controller =
        SharedData::getElectricCabinetSerialPortController();
    if (!controller) {
        setState(Failed);
        const QString reason = QStringLiteral("ElectricCabinetSerialPortController is null");
        m_logger->error("start", reason);
        submitDisconnectedAlarm(reason);
        emit finished(false, reason);
        return;
    }

    if (!settings.enabled) {
        controller->disconnectPort();
        m_logger->info("start", "Electric cabinet serial port monitor is disabled by config.");
        resolveDisconnectedAlarm(QStringLiteral("serial port monitor disabled"));
        emit progress(0, QStringLiteral("Electric cabinet serial port monitor disabled"));
        return;
    }

    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::connectedChanged,
                                 this, &ElectricCabinetSerialPortStatusTask::onConnectedChanged,
                                 Qt::QueuedConnection));
    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::portError,
                                 this, &ElectricCabinetSerialPortStatusTask::onPortError,
                                 Qt::QueuedConnection));
    m_connections.append(connect(controller, &ElectricCabinetSerialPortController::commandSendFailed,
                                 this, &ElectricCabinetSerialPortStatusTask::onCommandSendFailed,
                                 Qt::QueuedConnection));

    initializeController(controller, settings);
    controller->connectPort();

    m_logger->info(
        "start",
        QString("Electric cabinet serial port monitor started. port=%1 baudRate=%2 autoReconnect=%3 reconnectIntervalMs=%4")
            .arg(settings.portName)
            .arg(settings.baudRate)
            .arg(settings.autoReconnect ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(settings.reconnectIntervalMs));
    emit progress(0, QStringLiteral("Electric cabinet serial port monitor started"));
}

void ElectricCabinetSerialPortStatusTask::stop()
{
    m_stopped = true;
    if (ElectricCabinetSerialPortController* controller =
            SharedData::getElectricCabinetSerialPortController()) {
        controller->disconnectPort();
    }
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("Electric cabinet serial port monitor stopped"));
    if (m_logger) {
        m_logger->info("stop", "Electric cabinet serial port monitor stopped.");
    }
}

void ElectricCabinetSerialPortStatusTask::disconnectAll()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

void ElectricCabinetSerialPortStatusTask::initializeController(
    ElectricCabinetSerialPortController* controller,
    const ElectricCabinetSerialPortSettings& settings)
{
    controller->start();
    m_portName = settings.portName;

    controller->setPortName(settings.portName);
    controller->setBaudRate(settings.baudRate);
    controller->setDataBits(settings.dataBits);
    controller->setParity(settings.parity);
    controller->setStopBits(settings.stopBits);
    controller->setFlowControl(settings.flowControl);
    controller->setCommandTimeoutMs(settings.commandTimeoutMs);
    controller->setInterFrameTimeoutMs(settings.interFrameTimeoutMs);
    controller->setAutoReconnect(settings.autoReconnect, settings.reconnectIntervalMs);

    m_logger->info(
        "initializeController",
        QString("initialized from config. port=%1 baudRate=%2 dataBits=%3 parity=%4 stopBits=%5 flowControl=%6 autoReconnect=%7 reconnectIntervalMs=%8 commandTimeoutMs=%9 interFrameTimeoutMs=%10")
            .arg(settings.portName)
            .arg(settings.baudRate)
            .arg(static_cast<int>(settings.dataBits))
            .arg(static_cast<int>(settings.parity))
            .arg(static_cast<int>(settings.stopBits))
            .arg(static_cast<int>(settings.flowControl))
            .arg(settings.autoReconnect ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(settings.reconnectIntervalMs)
            .arg(settings.commandTimeoutMs)
            .arg(settings.interFrameTimeoutMs));
}

void ElectricCabinetSerialPortStatusTask::onConnectedChanged(bool connected)
{
    applyConnectionState(connected,
                         connected ? QStringLiteral("serial port connected")
                                   : QStringLiteral("serial port disconnected"));
}

void ElectricCabinetSerialPortStatusTask::onPortError(const QString& message)
{
    if (m_stopped) {
        return;
    }

    m_logger->error("onPortError", QString("port=%1 error=%2").arg(m_portName, message));
    if (!m_hasKnownStatus || !m_lastConnected) {
        applyConnectionState(false, message);
    }
}

void ElectricCabinetSerialPortStatusTask::onCommandSendFailed(const QString& message)
{
    if (m_stopped) {
        return;
    }

    m_logger->warn("onCommandSendFailed", QString("port=%1 send failed=%2").arg(m_portName, message));
}

void ElectricCabinetSerialPortStatusTask::applyConnectionState(bool connected, const QString& reason)
{
    if (m_stopped) {
        return;
    }

    const bool changed = !m_hasKnownStatus || m_lastConnected != connected;
    m_hasKnownStatus = true;
    m_lastConnected = connected;

    if (!changed) {
        return;
    }

    emit serialConnectionChanged(connected, reason);
    if (ElectricCabinetInfo* info = SharedData::getElectricCabinetInfo()) {
        info->setSerialPortConnected(connected);
    }

    if (connected) {
        m_logger->info("applyConnectionState", QString("port=%1 connected. reason=%2").arg(m_portName, reason));
        resolveDisconnectedAlarm(reason);
        m_disconnectedAlarmReported = false;
    } else {
        m_logger->warn("applyConnectionState", QString("port=%1 disconnected. reason=%2").arg(m_portName, reason));
        submitDisconnectedAlarm(reason);
        m_disconnectedAlarmReported = true;
    }
}

void ElectricCabinetSerialPortStatusTask::submitDisconnectedAlarm(const QString& reason)
{
    if (m_disconnectedAlarmReported) {
        return;
    }

    const QString description =
        QStringLiteral("[ElectricCabinet] Serial port is not connected. port=%1 reason=%2")
            .arg(m_portName.isEmpty() ? QStringLiteral("-") : m_portName)
            .arg(reason);

    if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
        dispatcher->submitAlarm(
            static_cast<int>(AlarmType::ElectricCabinetSerialPortDisconnected),
            static_cast<int>(AlarmSource::System),
            kAlarmSourceIdentifier,
            description);
    } else {
        m_logger->warn("submitDisconnectedAlarm", "AlarmDispatchTask is null.");
    }

    if (OperationDispatchTask* opTask = SharedData::getOperationDispatchTask()) {
        opTask->logError(description);
    }
}

void ElectricCabinetSerialPortStatusTask::resolveDisconnectedAlarm(const QString& reason)
{
    const QString description =
        QStringLiteral("[ElectricCabinet] Serial port connected. port=%1 reason=%2")
            .arg(m_portName.isEmpty() ? QStringLiteral("-") : m_portName)
            .arg(reason);

    if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
        dispatcher->submitResolve(
            static_cast<int>(AlarmType::ElectricCabinetSerialPortDisconnected),
            static_cast<int>(AlarmSource::System),
            kAlarmSourceIdentifier);
    } else {
        m_logger->warn("resolveDisconnectedAlarm", "AlarmDispatchTask is null.");
    }

    if (OperationDispatchTask* opTask = SharedData::getOperationDispatchTask()) {
        opTask->logMessage(description);
    }
}
