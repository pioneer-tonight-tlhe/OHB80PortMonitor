#include "modbustcpmaster.h"
#include "firmwareupgrader.h"
#include "sh85selfchecker.h"
#include "modbuslogger.h"
#include <QDebug>
#include <QMetaObject>
#include <QThread>
#include <QtGlobal>

// ============================================================
// ModbusTcpMaster - Modbus TCP 主控对象实现
// ============================================================

namespace {
QString connectionModeToString(ModbusConnecter::ConnectionMode mode)
{
    switch (mode) {
        case ModbusConnecter::ConnectionMode::SingleConnection: return "SingleConnection";
        case ModbusConnecter::ConnectionMode::AutoReconnect:    return "AutoReconnect";
    }
    return "Unknown";
}

QString connectionStatusToString(ModbusConnecter::ConnectionStatus status)
{
    switch (status) {
        case ModbusConnecter::ConnectionStatus::Disconnected: return "Disconnected";
        case ModbusConnecter::ConnectionStatus::Connecting:    return "Connecting";
        case ModbusConnecter::ConnectionStatus::Connected:     return "Connected";
        case ModbusConnecter::ConnectionStatus::Error:         return "Error";
    }
    return "Unknown";
}
} // namespace

ModbusTcpMaster::ModbusTcpMaster(const QString& ip, quint16 port, const QString& id, QObject* parent)
    : QObject(parent)
    , ID(id)
    , m_ip(ip)
    , m_port(port)
{
    m_socket = new QTcpSocket(this);
    m_connector = new ModbusConnecter(*m_socket, m_ip, m_port, ID, this);
    m_sender = new ModbusCommandSender(*m_socket, ID, this);
    m_periodicSender = new PeriodicCommandSender(*m_sender, ID, this);

    createInitialIssuerIfNeeded();
    m_firmwareUpgrader = new FirmwareUpgrader(this, this);
    m_selfChecker = new SH85SelfChecker(this, this);

    connect(m_connector, &ModbusConnecter::statusChanged,
            this, &ModbusTcpMaster::onConnectionStatusChanged);
    connect(m_connector, &ModbusConnecter::connectionError,
            this, &ModbusTcpMaster::onConnectionError);
    connect(m_periodicSender, &PeriodicCommandSender::disconnectDevice,
            this, &ModbusTcpMaster::onPeriodicDisconnectRequested);
}

bool ModbusTcpMaster::start(ModbusConnecter::ConnectionMode mode)
{
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "start",
        QString("开始启动设备连接，连接模式=%1").arg(connectionModeToString(mode)));
    enterState(State::Connecting);
    return m_connector->connectDevice(mode);
}

void ModbusTcpMaster::stop(ModbusConnecter::ConnectionMode mode)
{
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "stop",
        QString("停止设备，连接模式=%1").arg(connectionModeToString(mode)));
    pauseChildren();
    m_connector->disconnectDevice(mode);
    enterState(State::Idle);
}

ModbusConnecter* ModbusTcpMaster::connector() const
{
    return m_connector;
}

ModbusCommandSender* ModbusTcpMaster::sender() const
{
    return m_sender;
}


InitialCommandIssuer* ModbusTcpMaster::initialIssuer() const
{
    return m_initialIssuer;
}

void ModbusTcpMaster::configureInitialCommands(const QList<ModbusCommand>& queue, int intervalMs, int executionCount)
{
    m_initialCommandQueue = queue;
    m_initialCommandIntervalMs = qMax(0, intervalMs);
    m_initialCommandExecutionCount = qMax(1, executionCount);

    if (m_initialIssuer) {
        m_initialIssuer->setCommandQueue(m_initialCommandQueue);
        m_initialIssuer->setInterval(m_initialCommandIntervalMs);
        m_initialIssuer->setExecutionCount(m_initialCommandExecutionCount);
    }
}

PeriodicCommandSender* ModbusTcpMaster::periodicSender() const
{
    return m_periodicSender;
}

ModbusTcpMaster::State ModbusTcpMaster::currentState() const
{
    return m_state;
}

bool ModbusTcpMaster::isConnected() const
{
    return m_connector && m_connector->getStatus() == ModbusConnecter::ConnectionStatus::Connected;
}

QString ModbusTcpMaster::firmwareVersion() const
{
    return m_firmwareVersion;
}

FirmwareUpgrader* ModbusTcpMaster::firmwareUpgrader() const
{
    return m_firmwareUpgrader;
}

SH85SelfChecker* ModbusTcpMaster::selfChecker() const
{
    return m_selfChecker;
}

