#ifndef MODBUSTCPMASTERMANAGER_H
#define MODBUSTCPMASTERMANAGER_H

#include "modbusconnecter.h"
#include "modbustcpmasterpool.h"

#include <QObject>
#include <QString>

class ModbusConfigParser;
class ModbusTcpMasterPool;
class CommandPool;
class ModbusTcpMaster;

// ============================================================
// ModbusTcpMasterManager - Modbus TCP 主控管理器
//
// 统一管理 CommandPool、ModbusConfigParser 和 ModbusTcpMasterPool。
// 负责加载配置文件，创建并初始化 ModbusTcpMaster 实例。
// ============================================================
class ModbusTcpMasterManager : public QObject
{
    Q_OBJECT

public:
    static ModbusTcpMasterManager& instance();

    /**
     * @brief 加载配置文件
     * @param xmlFilePath XML 配置文件路径
     * @return 成功返回 true，失败返回 false
     * @details 解析 XML 配置文件，初始化 CommandPool 和配置信息
     */
    bool loadConfig(const QString& xmlFilePath);

    /**
     * @brief 设置工作线程数
     * @param threadNum 线程数，默认为 1。传入 0 或负数时自动使用系统推荐的线程数
     * @details 必须在添加任何 Master 之前调用
     */
    void setThreadCount(int threadNum = 1);

    /**
     * @brief 使用预设模式设置工作线程数
     * @param mode 线程数模式
     * @details 必须在添加任何 Master 之前调用
     */
    void setThreadCount(ModbusTcpMasterPool::ThreadCountMode mode);

    /**
     * @brief 创建并添加 Master 到池中
     * @param ip 设备 IP
     * @param port 设备端口
     * @param id Master 的唯一标识符
     * @return 返回创建的 Master 指针，失败返回 nullptr
     * @details 使用配置文件中的配置初始化 Master 的 InitialCommandIssuer 和 PeriodicCommandSender
     */
    ModbusTcpMaster* addMaster(const QString& ip, quint16 port, const QString& id);

    /**
     * @brief 根据 ID 获取 Master
     * @param id Master 的唯一标识符
     * @return 找到返回指针，否则返回 nullptr
     */
    ModbusTcpMaster* getMaster(const QString& id) const;

    /**
     * @brief 启动指定 Master 的连接流程
     * @param id Master 的唯一标识符
     * @param mode 连接模式
     * @return 找到目标 Master 返回 true，否则返回 false
     * @details 内部会在 Master 所在线程中执行启动逻辑
     */
    bool startMaster(const QString& id,
                     ModbusConnecter::ConnectionMode mode = ModbusConnecter::ConnectionMode::SingleConnection);

    /**
     * @brief 获取当前工作线程数
     * @return 线程数
     */
    int threadCount() const;

    /**
     * @brief 获取 Master 总数
     * @return Master 数量
     */
    int masterCount() const;

    /**
     * @brief 获取所有 Master 的 ID 列表
     * @return ID 列表
     */
    QStringList masterIds() const;

    /**
     * @brief 获取配置解析器
     * @return 配置解析器指针
     */
    ModbusConfigParser* configParser() const;

    /**
     * @brief 获取 Master 对象池
     * @return Master 对象池指针
     */
    ModbusTcpMasterPool* masterPool() const;

    /**
     * @brief 获取指令池
     * @return 指令池指针
     */
    CommandPool* commandPool() const;

    /**
     * @brief 优雅关闭：停止所有 Master、销毁工作线程
     * @details 必须在 QApplication 仍存活时调用（例如 App::cleanup() 中）。
     *          若不主动调用，待静态析构阶段再销毁 Pool，QApplication 已消亡，
     *          其中的 QMetaObject::invokeMethod / QThread::quit+wait 会崩溃。
     *          可重复调用，第二次是 no-op。
     */
    void shutdown();

private:
    explicit ModbusTcpMasterManager(QObject* parent = nullptr);
    ~ModbusTcpMasterManager();

    ModbusTcpMasterManager(const ModbusTcpMasterManager&) = delete;
    ModbusTcpMasterManager& operator=(const ModbusTcpMasterManager&) = delete;

    void initializeMaster(ModbusTcpMaster* master);

    ModbusConfigParser* m_configParser = nullptr;
    ModbusTcpMasterPool* m_masterPool = nullptr;
    CommandPool* m_commandPool = nullptr;
};

#endif // MODBUSTCPMASTERMANAGER_H
