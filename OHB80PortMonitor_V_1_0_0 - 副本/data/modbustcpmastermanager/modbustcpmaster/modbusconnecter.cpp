#include "modbusconnecter.h"
#include "modbuslogger.h"
#include <QDateTime>
#include <QDebug>
#include <QThread>

namespace {
constexpr int kReconnectIntervalMs = 3000;
constexpr int kReconnectTimeoutMs = 3000;
constexpr int kConnectionCheckIntervalMs = 3000;

QString connectionModeToString(ModbusConnecter::ConnectionMode mode)
{
    switch (mode) {
        case ModbusConnecter::ConnectionMode::SingleConnection: return "SingleConnection";
        case ModbusConnecter::ConnectionMode::AutoReconnect:    return "AutoReconnect";
    }
    return "Unknown";
}

bool shouldLogReconnectFailure(int retryCount)
{
    return retryCount == 1 || retryCount == 3 || retryCount == 10 || retryCount % 10 == 0;
}
} // namespace

ModbusConnecter::ModbusConnecter(QTcpSocket& socket, const QString& host, quint16 port, const QString& masterId, QObject *parent)
    : QObject(parent)
    , m_socket(&socket)
    , m_host(host)
    , m_port(port)
    , m_masterId(const_cast<QString&>(masterId))
    , m_status(ConnectionStatus::Disconnected)
    , m_autoReconnectEnabled(false)
    , m_retryCount(0)
    , m_reconnectTimer(new QTimer(this))
    , m_connectionCheckTimer(new QTimer(this))
{
    m_reconnectTimer->setInterval(kReconnectIntervalMs);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &ModbusConnecter::onReconnectTimer);

    m_reconnectTimeoutTimer = new QTimer(this);
    m_reconnectTimeoutTimer->setSingleShot(true);
    m_reconnectTimeoutTimer->setInterval(kReconnectTimeoutMs);
    connect(m_reconnectTimeoutTimer, &QTimer::timeout, this, &ModbusConnecter::onAsyncReconnectTimeout);

    m_connectionCheckTimer->setInterval(kConnectionCheckIntervalMs);
    connect(m_connectionCheckTimer, &QTimer::timeout, this, &ModbusConnecter::checkConnection);

    // 每次 TCP 连接建立后，立即禁用 Nagle 算法（TCP_NODELAY）。
    // 原因：HF2211 等串口转 TCP 网关串口侧转发慢，ACK 延迟较大；如果 Nagle 开启，
    // 多次小写入（8 字节 Modbus RTU 帧）会被内核缓存合并为一个 TCP segment，
    // 导致网关侧收到粘连帧后 RST 关闭连接。
    // 注意：此 lambda 必须先于 onAsyncReconnectConnected 连接，保证 NODELAY 优先生效。
    connect(m_socket, &QTcpSocket::connected, this, [this]() {
        m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
//        LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusConnecter][connected]：设备ID=%1 已启用 TCP_NODELAY，避免 Nagle 合并 Modbus RTU 小帧").arg(m_masterId).toStdString());
    });

    // 异步重连成功信号
    connect(m_socket, &QTcpSocket::connected, this, &ModbusConnecter::onAsyncReconnectConnected);

    // 异步重连失败信号 — 连接错误时快速触发下次重试
    connect(m_socket, &QAbstractSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) {
        if (!m_asyncReconnecting) return;
        // 连接失败，立即清理并安排下次重试
        cleanupAsyncReconnect();
        setStatus(ConnectionStatus::Error);
        m_lastError = m_socket->errorString();
        if (m_autoReconnectEnabled && m_retryCount == 0) {
            m_retryCount = 1; // 初次连接失败也计入本轮重连统计
        }
        if (m_autoReconnectEnabled && shouldLogReconnectFailure(m_retryCount)) {
            ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "onAsyncReconnectError",
                QString("自动重连失败，连续失败次数=%1，最近错误=%2").arg(m_retryCount).arg(m_lastError));
        }
        emit connectionError(m_lastError);
        if (m_autoReconnectEnabled) {
            startAutoReconnect();
        }
    });

    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        if (m_status != ConnectionStatus::Connected) {
            return;
        }

        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "disconnected",
            m_autoReconnectEnabled ? "TCP连接断开，准备自动重连" : "TCP连接断开");
        stopConnectionCheck();
        setStatus(ConnectionStatus::Disconnected);

        if (m_autoReconnectEnabled) {
            m_retryCount = 0;
            startAutoReconnect();
        }

        emit connectionError("Connection lost: " + getErrorString(m_socket->error()));
    });
}