bool ModbusTcpMaster::reconfigureDeviceInfo(const QString& newId,
                                            const QString& newIp,
                                            quint16 newPort,
                                            QString* errorMessage)
{
    if (QThread::currentThread() != thread()) {
        bool result = false;
        QString localError;
        const bool invoked = QMetaObject::invokeMethod(this, [&]() {
            result = reconfigureDeviceInfo(newId, newIp, newPort, &localError);
        }, Qt::BlockingQueuedConnection);
        if (!invoked) {
            localError = QStringLiteral("Failed to invoke master reconfiguration");
        }
        if (errorMessage) {
            *errorMessage = localError;
        }
        return invoked && result;
    }

    if (newId.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("QRCode is empty");
        return false;
    }
    if (newIp.trimmed().isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("IP is empty");
        return false;
    }
    if (newPort == 0) {
        if (errorMessage) *errorMessage = QStringLiteral("Port is invalid");
        return false;
    }
    if (!m_connector) {
        if (errorMessage) *errorMessage = QStringLiteral("Connector is null");
        return false;
    }

    const QString oldId = ID;
    const QString oldIp = m_ip;
    const quint16 oldPort = m_port;

    ModbusLogger::masterInfo(oldId, "ModbusTcpMaster", "reconfigureDeviceInfo",
        QString("Reconfigure device: oldId=%1 oldEndpoint=%2:%3 newId=%4 newEndpoint=%5:%6")
            .arg(oldId).arg(oldIp).arg(oldPort).arg(newId).arg(newIp).arg(newPort));

    stop(ModbusConnecter::ConnectionMode::AutoReconnect);
    m_ip = newIp;
    m_port = newPort;
    m_connector->setEndpoint(m_ip, m_port);
    ID = newId;
    start(ModbusConnecter::ConnectionMode::AutoReconnect);

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void ModbusTcpMaster::onConnectionStatusChanged(ModbusConnecter::ConnectionStatus status, const QString& /*masterId*/)
{
    if (status == ModbusConnecter::ConnectionStatus::Connected
        || status == ModbusConnecter::ConnectionStatus::Disconnected) {
        ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "ModbusConnecter", "onConnectionStatusChanged",
            QString("连接状态变化: %1").arg(connectionStatusToString(status)));
    }

    switch (status) {
        case ModbusConnecter::ConnectionStatus::Connected:
            qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 设备连接成功，准备启动指令发送器";
            // 固件升级期间不恢复子模块，避免 receiver 重新连接 socket 抢读升级响应数据
            if (m_firmwareUpgrader && m_firmwareUpgrader->isRunning()) {
                qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 固件升级进行中，跳过 resumeChildren";
            } else {
                resumeChildren();
            }
            break;
        case ModbusConnecter::ConnectionStatus::Disconnected:
            qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 连接断开，暂停发送器和定时发送器（不干预连接器重连）";
            if (m_initialIssuer && m_initialStarted) {
                m_initialIssuer->stop();
                m_initialStarted = false;
            }
            if (m_periodicSender && m_periodicStarted) {
                m_periodicSender->stop();
                m_periodicStarted = false;
            }
            if (m_sender) {
                m_sender->stop();
            }
            enterState(State::Connecting);
            break;
        case ModbusConnecter::ConnectionStatus::Error:
            break;
        default:
            break;
    }
}

void ModbusTcpMaster::onConnectionError(const QString& message)
{
    qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 连接阶段错误 -" << message;
    if (m_state != State::Error) {
        ModbusLogger::masterWarn(ID, "ModbusTcpMaster", "ModbusConnecter", "onConnectionError",
            QString("连接阶段错误: %1").arg(message));
    }
    emit errorOccurred(State::Connecting, message);
    enterState(State::Error);
}

void ModbusTcpMaster::startSender()
{
    enterState(State::SenderStartup);
    m_sender->start();
    qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 指令发送器已启动，立即进入初始化阶段";
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "ModbusCommandSender", "startSender",
        "指令发送器已启动，立即进入初始化阶段");
}

void ModbusTcpMaster::startInitialIssuer()
{
    createInitialIssuerIfNeeded();
    if (!m_initialIssuer || m_initialStarted) {
        return;
    }

    enterState(State::Initializing);
    m_initialStarted = true;
    m_initialIssuer->start();
}

