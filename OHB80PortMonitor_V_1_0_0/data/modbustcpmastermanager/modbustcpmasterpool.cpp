#include "modbustcpmasterpool.h"
#include "modbustcpmaster.h"
#include "modbustcpmaster/firmwareupgrader.h"
#include "modbusconfigparser.h"
#include "modbuslogger.h"
#include "firmwareconfig.h"
#include "initialcommandissuer.h"
#include "periodiccommandsender.h"
#include <QMetaObject>
#include <QDebug>
#include <QtGlobal>

// 最大线程数限制
static constexpr int MAX_THREAD_COUNT = 65535;

ModbusTcpMasterPool::ModbusTcpMasterPool(QObject* parent)
    : QObject(parent)
    , m_nextThreadIndex(0)
{
}

ModbusTcpMasterPool::~ModbusTcpMasterPool()
{
    qDebug() << "[data][ModbusTcpMasterPool] 析构开始，开始安全清理资源...";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "destructor", "析构开始，开始安全清理资源");

    // ── 第一步：同步停止所有 Master ──────────────────────────
    // BlockingQueuedConnection 保证每个 stop() 在 Master 所属线程上完成后才返回
    stopAllMasters();

    // ── 第二步：调度 Master 销毁（仅 deleteLater，不再二次 stop） ──
    // stop() 已在上一步同步完成，此处只需安排对象释放
    qDebug() << "[data][ModbusTcpMasterPool] 调度删除" << m_mastersById.size() << "个 Master";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "destructor",
        QString("调度删除 %1 个 Master").arg(m_mastersById.size()));
    for (auto it = m_mastersById.begin(); it != m_mastersById.end(); ++it) {
        ModbusTcpMaster* master = it.value();
        if (master) {
            master->deleteLater();
        }
    }
    m_mastersById.clear();
    m_masterToThreadIndex.clear();

    // ── 第三步：向所有线程并行发出 quit 信号 ─────────────────
    // quit() 在 deleteLater 之后投递，事件循环按 FIFO 处理：
    //   先执行 DeferredDelete（销毁 Master），再退出事件循环
    qDebug() << "[data][ModbusTcpMasterPool] 向" << m_threads.size() << "个工作线程发出退出信号";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "destructor",
        QString("向 %1 个工作线程发出退出信号").arg(m_threads.size()));
    for (QThread* thread : qAsConst(m_threads)) {
        if (thread) {
            thread->quit();
        }
    }

    // ── 第四步：逐一等待线程退出并释放 ───────────────────────
    for (int i = 0; i < m_threads.size(); ++i) {
        QThread* thread = m_threads[i];
        if (!thread) {
            continue;
        }

        if (!thread->wait(5000)) {
            qWarning() << "[data][ModbusTcpMasterPool] 线程" << i << thread->objectName() << "优雅退出超时，强制终止";
            ModbusLogger::systemWarn("ModbusTcpMasterPool", "destructor",
                QString("线程 %1 %2 优雅退出超时，强制终止").arg(i).arg(thread->objectName()));
            thread->terminate();
            thread->wait(1000);
        }

        delete thread;
    }

    m_threads.clear();
    qDebug() << "[data][ModbusTcpMasterPool] 析构完成，所有资源已清理";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "destructor", "析构完成，所有资源已清理");
}

void ModbusTcpMasterPool::setThreadCount(int threadNum)
{
    // 如果已经有 Master，不允许修改线程数
    if (!m_mastersById.isEmpty()) {
        qWarning() << "[data][ModbusTcpMasterPool] 无法修改线程数：池中已有 Master 对象";
        ModbusLogger::systemWarn("ModbusTcpMasterPool", "setThreadCount",
            "无法修改线程数：池中已有 Master 对象");
        return;
    }

    // 如果传入 0 或负数，使用系统推荐的线程数
    if (threadNum <= 0) {
        threadNum = QThread::idealThreadCount();
        qDebug() << "[data][ModbusTcpMasterPool] 自动检测系统推荐线程数：" << threadNum;
        ModbusLogger::systemInfo("ModbusTcpMasterPool", "setThreadCount",
            QString("自动检测系统推荐线程数：%1").arg(threadNum));
    }

    // 限制在 [1, MAX_THREAD_COUNT] 范围内
    if (threadNum < 1) {
        threadNum = 1;
    } else if (threadNum > MAX_THREAD_COUNT) {
        qWarning() << "[data][ModbusTcpMasterPool] 请求的线程数" << threadNum
                   << "超过最大限制" << MAX_THREAD_COUNT << "，已调整为" << MAX_THREAD_COUNT;
        ModbusLogger::systemWarn("ModbusTcpMasterPool", "setThreadCount",
            QString("请求的线程数 %1 超过最大限制 %2，已调整").arg(threadNum).arg(MAX_THREAD_COUNT));
        threadNum = MAX_THREAD_COUNT;
    }

    // 清理旧线程
    for (QThread* thread : m_threads) {
        if (thread) {
            thread->quit();
            if (!thread->wait(3000)) {
                thread->terminate();
                thread->wait(1000);
            }
            delete thread;
        }
    }
    m_threads.clear();

    // 创建新线程
    m_threads.reserve(threadNum);
    for (int i = 0; i < threadNum; ++i) {
        QThread* thread = new QThread(this);
        thread->setObjectName(QString("ModbusPoolThread-%1").arg(i));
        thread->start();
        m_threads.append(thread);
    }

    m_nextThreadIndex = 0;
    qDebug() << "[data][ModbusTcpMasterPool] 已创建" << threadNum << "个工作线程";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "setThreadCount",
        QString("已创建 %1 个工作线程").arg(threadNum));
}

