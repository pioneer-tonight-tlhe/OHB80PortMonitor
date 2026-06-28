/*******************************************************************************************
 * @file modbustcpmaster.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class ModbusTcpMaster
 * @brief 编排单台 Modbus TCP 设备的连接、初始化、周期轮询和业务子模块。
 *
 * 设计目标：
 *      1. 统一管理单台设备的连接生命周期和运行状态机。
 *      2. 将连接器、指令发送器、初始化器和周期轮询器组合为完整设备运行单元。
 *      3. 为调度层提供稳定的设备重配置、状态查询和子模块访问入口。
 *******************************************************************************************/
#ifndef MODBUSTCPMASTER_H
#define MODBUSTCPMASTER_H

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTcpSocket>

#include "modbusconnecter.h"
#include "modbuscommandreceiver.h"
#include "modbuscommandsender.h"
#include "initialcommandissuer.h"
#include "periodiccommandsender.h"

class FirmwareUpgrader;
class SH85SelfChecker;

class ModbusTcpMaster : public QObject
{
    Q_OBJECT

    friend class InitialCommandIssuer;
    friend class PeriodicCommandSender;
    friend class ModbusTcpMasterPool;
    friend class FirmwareUpgrader;

public:
    // ============================ 公共数据类型 ============================
    /**
     * @brief 状态机枚举
     * @details 表示 ModbusTcpMaster 当前所处的运行阶段
     */
    enum class State {
        Idle,             // 空闲，等待外部启动连接
        Connecting,       // 连接中，等待 ModbusConnect 建立连接
        SenderStartup,    // 指令发送器已启动
        Initializing,     // 初始指令下发中
        PeriodicStartup,  // 启动定时发送器中
        Running,          // 正常运行，定时发送器工作中
        Error,            // 错误状态（连接错误或连续失败超阈值）
    };
    Q_ENUM(State)

    // ============================ 状态转换 ============================
    /**
     * @brief 将状态枚举转换为字符串
     * @param state 状态枚举值
     * @return 状态的中文字符串描述
     */
    static QString stateToString(State state);

    // ============================ 构造函数 ============================
    /**
     * @brief 构造函数
     * @param ip 设备 IP 地址
     * @param port 设备端口
     * @param id Master 唯一标识符
     * @param parent 父对象
     */
    explicit ModbusTcpMaster(const QString& ip, quint16 port, const QString& id = QString(), QObject* parent = nullptr);

    // ============================ 身份信息 ============================
    /**
     * @brief 唯一标识符，由外部生成并设置，用于区分不同的 ModbusTcpMaster 实例
     */
    QString ID;

    // ============================ 生命周期 ============================
    /**
     * @brief 启动 Modbus TCP Master
     * @param mode 连接模式（单连接/双连接）
     * @return 成功返回 true，失败返回 false
     */
    bool start(ModbusConnecter::ConnectionMode mode = ModbusConnecter::ConnectionMode::SingleConnection);

    /**
     * @brief 停止 Modbus TCP Master
     * @param mode 连接模式（单连接/双连接）
     */
    void stop(ModbusConnecter::ConnectionMode mode = ModbusConnecter::ConnectionMode::SingleConnection);

    // ============================ 子模块访问 ============================
    /**
     * @brief 获取连接器
     * @return ModbusConnecter 指针
     * @details 可用于配置连接参数、启动连接
     */
    ModbusConnecter* connector() const;

    /**
     * @brief 获取指令发送器
     * @return ModbusCommandSender 指针
     * @details 可用于配置超时、队列容量等
     */
    ModbusCommandSender* sender() const;

    /**
     * @brief 获取接收器
     * @return ModbusCommandReceiver 指针
     * @details 由发送器内部维护
     */
    ModbusCommandReceiver* receiver() const;

    /**
     * @brief 获取初始下发器
     * @details 由外部完成队列、间隔、重试等全部配置。
     */
    InitialCommandIssuer* initialIssuer() const;

    // ============================ 初始指令配置 ============================
    /**
     * @brief 配置初始指令队列。
     * @param queue 初始指令队列
     * @param intervalMs 指令间隔，单位 ms
     * @param executionCount 初始化队列下发轮次
     * @details Master 会缓存该配置，后续 InitialCommandIssuer 重建时自动恢复。
     */
    void configureInitialCommands(const QList<ModbusCommand>& queue, int intervalMs, int executionCount);

    // ---- 子模块访问 ----
    /**
     * @brief 获取定时发送器
     * @return PeriodicCommandSender 指针
     * @details 由外部完成队列、间隔等全部配置
     */
    PeriodicCommandSender* periodicSender() const;

    // ============================ 状态查询 ============================
    /**
     * @brief 获取当前状态机状态
     * @return 当前状态
     */
    State currentState() const;

    /**
     * @brief 判断设备是否已连接
     * @return 连接成功返回 true
     */
    bool isConnected() const;

    /**
     * @brief 获取固件版本号
     * @return 固件版本号字符串
     */
    QString firmwareVersion() const;