void ModbusTcpMaster::onInitialFinished(bool isOk, QStringList errorMsgList)
{
    qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 初始化完成，isOk="
             << isOk << "错误信息数：" << errorMsgList.size();
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "InitialCommandIssuer", "onInitialFinished",
        QString("初始化完成，isOk=%1，错误信息数=%2").arg(isOk).arg(errorMsgList.size()));

    m_initialStarted = false;

    if (!isOk) {
        emit errorOccurred(State::Initializing,
            QString("初始化完成，存在 %1 条错误信息：%2")
                .arg(errorMsgList.size())
                .arg(errorMsgList.join("; ")));
    }

    if (m_initialIssuer) {
        m_initialIssuer->deleteLater();
        m_initialIssuer = nullptr;
    }

    startPeriodicSender();
}

void ModbusTcpMaster::startPeriodicSender()
{
    if (m_periodicStarted) {
        enterState(State::Running);
        return;
    }

    // 检查 periodicSender 的队列是否为空，避免空队列启动
    if (m_periodicSender && m_periodicSender->commandQueue().isEmpty()) {
        qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 定时发送器队列为空，跳过启动，直接进入运行状态";
        ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "PeriodicCommandSender", "startPeriodicSender",
            "定时发送器队列为空，跳过启动，直接进入运行状态");
        enterState(State::Running);
        return;
    }

    enterState(State::PeriodicStartup);
    m_periodicStarted = true;
    m_periodicSender->start();

    qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 定时发送器已启动，进入正常运行状态";
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "PeriodicCommandSender", "startPeriodicSender",
        "定时发送器已启动，进入正常运行状态");
    enterState(State::Running);
}

void ModbusTcpMaster::onPeriodicDisconnectRequested()
{
    const QString msg = QString("定时发送器连续失败达到阈值（%1 次），触发断开重连")
                            .arg(PeriodicCommandSender::MAX_CONSECUTIVE_FAILURES);
    qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] " << msg;
    ModbusLogger::masterWarn(ID, "ModbusTcpMaster", "PeriodicCommandSender", "onPeriodicDisconnectRequested", msg);

    emit errorOccurred(State::Running, msg);

    pauseChildren();
    m_connector->disconnectDevice(ModbusConnecter::ConnectionMode::SingleConnection);
    enterState(State::Error);
}

void ModbusTcpMaster::createInitialIssuerIfNeeded()
{
    if (m_initialIssuer) {
        return;
    }

    m_initialIssuer = new InitialCommandIssuer(*m_sender, ID, this, this);
    connect(m_initialIssuer, &InitialCommandIssuer::finish,
            this, &ModbusTcpMaster::onInitialFinished);

    if (!m_initialCommandQueue.isEmpty()) {
        m_initialIssuer->setCommandQueue(m_initialCommandQueue);
        m_initialIssuer->setInterval(m_initialCommandIntervalMs);
        m_initialIssuer->setExecutionCount(m_initialCommandExecutionCount);
    }
}

void ModbusTcpMaster::pauseChildren()
{
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "pauseChildren", "暂停普通收发子模块");

    if (m_initialIssuer && m_initialStarted) {
        m_initialIssuer->stop();
        m_initialStarted = false;
    }

    if (m_periodicSender && m_periodicStarted) {
        m_periodicSender->stop();
        m_periodicStarted = false;
    }

    if (m_sender) {
        m_sender->stop();
        if (m_sender->receiver()) {
            m_sender->receiver()->disconnectSocketSignalSlots();
        }
    }
}

void ModbusTcpMaster::resumeChildren()
{
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "resumeChildren", "恢复普通收发子模块");

    if (m_sender && m_sender->receiver()) {
        m_sender->receiver()->reconnectSocketSignalSlots();
    }
    startSender();
    startInitialIssuer();
    if (!m_initialIssuer) {
        startPeriodicSender();
    }
}

void ModbusTcpMaster::enterState(State state)
{
    if (m_state == state) return;
    State oldState = m_state;
    m_state = state;
    emit stateChanged(state);
    qDebug() << "ModbusTcpMaster: [设备ID=" << ID << "] 状态切换: " << stateToString(oldState) << " -> " << stateToString(state);
    ModbusLogger::masterInfo(ID, "ModbusTcpMaster", "enterState",
        QString("状态切换: %1 -> %2").arg(stateToString(oldState), stateToString(state)));
}

QString ModbusTcpMaster::stateToString(State state)
{
    switch (state) {
        case State::Idle: return "空闲";
        case State::Connecting: return "连接中";
        case State::SenderStartup: return "指令发送器已启动";
        case State::Initializing: return "初始指令下发中";
        case State::PeriodicStartup: return "启动定时发送器中";
        case State::Running: return "正常运行";
        case State::Error: return "错误状态";
        default: return "未知状态";
    }
}