ModbusConnecter::~ModbusConnecter()
{
    stopAutoReconnect();
    stopConnectionCheck();
}

bool ModbusConnecter::connectDevice(ConnectionMode mode)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, mode]() { connectDevice(mode); }, Qt::QueuedConnection);
        return true;
    }
    if (!m_socket) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "connectDevice", "QTcpSocket 指针为空");
        emit connectionError("QTcpSocket 无效");
        return false;
    }

    if (mode == ConnectionMode::AutoReconnect) {
        m_autoReconnectEnabled = true;
        m_retryCount = 0;
    } else {
        m_autoReconnectEnabled = false;
        stopAutoReconnect();
    }

    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "connectDevice",
        QString("开始连接设备 IP=%1 Port=%2 模式=%3")
            .arg(m_host).arg(m_port).arg(connectionModeToString(mode)));

    // 如果已经连接，直接返回
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        setStatus(ConnectionStatus::Connected);
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "connectDevice", "设备已处于连接状态");
        if (m_autoReconnectEnabled) {
            startConnectionCheck();
        }
        return true;
    }

    // 非阻塞连接：发起 connectToHost 后立即返回，不阻塞事件循环
    // 连接结果通过 connected/error 信号异步通知
    setStatus(ConnectionStatus::Connecting);

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

//    QString logMsg = QString("设备ID=%1 正在尝试连接服务器 %2:%3（非阻塞）").arg(m_masterId).arg(m_host).arg(m_port);
//    qDebug() << "ModbusConnecter: [设备ID=" << m_masterId << "] " << logMsg;
//    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusConnecter][connectDevice]：%1").arg(logMsg).toStdString());

    m_asyncReconnecting = true;
    m_socket->connectToHost(m_host, m_port);
    m_reconnectTimeoutTimer->start(); // 3秒超时

    return true; // 请求已接受，结果异步通知
}

bool ModbusConnecter::disconnectDevice(ConnectionMode mode)
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, mode]() { disconnectDevice(mode); }, Qt::QueuedConnection);
        return true;
    }
    if (!m_socket) {
        return false;
    }

    if (mode == ConnectionMode::SingleConnection) {
        stopConnectionCheck();
        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->disconnectFromHost();
        }
        setStatus(ConnectionStatus::Disconnected);
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "disconnectDevice",
            "主动断开连接，模式=SingleConnection");

        if (m_autoReconnectEnabled) {
            m_retryCount = 0;
            startAutoReconnect();
        }
        return true;
    } else {
        stopAutoReconnect();
        stopConnectionCheck();

        if (m_socket->state() != QAbstractSocket::UnconnectedState) {
            m_socket->disconnectFromHost();
        }
        setStatus(ConnectionStatus::Disconnected);
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "disconnectDevice",
            "完全断开连接，已停止自动重连");
        return true;
    }
}

void ModbusConnecter::setAutoReconnectInterval(int intervalMs)
{
    m_reconnectTimer->setInterval(intervalMs);
}

bool ModbusConnecter::performConnection()
{
    if (!m_socket) {
        m_lastError = "No TCP socket available";
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "performConnection", m_lastError);
        return false;
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        return true;
    }

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

