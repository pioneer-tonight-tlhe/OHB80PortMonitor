#ifndef MODBUSCONNECTER_H
#define MODBUSCONNECTER_H

#include <QObject>
#include <QTimer>
#include <QTcpSocket>

class ModbusConnecter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 连接模式枚举
     * @details 定义连接行为：单次连接或自动重连模式
     */
    enum class ConnectionMode {
        SingleConnection,    // 单次连接，不启用自动重连
        AutoReconnect       // 启用自动重连
    };
    Q_ENUM(ConnectionMode)

    /**
     * @brief 连接状态枚举
     * @details 表示当前 Modbus 连接所处的状态
     */
    enum class ConnectionStatus {
        Disconnected,        // 已断开
        Connecting,          // 连接中
        Connected,           // 已连接
        Error                // 错误状态
    };
    Q_ENUM(ConnectionStatus)

    /**
     * @brief 构造函数
     * @param socket 外部传入的 TCP Socket 引用
     * @param host 目标主机地址
     * @param port 目标端口
     * @param masterId Master 设备 ID
     * @param parent 父对象，默认为 nullptr
     * @details  初始化定时器和成员变量，保存目标端点用于重连
     */
    explicit ModbusConnecter(QTcpSocket& socket, const QString& host, quint16 port, const QString& masterId = QString(), QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     * @details  自动停止所有定时器并清理资源
     */
    ~ModbusConnecter();

    // 禁用拷贝和赋值（引用成员不支持）
    ModbusConnecter(const ModbusConnecter&) = delete;
    ModbusConnecter& operator=(const ModbusConnecter&) = delete;

    /**
     * @brief 连接到 Modbus 设备
     * @param mode 连接模式（单次连接或自动重连）
     * @return true 连接成功，false 连接失败
     * @details  直接对外部传入的 QTcpSocket 发起连接。
     */
    bool connectDevice(ConnectionMode mode = ConnectionMode::SingleConnection);
    
    /**
     * @brief 断开与 Modbus 设备的连接
     * @param mode 断开模式（单次断开或完全断开）
     * @return true 断开成功，false 已经断开
     * @details  单次断开不影响自动重连机制，完全断开会停止重连机制
     */
    bool disconnectDevice(ConnectionMode mode = ConnectionMode::SingleConnection);
    
    /**
     * @brief 获取当前连接状态
     * @return 当前连接状态
     */
    ConnectionStatus getStatus() const { return m_status; }
    
    /**
     * @brief 检查是否启用自动重连
     * @return true 启用自动重连，false 禁用自动重连
     */
    bool isAutoReconnectEnabled() const { return m_autoReconnectEnabled; }
    
    /**
     * @brief 设置自动重连间隔
     * @param intervalMs 重连间隔（毫秒）
     * @details  设置重连尝试之间的时间间隔
     */
    void setAutoReconnectInterval(int intervalMs);

    QString host() const { return m_host; }
    quint16 port() const { return m_port; }
    void setEndpoint(const QString& host, quint16 port);

signals:
    /**
     * @brief 连接状态改变信号
     * @param status 新的连接状态
     * @param masterId Master 设备 ID
     * @details  当连接状态改变时发射
     */
    void statusChanged(ConnectionStatus status, const QString& masterId);
    
    /**
     * @brief 连接错误信号
     * @param errorMessage 错误信息
     * @details  当连接失败或发生错误时发射
     */
    void connectionError(const QString& errorMessage);
    
private slots:
    /**
     * @brief 重连定时器槽函数
     * @details 由重连定时器触发，执行非阻塞重新连接逻辑
     */
    void onReconnectTimer();
    
    /**
     * @brief 异步重连成功槽函数
     */
    void onAsyncReconnectConnected();

    /**
     * @brief 异步重连超时槽函数
     */
    void onAsyncReconnectTimeout();

    /**
     * @brief 连接检查定时器槽函数
     * @details 定期检查连接状态，连接丢失时触发重连
     */
    void checkConnection();

private:
    // 私有辅助方法
    void setStatus(ConnectionStatus status);               // 更新连接状态并发射信号
    void startAutoReconnect();                              // 启动自动重连机制
    void stopAutoReconnect();                               // 停止自动重连机制
    void startConnectionCheck();                            // 启动连接健康检查
    void stopConnectionCheck();                             // 停止连接健康检查
    bool performConnection();                               // 执行连接操作
    QString getErrorString(QAbstractSocket::SocketError errorCode) const; // 获取错误描述字符串

    void cleanupAsyncReconnect();                    // 清理异步重连临时连接

    QTcpSocket* m_socket = nullptr;         // 外部传入的 Socket（不拥有）
    QString m_host;                        // 目标主机地址
    quint16 m_port = 0;                    // 目标端口
    QString& m_masterId;                   // Master 设备 ID（引用）
    ConnectionStatus m_status;             // 当前连接状态
    bool m_autoReconnectEnabled;           // 自动重连启用标志
    bool m_asyncReconnecting = false;      // 是否正在异步重连中
    bool m_autoReconnectStartedLogged = false; // 当前断线周期内是否已记录开始自动重连
    int m_retryCount;                      // 当前重试次数
    qint64 m_disconnectStartMs = 0;         // 当前断线周期开始时间，用于恢复时统计持续时间
    QString m_lastError;                   // 最近一次错误信息
    QTimer* m_reconnectTimer;             // 重连定时器
    QTimer* m_reconnectTimeoutTimer = nullptr; // 异步重连超时定时器
    QTimer* m_connectionCheckTimer = nullptr;       // 连接检查定时器
};

#endif // MODBUSCONNECTER_H
