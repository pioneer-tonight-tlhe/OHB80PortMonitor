#ifndef MODBUSTCPMASTERPOOL_H
#define MODBUSTCPMASTERPOOL_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include <QThread>

class ModbusTcpMaster;
class ModbusConfigParser;

// ============================================================
// ModbusTcpMasterPool - ModbusTcpMaster 对象池
//
// 管理多个 ModbusTcpMaster 实例，支持多线程运行，自动平均分配对象到不同线程。
// ============================================================
class ModbusTcpMasterPool : public QObject
{
    Q_OBJECT

public:
    explicit ModbusTcpMasterPool(QObject* parent = nullptr);
    ~ModbusTcpMasterPool();

    /**
     * @brief 线程数模式枚举
     */
    enum class ThreadCountMode {
        SingleThread = 1,    // 单线程模式（1 个线程）
        TwoThreads = 2,      // 2 个线程
        ThreeThreads = 3,    // 3 个线程
        FourThreads = 4,     // 4 个线程
        FiveThreads = 5,     // 5 个线程
        SixThreads = 6,      // 6 个线程
        SevenThreads = 7,    // 7 个线程
        EightThreads = 8,    // 8 个线程
        NineThreads = 9,     // 9 个线程
        TenThreads = 10,     // 10 个线程
        MaxThreads = 0       // 最大线程模式（使用系统推荐的线程数）
    };

    /**
     * @brief 设置工作线程数
     * @param threadNum 线程数，默认为 1。传入 0 或负数时自动使用系统推荐的线程数（QThread::idealThreadCount()）
     * @details 必须在添加任何 ModbusTcpMaster 之前调用
     *          线程数会被限制在 [1, 65535] 范围内，超出范围时自动调整
     */
    void setThreadCount(int threadNum = 1);

    /**
     * @brief 使用预设模式设置工作线程数
     * @param mode 线程数模式
     * @details 必须在添加任何 ModbusTcpMaster 之前调用
     *          SingleThread: 使用 1 个线程
     *          MaxThreads: 使用系统推荐的线程数（QThread::idealThreadCount()）
     */
    void setThreadCount(ThreadCountMode mode);

    /**
     * @brief 设置配置解析器
     * @param parser 配置解析器指针
     * @details 用于在创建 Master 时初始化 InitialCommandIssuer 和 PeriodicCommandSender
     */
    void setConfigParser(ModbusConfigParser* parser);

    /**
     * @brief 创建并添加 ModbusTcpMaster 对象到池中
     * @param ip 设备 IP
     * @param port 设备端口
     * @param id Master 的唯一标识符
     * @return 返回创建的 Master 指针，失败返回 nullptr
     * @details Pool 内部负责创建和销毁 Master 对象
     *          使用轮询策略平均分配到不同线程
     *          如果设置了 ConfigParser，会自动初始化 InitialCommandIssuer 和 PeriodicCommandSender
     */
    ModbusTcpMaster* addMaster(const QString& ip, quint16 port, const QString& id);

    /**
     * @brief 根据 ID 获取 ModbusTcpMaster 对象
     * @param id ModbusTcpMaster 的唯一标识符
     * @return 找到返回指针，否则返回 nullptr
     */
    ModbusTcpMaster* getMaster(const QString& id) const;

    /**
     * @brief 获取当前工作线程数
     * @return 线程数
     */
    int threadCount() const { return m_threads.size(); }

    /**
     * @brief 获取池中 Master 总数
     * @return Master 数量
     */
    int masterCount() const { return m_mastersById.size(); }

    /**
     * @brief 获取所有 Master 的 ID 列表
     * @return ID 列表
     */
    QStringList masterIds() const;

private:
    void stopAllMasters();
    bool removeMaster(const QString& id);
    void clear();
    void initializeMaster(ModbusTcpMaster* master);

    QVector<QThread*> m_threads;                          // 线程池
    QMap<QString, ModbusTcpMaster*> m_mastersById;        // ID -> Master 映射
    QMap<ModbusTcpMaster*, int> m_masterToThreadIndex;    // Master -> 线程索引映射
    int m_nextThreadIndex;                                // 轮询索引，用于平均分配
    ModbusConfigParser* m_configParser = nullptr;         // 配置解析器引用
};

#endif // MODBUSTCPMASTERPOOL_H
