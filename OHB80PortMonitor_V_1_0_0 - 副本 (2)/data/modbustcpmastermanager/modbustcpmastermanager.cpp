#include "modbustcpmastermanager.h"
#include "modbusconfigparser.h"
#include "modbustcpmasterpool.h"
#include "commandpool.h"
#include "modbustcpmaster.h"
#include "modbuslogger.h"
#include <QMetaObject>
#include <QDebug>

ModbusTcpMasterManager& ModbusTcpMasterManager::instance()
{
    static ModbusTcpMasterManager manager;
    return manager;
}

ModbusTcpMasterManager::ModbusTcpMasterManager(QObject* parent)
    : QObject(parent)
    , m_configParser(new ModbusConfigParser())
    , m_masterPool(new ModbusTcpMasterPool(this))
    , m_commandPool(new CommandPool())
{
    qDebug() << "[data][ModbusTcpMasterManager] 已创建管理器";
    ModbusLogger::systemInfo("ModbusTcpMasterManager", "constructor", "已创建管理器");
}

ModbusTcpMasterManager::~ModbusTcpMasterManager()
{
    qDebug() << "[data][ModbusTcpMasterManager] 析构管理器";
    // 兜底：如果未主动 shutdown()，此处尝试清理（可能已处于静态析构阶段，但总比不做强）
    shutdown();

    delete m_configParser;  m_configParser = nullptr;
    delete m_commandPool;   m_commandPool  = nullptr;
}

void ModbusTcpMasterManager::shutdown()
{
    if (!m_masterPool) return; // 已清理，幂等返回

    qDebug() << "[data][ModbusTcpMasterManager] shutdown: 销毁 Master 池及工作线程";
    ModbusLogger::systemInfo("ModbusTcpMasterManager", "shutdown", "销毁 Master 池及工作线程");

    // 主动销毁 Pool，触发 ~ModbusTcpMasterPool()：
    //   stopAllMasters() -> clear() -> quit/wait/delete 每个 QThread
    // 必须在 QApplication 仍活时执行，跨线程调用才可靠。
    delete m_masterPool;
    m_masterPool = nullptr;
}

bool ModbusTcpMasterManager::loadConfig(const QString& xmlFilePath)
{
    qDebug() << "[data][ModbusTcpMasterManager] 开始加载配置文件:" << xmlFilePath;
    ModbusLogger::systemInfo("ModbusTcpMasterManager", "loadConfig",
        QString("开始加载配置文件：%1").arg(xmlFilePath));

    if (!m_configParser->parse(xmlFilePath)) {
        qWarning() << "[data][ModbusTcpMasterManager] 配置文件解析失败:" << m_configParser->errorMessage();
        ModbusLogger::systemError("ModbusTcpMasterManager", "loadConfig",
            QString("配置文件解析失败：%1").arg(m_configParser->errorMessage()));
        return false;
    }

    // 更新指令池
    *m_commandPool = m_configParser->pool();

    // 设置配置解析器到 MasterPool
    m_masterPool->setConfigParser(m_configParser);

    qDebug() << "[data][ModbusTcpMasterManager] 配置文件加载成功";
    ModbusLogger::systemInfo("ModbusTcpMasterManager", "loadConfig", "配置文件加载成功");
    return true;
}

void ModbusTcpMasterManager::setThreadCount(int threadNum)
{
    m_masterPool->setThreadCount(threadNum);
}

void ModbusTcpMasterManager::setThreadCount(ModbusTcpMasterPool::ThreadCountMode mode)
{
    m_masterPool->setThreadCount(mode);
}

ModbusTcpMaster* ModbusTcpMasterManager::addMaster(const QString& ip, quint16 port, const QString& id)
{
    qDebug() << "[data][ModbusTcpMasterManager] 创建 Master (ID:" << id << ", IP:" << ip << ", Port:" << port << ")";
    ModbusLogger::systemInfo("ModbusTcpMasterManager", "addMaster",
        QString("创建 Master ID=%1 IP=%2 Port=%3").arg(id).arg(ip).arg(port));
    return m_masterPool->addMaster(ip, port, id);
}

ModbusTcpMaster* ModbusTcpMasterManager::getMaster(const QString& id) const
{
    return m_masterPool->getMaster(id);
}

bool ModbusTcpMasterManager::startMaster(const QString& id, ModbusConnecter::ConnectionMode mode)
{
    ModbusTcpMaster* master = m_masterPool->getMaster(id);
    if (!master) {
        qWarning() << "[data][ModbusTcpMasterManager] 未找到要启动的 Master, ID:" << id;
        ModbusLogger::systemWarn("ModbusTcpMasterManager", "startMaster",
            QString("未找到要启动的 Master ID=%1").arg(id));
        return false;
    }

    QMetaObject::invokeMethod(master, [master, mode]() {
        master->start(mode);
    }, Qt::QueuedConnection);

    qDebug() << "[data][ModbusTcpMasterManager] 已投递启动请求, ID:" << id;
    ModbusLogger::systemInfo("ModbusTcpMasterManager", "startMaster",
        QString("已投递启动请求 ID=%1").arg(id));
    return true;
}

int ModbusTcpMasterManager::threadCount() const
{
    return m_masterPool->threadCount();
}

int ModbusTcpMasterManager::masterCount() const
{
    return m_masterPool->masterCount();
}

QStringList ModbusTcpMasterManager::masterIds() const
{
    return m_masterPool->masterIds();
}

ModbusConfigParser* ModbusTcpMasterManager::configParser() const
{
    return m_configParser;
}

ModbusTcpMasterPool* ModbusTcpMasterManager::masterPool() const
{
    return m_masterPool;
}

CommandPool* ModbusTcpMasterManager::commandPool() const
{
    return m_commandPool;
}