void ModbusTcpMasterPool::setThreadCount(ThreadCountMode mode)
{
    // 先获取系统能够提供的最大线程数
    int systemMaxThreads = QThread::idealThreadCount();
    qDebug() << "[data][ModbusTcpMasterPool] 系统最大可用线程数：" << systemMaxThreads;
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "setThreadCount",
        QString("系统最大可用线程数：%1").arg(systemMaxThreads));

    int requestedThreads = 0;

    switch (mode) {
        case ThreadCountMode::SingleThread:
            requestedThreads = 1;
            break;
        case ThreadCountMode::TwoThreads:
            requestedThreads = 2;
            break;
        case ThreadCountMode::ThreeThreads:
            requestedThreads = 3;
            break;
        case ThreadCountMode::FourThreads:
            requestedThreads = 4;
            break;
        case ThreadCountMode::FiveThreads:
            requestedThreads = 5;
            break;
        case ThreadCountMode::SixThreads:
            requestedThreads = 6;
            break;
        case ThreadCountMode::SevenThreads:
            requestedThreads = 7;
            break;
        case ThreadCountMode::EightThreads:
            requestedThreads = 8;
            break;
        case ThreadCountMode::NineThreads:
            requestedThreads = 9;
            break;
        case ThreadCountMode::TenThreads:
            requestedThreads = 10;
            break;
        case ThreadCountMode::MaxThreads:
            requestedThreads = systemMaxThreads;
            break;
    }

    // 以系统最大线程数为最终评判依据
    if (requestedThreads > systemMaxThreads) {
        qWarning() << "[data][ModbusTcpMasterPool] 请求的线程数" << requestedThreads
                   << "超过系统最大可用线程数" << systemMaxThreads
                   << "，已调整为系统最大线程数";
        ModbusLogger::systemWarn("ModbusTcpMasterPool", "setThreadCount",
            QString("请求的线程数 %1 超过系统最大可用线程数 %2，已调整")
                .arg(requestedThreads).arg(systemMaxThreads));
        requestedThreads = systemMaxThreads;
    }

    qDebug() << "[data][ModbusTcpMasterPool] 最终设置线程数：" << requestedThreads;
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "setThreadCount",
        QString("最终设置线程数：%1").arg(requestedThreads));
    setThreadCount(requestedThreads);
}

ModbusTcpMaster* ModbusTcpMasterPool::addMaster(const QString& ip, quint16 port, const QString& id)
{
    if (id.isEmpty()) {
        qWarning() << "[data][ModbusTcpMasterPool] ID 为空，无法创建 Master";
        ModbusLogger::systemWarn("ModbusTcpMasterPool", "addMaster", "ID 为空，无法创建 Master");
        return nullptr;
    }

    if (m_mastersById.contains(id)) {
        qWarning() << "[data][ModbusTcpMasterPool] 已存在 ID 为" << id << "的 Master";
        ModbusLogger::systemWarn("ModbusTcpMasterPool", "addMaster",
            QString("已存在 ID=%1 的 Master").arg(id));
        return nullptr;
    }

    // 如果线程池为空，默认创建一个线程
    if (m_threads.isEmpty()) {
        setThreadCount(1);
    }

    // 轮询分配线程
    int threadIndex = m_nextThreadIndex;
    m_nextThreadIndex = (m_nextThreadIndex + 1) % m_threads.size();

    QThread* targetThread = m_threads[threadIndex];
    if (!targetThread) {
        qWarning() << "[data][ModbusTcpMasterPool] 线程" << threadIndex << "无效";
        ModbusLogger::systemWarn("ModbusTcpMasterPool", "addMaster",
            QString("线程 %1 无效").arg(threadIndex));
        return nullptr;
    }

    // 创建 Master 对象（不设置父对象，由 Pool 管理生命周期）
    ModbusTcpMaster* master = new ModbusTcpMaster(ip, port, id, nullptr);

    // 将 Master 移动到目标线程
    master->moveToThread(targetThread);

    // 添加到映射
    m_mastersById[id] = master;
    m_masterToThreadIndex[master] = threadIndex;

    qDebug() << "[data][ModbusTcpMasterPool] 已创建并添加 Master (ID:" << id
             << ", IP:" << ip << ", Port:" << port
             << ") 到线程" << threadIndex << targetThread->objectName();
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "addMaster",
        QString("已创建并添加 Master ID=%1 IP=%2 Port=%3 到线程 %4 %5")
            .arg(id).arg(ip).arg(port).arg(threadIndex).arg(targetThread->objectName()));

    // 如果设置了配置解析器，初始化 Master
    if (m_configParser) {
        initializeMaster(master);
    }

    return master;
}