//    QString logMsg = QString("设备ID=%1 正在尝试连接服务器 %2:%3").arg(m_masterId).arg(m_host).arg(m_port);
//    qDebug() << "ModbusConnecter: [设备ID=" << m_masterId << "] " << logMsg;
//    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusConnecter][performConnection]：%1").arg(logMsg).toStdString());
    m_socket->connectToHost(m_host, m_port);
    if (!m_socket->waitForConnected(3000)) {
        m_lastError = getErrorString(m_socket->error());
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "performConnection",
            QString("TCP连接失败，error=%1").arg(m_lastError));
        return false;
    }

    // 同步连接路径下显式设置 TCP_NODELAY（与 connected 信号 lambda 幂等）
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "performConnection", "TCP连接成功");

    return true;
}

void ModbusConnecter::setStatus(ConnectionStatus status)
{
    if (m_status != status) {
        ConnectionStatus oldStatus = m_status;
        m_status = status;
        emit statusChanged(status, m_masterId);

        QString oldStatusStr;
        switch (oldStatus) {
            case ConnectionStatus::Disconnected:
                oldStatusStr = "已断开";
                break;
            case ConnectionStatus::Connecting:
                oldStatusStr = "连接中";
                break;
            case ConnectionStatus::Connected:
                oldStatusStr = "已连接";
                break;
            case ConnectionStatus::Error:
                oldStatusStr = "错误";
                break;
        }

        QString newStatusStr;
        switch (status) {
            case ConnectionStatus::Disconnected:
                newStatusStr = "已断开";
                break;
            case ConnectionStatus::Connecting:
                newStatusStr = "连接中";
                break;
            case ConnectionStatus::Connected:
                newStatusStr = "已连接";
                break;
            case ConnectionStatus::Error:
                newStatusStr = "错误";
                break;
        }
//        QString logMsg = QString("设备ID=%1 状态转变：%2 -> %3").arg(m_masterId).arg(oldStatusStr).arg(newStatusStr);
//        qDebug() << "ModbusConnecter: [设备ID=" << m_masterId << "] " << logMsg;
//        LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusConnecter][setStatus]：%1").arg(logMsg).toStdString());
    }
}

void ModbusConnecter::startAutoReconnect()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &ModbusConnecter::startAutoReconnect, Qt::QueuedConnection);
        return;
    }
    if (!m_autoReconnectEnabled || m_reconnectTimer->isActive()) {
        return;
    }

    if (!m_autoReconnectStartedLogged) {
        m_autoReconnectStartedLogged = true;
        if (m_disconnectStartMs == 0) {
            m_disconnectStartMs = QDateTime::currentMSecsSinceEpoch();
        }
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "startAutoReconnect",
            QString("开始自动重连，间隔=%1ms").arg(m_reconnectTimer->interval()));
    }
    m_reconnectTimer->start();
}

void ModbusConnecter::stopAutoReconnect()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &ModbusConnecter::stopAutoReconnect, Qt::QueuedConnection);
        return;
    }
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "stopAutoReconnect", "停止自动重连");
    }

    // 清理异步重连状态
    if (m_asyncReconnecting) {
        cleanupAsyncReconnect();
    }

    m_autoReconnectEnabled = false;
    m_autoReconnectStartedLogged = false;
    m_disconnectStartMs = 0;
}

void ModbusConnecter::startConnectionCheck()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &ModbusConnecter::startConnectionCheck, Qt::QueuedConnection);
        return;
    }
    if (!m_connectionCheckTimer->isActive()) {
        m_connectionCheckTimer->start();
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "startConnectionCheck",
            QString("启动连接心跳检查，间隔=%1ms").arg(m_connectionCheckTimer->interval()));
    }
}

void ModbusConnecter::stopConnectionCheck()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &ModbusConnecter::stopConnectionCheck, Qt::QueuedConnection);
        return;
    }
    if (m_connectionCheckTimer->isActive()) {
        m_connectionCheckTimer->stop();
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "stopConnectionCheck", "停止连接心跳检查");
    }
}