    /**
     * @brief 设置固件版本号
     * @param firmwareVersion 固件版本号字符串
     */
    void setFirmwareVersion(const QString& firmwareVersion);

    /**
     * @brief 获取 UI 屏版本号
     * @return UI 屏版本号字符串
     */
    QString uiScreenVersion() const;

    /**
     * @brief 设置 UI 屏版本号
     * @param uiScreenVersion UI 屏版本号字符串
     */
    void setUiScreenVersion(const QString& uiScreenVersion);

    /**
     * @brief 获取固件升级器
     * @return MtcFirmwareUpgrader 指针
     */
    FirmwareUpgrader* firmwareUpgrader() const;

    /**
     * @brief 获取 SH85 自检器
     * @return SH85SelfChecker 指针
     * @details 自检器作为 master 子控件存在，外部可调用 start()/stop() 启停自检
     */
    SH85SelfChecker* selfChecker() const;

    // ============================ 设备信息 ============================
    /**
     * @brief 获取设备 IP 地址
     * @return IP 地址字符串
     */
    QString ip() const { return m_ip; }

    /**
     * @brief 获取设备端口
     * @return 端口号
     */
    quint16 port() const { return m_port; }

    // ============================ 设备重配置 ============================
    bool reconfigureDeviceInfo(const QString& newId,
                               const QString& newIp,
                               quint16 newPort,
                               QString* errorMessage = nullptr);

private:
    // ---- 状态转换 ----
    /**
     * @brief 进入新状态
     * @param state 新状态
     */
    void enterState(State state);

    // ---- 子模块生命周期 ----
    /**
     * @brief 按需创建初始下发器
     */
    void createInitialIssuerIfNeeded();

    /**
     * @brief 暂停子控件（含断开 receiver 的 socket 信号槽）
     */
    void pauseChildren();

    /**
     * @brief 恢复子控件（含重连 receiver 的 socket 信号槽）
     */
    void resumeChildren();

    /**
     * @brief 启动发送器
     */
    void startSender();

    /**
     * @brief 启动初始下发器
     */
    void startInitialIssuer();

    /**
     * @brief 启动定时发送器
     */
    void startPeriodicSender();

    // ---- 子模块访问 ----
    /**
     * @brief 获取 Socket
     * @return QTcpSocket 指针
     */
    QTcpSocket* socket() const { return m_socket; }

signals:
    // ---- 状态通知 ----
    /**
     * @brief 错误信号
     * @param state 发生错误时的状态
     * @param message 错误描述
     * @details 部分错误不中断流程（如初始化时部分指令失败），
     *          连接错误和连续失败超阈值会导致状态进入 Error。
     */
    void errorOccurred(ModbusTcpMaster::State state, const QString& message);

    /**
     * @brief 状态机状态变更信号
     * @param state 新状态
     */
    void stateChanged(ModbusTcpMaster::State state);

private slots:
    // ---- 连接状态 ----
    /**
     * @brief 连接状态改变槽函数
     * @param status 新的连接状态
     */
    void onConnectionStatusChanged(ModbusConnecter::ConnectionStatus status, const QString& masterId);

    /**
     * @brief 连接错误槽函数
     * @param message 错误描述
     */
    void onConnectionError(const QString& message);

    // ---- 初始化状态 ----
    /**
     * @brief 初始指令完成槽函数
     * @param isOk 初始指令是否全部通过
     * @param errorMsgList 错误信息列表
     */
    void onInitialFinished(bool isOk, QStringList errorMsgList);

    // ---- 周期轮询状态 ----
    /**
     * @brief 定时发送器请求断开连接槽函数
     */
    void onPeriodicDisconnectRequested();

private:
    // ---- 设备信息 ----
    QString m_ip;                              // 设备 IP 地址。
    quint16 m_port = 0;                        // 设备端口。
    QString m_firmwareVersion;                 // 固件版本号。
    QString m_uiScreenVersion;                 // UI 屏版本号。

    // ---- 功能模块成员 ----
    QTcpSocket* m_socket = nullptr;            // TCP Socket。
    ModbusConnecter* m_connector = nullptr;    // 连接器。
    ModbusCommandSender* m_sender = nullptr;   // 指令发送器。
    InitialCommandIssuer* m_initialIssuer = nullptr;  // 初始下发器。
    PeriodicCommandSender* m_periodicSender = nullptr; // 定时发送器。
    FirmwareUpgrader* m_firmwareUpgrader = nullptr;    // 固件升级器。
    SH85SelfChecker* m_selfChecker = nullptr;          // SH85 自检器。

    // ---- 初始指令配置 ----
    QList<ModbusCommand> m_initialCommandQueue; // 初始指令队列缓存。
    int m_initialCommandIntervalMs = 1000;      // 初始指令间隔，单位 ms。
    int m_initialCommandExecutionCount = 1;     // 初始指令下发轮次。

    // ---- 运行状态 ----
    bool m_initialStarted = false;             // 初始下发器是否已启动。
    bool m_periodicStarted = false;            // 定时发送器是否已启动。
    State m_state = State::Idle;               // 当前状态机状态。
};

#endif // MODBUSTCPMASTER_H