void ModbusTcpMasterPool::setConfigParser(ModbusConfigParser* parser)
{
    m_configParser = parser;
    qDebug() << "[data][ModbusTcpMasterPool] 已设置配置解析器";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "setConfigParser", "已设置配置解析器");
}

void ModbusTcpMasterPool::initializeMaster(ModbusTcpMaster* master)
{
    if (!master || !m_configParser) {
        return;
    }

    qDebug() << "[data][ModbusTcpMasterPool] 开始初始化 Master (ID:" << master->ID << ")";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "initializeMaster",
        QString("开始初始化 Master ID=%1").arg(master->ID));

    // 获取配置
    auto initialConfig = m_configParser->initialConfig();
    auto periodicConfig = m_configParser->periodicConfig();
    auto initialQueue = m_configParser->initialCommandQueue();
    auto periodicQueue = m_configParser->periodicCommandQueue();

    // 配置发送器
    auto sender = master->sender();
    if (sender) {
        sender->setQueueCapacity(32);
    }

    // 配置 InitialCommandIssuer
    if (!initialQueue.isEmpty()) {
        const int initialExecutionCount = qMax(1, initialConfig.retryCount + 1);
        master->configureInitialCommands(initialQueue, initialConfig.interval, initialExecutionCount);
        qDebug() << "[data][ModbusTcpMasterPool] 已配置 InitialCommandIssuer:"
                 << "指令数=" << initialQueue.size()
                 << "间隔=" << initialConfig.interval
                 << "超时=" << initialConfig.timeout
                 << "失败重试轮次=" << initialConfig.retryCount
                 << "总轮次=" << initialExecutionCount;
        ModbusLogger::systemInfo("ModbusTcpMasterPool", "InitialCommandIssuer", "initializeMaster",
            QString("已配置 InitialCommandIssuer 指令数=%1 间隔=%2 超时=%3 失败重试轮次=%4 总轮次=%5")
                .arg(initialQueue.size())
                .arg(initialConfig.interval)
                .arg(initialConfig.timeout)
                .arg(initialConfig.retryCount)
                .arg(initialExecutionCount));
    }

    // 配置 PeriodicCommandSender
    auto periodicSender = master->periodicSender();
    if (!periodicQueue.isEmpty() && periodicSender) {
        periodicSender->setCommandQueue(periodicQueue);
        periodicSender->setInterval(periodicConfig.interval);
        qDebug() << "[data][ModbusTcpMasterPool] 已配置 PeriodicCommandSender:"
                 << "指令数=" << periodicQueue.size()
                 << "间隔=" << periodicConfig.interval
                 << "超时=" << periodicConfig.timeout
                 << "重试=" << periodicConfig.retryCount;
        ModbusLogger::systemInfo("ModbusTcpMasterPool", "PeriodicCommandSender", "initializeMaster",
            QString("已配置 PeriodicCommandSender 指令数=%1 间隔=%2 超时=%3 重试=%4")
                .arg(periodicQueue.size()).arg(periodicConfig.interval).arg(periodicConfig.timeout).arg(periodicConfig.retryCount));
    }

    // 初始化固件升级模块
    auto upgrader = master->firmwareUpgrader();
    if (upgrader) {
        FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
        upgrader->setPrepareTimeout(fwConfig.prepareCmdTimeoutMs());
        upgrader->setWaitingTime(fwConfig.waitingForEquipmentReadyMs());
        upgrader->setSendInterval(fwConfig.sendIntervalForDataMs());
        upgrader->setTransferTimeout(fwConfig.transferResponseTimeoutMs());
        
        qDebug() << "[data][ModbusTcpMasterPool] 已配置 FirmwareUpgrader:"
                 << "PrepareTimeout=" << fwConfig.prepareCmdTimeoutMs() << "ms"
                 << "WaitingTime=" << fwConfig.waitingForEquipmentReadyMs() << "ms"
                 << "SendInterval=" << fwConfig.sendIntervalForDataMs() << "ms"
                 << "TransferTimeout=" << fwConfig.transferResponseTimeoutMs() << "ms";
        ModbusLogger::systemInfo("ModbusTcpMasterPool", "FirmwareUpgrader", "initializeMaster",
            QString("已配置 FirmwareUpgrader PrepareTimeout=%1ms WaitingTime=%2ms SendInterval=%3ms TransferTimeout=%4ms")
                .arg(fwConfig.prepareCmdTimeoutMs()).arg(fwConfig.waitingForEquipmentReadyMs())
                .arg(fwConfig.sendIntervalForDataMs()).arg(fwConfig.transferResponseTimeoutMs()));
    }
}