void ModbusConnecter::onReconnectTimer()
{
    if (!m_autoReconnectEnabled || !m_socket) {
        return;
    }

    // 如果已经在异步重连中，跳过
    if (m_asyncReconnecting) {
        return;
    }

    m_retryCount++;
//    QString logMsg = QString("设备ID=%1 第 %2 次重连尝试（非阻塞）").arg(m_masterId).arg(m_retryCount);
//    qDebug() << "ModbusConnecter: [设备ID=" << m_masterId << "] " << logMsg;
//    LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusConnecter][onReconnectTimer]：%1").arg(logMsg).toStdString());

    // 非阻塞重连：发起 connectToHost 后立即返回，不阻塞事件循环
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_asyncReconnecting = true;
    setStatus(ConnectionStatus::Connecting);
    m_socket->connectToHost(m_host, m_port);
    m_reconnectTimeoutTimer->start(); // 3秒超时
}

void ModbusConnecter::onAsyncReconnectConnected()
{
    // 仅在异步重连过程中处理
    if (!m_asyncReconnecting) {
        return;
    }

    cleanupAsyncReconnect();
    setStatus(ConnectionStatus::Connected);
    const int failedRetryCount = m_retryCount;
    const qint64 disconnectedMs = m_disconnectStartMs > 0
        ? QDateTime::currentMSecsSinceEpoch() - m_disconnectStartMs
        : 0;
    m_retryCount = 0;
    m_autoReconnectStartedLogged = false;
    m_disconnectStartMs = 0;

    startConnectionCheck();
    if (failedRetryCount > 0 || disconnectedMs > 0) {
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "onAsyncReconnectConnected",
            QString("自动重连成功，失败次数=%1，断开持续=%2ms").arg(failedRetryCount).arg(disconnectedMs));
    } else {
        ModbusLogger::masterInfo(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "onAsyncReconnectConnected", "TCP连接成功");
    }
}

void ModbusConnecter::onAsyncReconnectTimeout()
{
    if (!m_asyncReconnecting) {
        return;
    }

    // 超时：中止连接尝试
    m_socket->abort();
    cleanupAsyncReconnect();
    setStatus(ConnectionStatus::Error);
    m_lastError = "Connection timeout (non-blocking)";
    if (m_autoReconnectEnabled && m_retryCount == 0) {
        m_retryCount = 1; // 初次连接超时也计入本轮重连统计
    }
    if (m_autoReconnectEnabled && shouldLogReconnectFailure(m_retryCount)) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "onAsyncReconnectTimeout",
            QString("自动重连超时，连续失败次数=%1").arg(m_retryCount));
    }
    emit connectionError(m_lastError);

    // 安排下次重连
    if (m_autoReconnectEnabled) {
        startAutoReconnect();
    }
}

void ModbusConnecter::cleanupAsyncReconnect()
{
    m_asyncReconnecting = false;
    m_reconnectTimeoutTimer->stop();
}

void ModbusConnecter::checkConnection()
{
    if (!m_socket || m_status != ConnectionStatus::Connected) {
        return;
    }

    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        ModbusLogger::masterWarn(m_masterId, "ModbusTcpMaster", "ModbusConnecter", "checkConnection",
            "连接检查失败，socket已断开");
        setStatus(ConnectionStatus::Disconnected);

        if (m_autoReconnectEnabled) {
            m_retryCount = 0;
            startAutoReconnect();
        }
        emit connectionError("Connection lost: " + getErrorString(m_socket->error()));
    } else {
//        qDebug() << "ModbusConnecter: [设备ID=" << m_masterId << "] Connection check - socket OK";
//        LoggerManager::getInstance()->log(AppLogger::ModbusMasterLoggerPath(m_masterId).toStdString(), Level::INFO, QString("[data][ModbusConnecter][checkConnection]：设备ID=%1 连接检查 - socket正常").arg(m_masterId).toStdString());
    }
}

QString ModbusConnecter::getErrorString(QAbstractSocket::SocketError errorCode) const
{
    if (!m_socket) {
        return QString("Socket 无效");
    }

    if (errorCode == QAbstractSocket::UnknownSocketError) {
        return m_socket->errorString();
    }

    return m_socket->errorString();
}