ModbusTcpMaster* ModbusTcpMasterPool::getMaster(const QString& id) const
{
    return m_mastersById.value(id, nullptr);
}

bool ModbusTcpMasterPool::renameMaster(const QString& oldId, const QString& newId, QString* errorMessage)
{
    if (oldId == newId) {
        if (errorMessage) errorMessage->clear();
        return true;
    }

    if (!m_mastersById.contains(oldId)) {
        if (errorMessage) *errorMessage = QString("Master %1 does not exist").arg(oldId);
        return false;
    }

    if (m_mastersById.contains(newId)) {
        if (errorMessage) *errorMessage = QString("Master %1 already exists").arg(newId);
        return false;
    }

    ModbusTcpMaster* master = m_mastersById.take(oldId);
    m_mastersById.insert(newId, master);
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "renameMaster",
        QString("Renamed master key oldId=%1 newId=%2").arg(oldId, newId));

    if (errorMessage) errorMessage->clear();
    return true;
}

QStringList ModbusTcpMasterPool::masterIds() const
{
    return m_mastersById.keys();
}

bool ModbusTcpMasterPool::removeMaster(const QString& id)
{
    ModbusTcpMaster* master = m_mastersById.value(id, nullptr);
    if (!master) {
        return false;
    }

    // 从映射中移除
    m_mastersById.remove(id);
    m_masterToThreadIndex.remove(master);

    QMetaObject::invokeMethod(master, [master]() {
        master->stop(ModbusConnecter::ConnectionMode::AutoReconnect);
        master->deleteLater();
    }, Qt::QueuedConnection);

    qDebug() << "[data][ModbusTcpMasterPool] 已从池中移除并删除 Master (ID:" << id << ")";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "removeMaster",
        QString("已从池中移除并删除 Master ID=%1").arg(id));
    return true;
}

void ModbusTcpMasterPool::clear()
{
    if (m_mastersById.isEmpty()) {
        return;
    }

    qDebug() << "[data][ModbusTcpMasterPool] 清空池，删除" << m_mastersById.size() << "个 Master";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "clear",
        QString("清空池，删除 %1 个 Master").arg(m_mastersById.size()));

    for (auto it = m_mastersById.begin(); it != m_mastersById.end(); ++it) {
        ModbusTcpMaster* master = it.value();
        if (master) {
            QMetaObject::invokeMethod(master, [master]() {
                master->stop(ModbusConnecter::ConnectionMode::AutoReconnect);
                master->deleteLater();
            }, Qt::QueuedConnection);
        }
    }

    m_mastersById.clear();
    m_masterToThreadIndex.clear();
}

void ModbusTcpMasterPool::stopAllMasters()
{
    qDebug() << "[data][ModbusTcpMasterPool] 开始停止所有 Master...";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "stopAllMasters", "开始停止所有 Master");

    for (auto it = m_mastersById.begin(); it != m_mastersById.end(); ++it) {
        ModbusTcpMaster* master = it.value();
        if (master) {
            qDebug() << "[data][ModbusTcpMasterPool] 停止 Master (ID:" << master->ID << ")";
            ModbusLogger::systemInfo("ModbusTcpMasterPool", "stopAllMasters",
                QString("停止 Master ID=%1").arg(master->ID));
            QMetaObject::invokeMethod(master, [master]() {
                master->stop(ModbusConnecter::ConnectionMode::AutoReconnect);
            }, Qt::BlockingQueuedConnection);
        }
    }

    qDebug() << "[data][ModbusTcpMasterPool] 所有 Master 已停止";
    ModbusLogger::systemInfo("ModbusTcpMasterPool", "stopAllMasters", "所有 Master 已停止");
}
