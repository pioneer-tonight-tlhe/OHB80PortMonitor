# 调度层（Scheduler）设计方案与代码实现详解

## 一、背景与需求

### 1.1 项目背景

本项目管理 **80 个 ModbusTcpMaster 设备**（OHB1~OHB80），每个设备通过 TCP 协议进行 Modbus 通信。设备管理涉及大量异步操作，这些操作不能直接在 UI 线程中执行，否则会导致界面卡顿甚至无响应。

为此，我们引入了一个 **调度层（Scheduler）**，它运行在独立线程中，管理两种类型的任务：
- **普通任务**：有明确的开始和结束，执行完自动回收（如数据初始化、固件升级）
- **长驻任务**：启动后永久运行，持续监控设备状态直到被主动停止（如连接监控、实时数据轮询）

### 1.2 需要支持的任务场景

| 任务场景 | 类型 | 实现状态 | 特点 | 持续时间 |
|---------|------|---------|------|---------|
| 初始化端口数据表格 | 普通任务 | ✅ 已实现 | 一次性、读取配置并推送 UI 数据 | 瞬间完成 |
| 连接所有设备并监控状态 | **长驻任务** | ✅ 已实现 | 启动连接后持续监控所有设备连接状态变化 | 应用全生命周期 |
| 监控设备实时数据 | **长驻任务** | ✅ 已实现 | 监听 ReadStatus 响应，解析并上报压力/流量/湿度等数据 | 应用全生命周期 |
| 固件升级 | 普通任务 | 🔲 待实现 | 多步骤、长耗时、单设备或批量 | 几分钟~几十分钟 |
| 下发多条指令 | 普通任务 | 🔲 待实现 | 顺序执行、单设备 | 几秒~几十秒 |
| 充气/关气定时控制 | 普通任务 | 🔲 待实现 | 计时器协调、循环执行 | 几分钟~几小时 |

### 1.3 核心约束

1. **不能阻塞 UI 线程** — 所有业务逻辑必须在后台线程执行
2. **长驻任务与普通任务共存** — 连接监控、数据轮询等需要贯穿应用全生命周期，**不能占用普通任务的并发槽位**
3. **跨线程安全** — 设备对象（ModbusTcpMaster）运行在各自的工作线程中，调度器和 UI 各有自己的线程
4. **统一的数据上报机制** — 不同任务产生不同类型的数据，UI 层需要一个统一入口接收并分发

---

## 二、整体架构

### 2.1 三层架构图

```
┌──────────────────────────────────────────────────────────────┐
│                    UI 层 (MainWindow)                          │
│  ┌────────────────────────────────────────────────────────┐   │
│  │  listWidget / 各种控件                                  │   │
│  │                                                          │   │
│  │  initScheduler() {                                       │   │
│  │      connect(scheduler::taskDataResult → lambda分发)      │   │
│  │      submitTask(InitPortDataTask)   // 普通任务            │   │
│  │      submitTask(ConnectAllTask)     // 长驻任务            │   │
│  │  }                                                       │   │
│  └───────────────────────┬────────────────────────────────┘   │
│                           │ 信号/槽 (跨线程自动 QueuedConnection) │
└───────────────────────────┼──────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────┐
│                Scheduler 调度器 (独立线程)                      │
│                                                                │
│  ┌── 长驻任务区 (m_persistentTasks) ──────────────────────┐   │
│  │  不限数量 · 立即启动 · 不占并发槽位 · 永不自动回收        │   │
│  │                                                          │   │
│  │  [ConnectAllTask]  [MonitorDataTask]  ...                │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                │
│  ┌── 普通任务区 ─────────────────────────────────────────┐   │
│  │  m_pendingQueue → FIFO 待执行队列                       │   │
│  │  m_runningTasks → 运行中集合 (≤ m_maxConcurrent = 10)   │   │
│  │                                                          │   │
│  │  [InitPortDataTask] [FirmwareTask(未来)] ...              │   │
│  └──────────────────────────────────────────────────────────┘   │
│                                                                │
│  ┌── SchedulerTask 任务基类 ──────────────────────────────┐   │
│  │  isPersistent() == true  → 长驻任务                      │   │
│  │  isPersistent() == false → 普通任务（默认）               │   │
│  └──────────────────────────────────────────────────────────┘   │
└───────────────────────────┬──────────────────────────────────┘
                            │
                            │ QMetaObject::invokeMethod()
                            │ (跨线程调用 master 方法)
                            ▼
┌──────────────────────────────────────────────────────────────┐
│          ModbusTcpMasterManager (Master 工作线程池)             │
│                                                                │
│  Thread-1: [Master_1] [Master_2] ...                           │
│  Thread-2: [Master_3] [Master_4] ...                           │
│  Thread-N: [Master_79] [Master_80]                             │
│                                                                │
│  每个 Master 运行在分配的工作线程中                               │
│  TCP 通信在各自线程中完成，互不阻塞                                │
└──────────────────────────────────────────────────────────────┘
```

### 2.2 线程分布

| 线程 | 运行内容 | 职责 |
|------|---------|------|
| **UI 线程** | MainWindow | 界面显示、用户交互、接收数据并渲染 |
| **调度器线程** | Scheduler + 所有 SchedulerTask | 任务调度、业务逻辑、状态跟踪、进度上报 |
| **Master 工作线程 1~N** | ModbusTcpMaster 对象 | TCP 连接、Modbus 数据收发 |

> **关键**：所有 `SchedulerTask` 对象（无论普通还是长驻）都运行在**同一个调度器线程**中。它们通过事件循环轮流执行，而不是各自占用独立线程。这极大降低了线程管理的复杂度。

### 2.3 跨线程通信方式

```
UI 线程                      调度器线程                      Master 工作线程
────────                      ──────────                      ────────────

submitTask(task)  ─────→  task 入队 / 立即启动
    (直接调用，QMutex 保护)

                          QMetaObject::invokeMethod    ─→  master->connectWithReconnect()
                          (task, "start",                  (在 master 所属线程执行)
                           Qt::QueuedConnection)

                          task::onStateChanged()      ←─  master::connectionStateChanged
                          (Qt::QueuedConnection)            (跨线程信号)

lambda(key,data)  ←─────  emit dataResult(key, data)
(Qt::AutoConnection,       (task 在调度器线程中发出)
 跨线程自动队列化)
```

**所有跨线程通信汇总**：

| 方向 | 机制 | 原因 |
|------|------|------|
| UI → 调度器 | `submitTask()` 直接调用 + `QMutex` | 只操作队列数据结构，需要加锁保护 |
| 调度器 → Task | `QMetaObject::invokeMethod(Qt::QueuedConnection)` | 确保 `start()` 在调度器事件循环中执行 |
| Task → Master | `QMetaObject::invokeMethod(Qt::QueuedConnection)` | Master 在工作线程中，不能直接调用 |
| Master → Task | `connect(..., Qt::QueuedConnection)` | 信号跨线程，需要队列化 |
| Task → 调度器 | 直接信号（同线程） | Task 和 Scheduler 在同一线程 |
| 调度器 → UI | `connect(scheduler, signal, mainwindow, slot)` | Qt 自动检测跨线程，使用 `QueuedConnection` |

---

## 三、核心类详解

### 3.1 SchedulerTask — 任务基类

**文件**：`scheduler/scheduler_task.h`（纯头文件，无 .cpp）

这是所有任务的抽象基类，定义了**统一接口**、**生命周期**和**任务分类**。

#### 3.1.1 任务状态

```cpp
enum State { Pending, Running, Paused, Finished, Failed, Cancelled };
```

状态转换图：

```
                    ┌── pause() ──→ Paused
                    │                 │
Pending ─ start() → Running          resume()
                    │                 │
                    │ ←───────────────┘
                    │
                    ├── 正常完成 ──→ Finished    (普通任务才会到达)
                    ├── stop()   ──→ Cancelled   (所有任务)
                    └── 出错     ──→ Failed
```

> **长驻任务的特殊性**：长驻任务进入 `Running` 后不会自动到达 `Finished`，只有 `stop()` 能使其变为 `Cancelled`。

#### 3.1.2 任务分类 — 普通任务 vs 长驻任务

```cpp
virtual bool isPersistent() const { return false; }  // 默认为普通任务
```

这是调度器区分两种任务的**唯一依据**，子类重写此方法即可声明为长驻任务：

| 特性 | 普通任务 (`isPersistent() = false`) | 长驻任务 (`isPersistent() = true`) |
|------|-------------------------------------|-------------------------------------|
| 生命周期 | 执行完毕后 `Finished`，调度器自动回收 | 始终 `Running`，不会自动结束 |
| 提交后行为 | 入 `m_pendingQueue`，等待 `scheduleNext()` 调度 | **立即启动**，不入队 |
| 并发管理 | 存入 `m_runningTasks`，占用并发槽位 | 存入 `m_persistentTasks`，**不占并发槽位** |
| 结束方式 | `emit finished()` 或 `cancelTask()` | 仅 `cancelTask()` 或 `Scheduler::stop()` |
| 完成后回调 | `onTaskFinished()` → 清理 → `scheduleNext()` | 无（永不触发 `onTaskFinished`） |
| 典型场景 | 数据初始化、固件升级、指令下发 | 连接监控、实时数据轮询 |

#### 3.1.3 纯虚接口

| 方法 | 修饰 | 说明 |
|------|------|------|
| `start()` | `Q_INVOKABLE virtual` | 启动任务。子类在此实现业务逻辑。标记 `Q_INVOKABLE` 使其可被 `QMetaObject::invokeMethod` 跨线程调用 |
| `stop()` | `Q_INVOKABLE virtual` | 停止任务。子类在此清理资源、断开信号、设置 `Cancelled` 状态 |
| `taskType()` | `virtual` | 返回任务类型名字符串，如 `"ConnectAllTask"`，用于日志和调试 |

#### 3.1.4 信号

| 信号 | 参数 | 用途 | 调度器转发名 |
|------|------|------|------------|
| `stateChanged` | `State newState` | 状态变更通知 | `taskStateChanged` |
| `progress` | `int percent, QString msg` | 进度上报（0-100%） | `taskProgress` |
| `finished` | `bool success, QString msg` | 任务完成通知（**长驻任务正常运行时不发此信号**） | `taskFinished` |
| `dataResult` | `QString key, QVariantMap data` | 通用数据上报 | `taskDataResult` |

> 调度器充当**信号中转站**：Task → Scheduler → UI。UI 只需连接 Scheduler 的信号，无需知道具体 Task 对象。

#### 3.1.5 为什么使用 QVariantMap 作为数据载体？

`dataResult` 信号的 `QVariantMap` 是一个**灵活的键值对容器**，不同任务可以传递完全不同结构的数据：

| 任务 | key | data 示例 |
|------|-----|----------|
| InitPortDataTask | `"init_port_data"` | `{"action":"add_row", "index":0, "qrcode":"12034", "status":"未连接", "pressure":0.0, ...}` |
| ConnectAllTask | 设备ID（如 `"12034"`） | `{"qrcode":"12034", "status":"Connected"}` |
| MonitorDataTask（未来） | `"monitor_data"` | `{"qrcode":"12034", "pressure":12.5, "flow":23.4, "humidity":45.6}` |
| FirmwareTask（未来） | `"firmware"` | `{"qrcode":"12034", "step":"传输中", "version":"1.2.0"}` |

UI 层通过 `key` 值判断数据来源，在**一个统一的 lambda** 中分发处理，无需为每种任务定义专用信号。

#### 3.1.6 完整代码

```cpp
class SchedulerTask : public QObject
{
    Q_OBJECT

public:
    enum State { Pending, Running, Paused, Finished, Failed, Cancelled };
    Q_ENUM(State)

    enum Priority { Low, Normal, High, Urgent };
    Q_ENUM(Priority)

    explicit SchedulerTask(QObject *parent = nullptr)
        : QObject(parent)
        , m_taskId(QUuid::createUuid().toString(QUuid::WithoutBraces))
        , m_state(Pending)
        , m_priority(Normal)
    {}

    virtual ~SchedulerTask() = default;

    // 子类必须实现
    Q_INVOKABLE virtual void start() = 0;
    Q_INVOKABLE virtual void stop() = 0;
    virtual QString taskType() const = 0;

    // 可选重写
    virtual void pause() {}
    virtual void resume() {}

    // 属性访问
    QString taskId() const { return m_taskId; }
    State state() const { return m_state; }
    Priority priority() const { return m_priority; }
    void setPriority(Priority p) { m_priority = p; }

    // 任务分类
    virtual bool isPersistent() const { return false; }  // 是否长驻任务

    // 周期任务支持（预留）
    virtual bool isRecurring() const { return false; }
    virtual int intervalMs() const { return 0; }

signals:
    void stateChanged(SchedulerTask::State newState);
    void progress(int percent, const QString &msg);
    void finished(bool success, const QString &msg);
    void dataResult(const QString &key, const QVariantMap &data);

protected:
    void setState(State s) {
        if (m_state != s) {
            m_state = s;
            emit stateChanged(m_state);
        }
    }

private:
    QString m_taskId;    // UUID 自动生成，全局唯一
    State m_state;
    Priority m_priority;
};
```

**设计要点**：

- `m_taskId` 在构造时通过 `QUuid` 自动生成，保证全局唯一
- `setState()` 是 `protected` 的，只允许子类内部调用
- `Q_ENUM` 宏使枚举可在调试输出中自动转为字符串
- `isPersistent()` 默认返回 `false`，子类重写为 `true` 即声明为长驻任务

---

### 3.2 Scheduler — 调度器

**文件**：`scheduler/scheduler.h`、`scheduler/scheduler.cpp`

调度器是整个调度层的**核心引擎**，负责任务的注册、分类、调度和生命周期管理。它区分**长驻任务**和**普通任务**，采用完全不同的管理策略。

#### 3.2.1 设计决策

| 决策 | 选择 | 原因 |
|------|------|------|
| 模式 | 单例 | 全局只需一个调度器 |
| 线程 | 独立 QThread | 不阻塞 UI，不占用 Master 工作线程 |
| 并发 | 最大并发数限制（仅普通任务） | 防止资源耗尽，同时不限制长驻任务 |
| 线程安全 | QMutex | 保护任务容器的并发访问 |
| 任务分类 | `isPersistent()` 虚方法 | 一个判断条件驱动两套管理逻辑 |

#### 3.2.2 内部数据结构 — 双轨管理

```cpp
// 所有任务的注册表（含长驻和普通）
QMap<QString, SchedulerTask*> m_tasks;

// ─── 长驻任务区 ───
QSet<SchedulerTask*> m_persistentTasks;      // 长驻任务集合（不限数量）

// ─── 普通任务区 ───
QQueue<SchedulerTask*> m_pendingQueue;       // 待执行队列（FIFO）
QSet<SchedulerTask*> m_runningTasks;         // 运行中集合

QMutex m_mutex;                              // 保护以上所有容器
int m_maxConcurrent = 10;                    // 最大并发数（仅计普通任务）
```

**双轨管理流程图**：

```
submitTask(task)
       │
       ├── task->isPersistent() == true ?
       │          │
       │          ▼ 是：长驻任务
       │   m_tasks[taskId] = task          ← 注册
       │   m_persistentTasks.insert(task)  ← 放入长驻集合
       │   invokeMethod(task, "start")     ← 立即启动
       │   （不入队，不占并发槽位，永不自动回收）
       │
       └── task->isPersistent() == false ?
                  │
                  ▼ 否：普通任务
           m_tasks[taskId] = task          ← 注册
           m_pendingQueue.enqueue(task)    ← 入队等待
           scheduleNext()                  ← 尝试调度
                  │
                  ▼
           m_pendingQueue.dequeue()        ← 出队
           m_runningTasks.insert(task)     ← 标记运行中
           invokeMethod(task, "start")     ← 启动
                  │
                  ▼ (任务完成后)
           onTaskFinished()
                  │
           m_runningTasks.remove(task)     ← 移除
           m_tasks.remove(taskId)          ← 注销
           task->deleteLater()             ← 延迟释放
           scheduleNext()                  ← 调度下一个
```

#### 3.2.3 submitTask() — 提交任务（核心入口）

```cpp
QString Scheduler::submitTask(SchedulerTask *task)
{
    if (!task) return QString();
    
    QMutexLocker locker(&m_mutex);
    QString taskId = task->taskId();
    
    // 步骤1：将任务移动到调度器线程
    task->moveToThread(&m_thread);
    task->setParent(nullptr);
    
    // 步骤2：连接信号
    connectTaskSignals(task);
    
    // 步骤3：注册任务
    m_tasks[taskId] = task;
    
    if (task->isPersistent()) {
        // ─── 长驻任务：立即启动，不入队，不占并发槽位 ───
        m_persistentTasks.insert(task);
        QMetaObject::invokeMethod(task, "start", Qt::QueuedConnection);
    } else {
        // ─── 普通任务：入队等待调度 ───
        m_pendingQueue.enqueue(task);
    }
    
    locker.unlock();
    
    // 步骤4：尝试调度普通任务
    if (!task->isPersistent()) {
        scheduleNext();
    }
    
    return taskId;
}
```

**为什么要 `moveToThread`？**

`SchedulerTask` 对象在 UI 线程中创建（`new ConnectAllTask()`），但它的 `start()` 方法需要在调度器线程中执行。`moveToThread(&m_thread)` 将对象的线程亲和性（thread affinity）改为调度器线程，这样：

1. 通过 `QMetaObject::invokeMethod(task, "start", Qt::QueuedConnection)` 调用时，`start()` 会在调度器线程的事件循环中执行
2. 任务内部创建的 QTimer 等对象也会运行在调度器线程中
3. 任务的槽函数（如 `onConnectionStateChanged`）会在调度器线程中被调用

**长驻任务为什么立即启动？**

长驻任务不入 `m_pendingQueue`，因为它们不受并发限制。调度器对其的态度是「提交即运行」——它们需要尽快开始监控工作。

#### 3.2.4 scheduleNext() — 普通任务调度引擎

```cpp
void Scheduler::scheduleNext()
{
    QMutexLocker locker(&m_mutex);
    
    while (!m_pendingQueue.isEmpty() && m_runningTasks.size() < m_maxConcurrent) {
        SchedulerTask *task = m_pendingQueue.dequeue();
        m_runningTasks.insert(task);
        
        QMetaObject::invokeMethod(task, "start", Qt::QueuedConnection);
    }
}
```

此方法**只管理普通任务**。`m_runningTasks.size()` 不包含长驻任务，所以长驻任务不会影响普通任务的并发能力。

**为什么用 `Qt::QueuedConnection`？**

`scheduleNext()` 可能从任意线程调用（UI 线程的 `submitTask()`、调度器线程的 `onTaskFinished()`）。`QueuedConnection` 确保 `start()` 始终在调度器线程的事件循环中执行。

#### 3.2.5 onTaskFinished() — 普通任务完成处理

```cpp
void Scheduler::onTaskFinished(bool success, const QString &msg)
{
    SchedulerTask *task = qobject_cast<SchedulerTask*>(sender());
    if (!task) return;
    
    QString taskId = task->taskId();
    emit taskFinished(taskId, success, msg);
    
    // 清理
    QMutexLocker locker(&m_mutex);
    m_runningTasks.remove(task);
    m_tasks.remove(taskId);
    task->deleteLater();
    locker.unlock();
    
    // 释放槽位后，调度下一个
    scheduleNext();
}
```

**为什么用 `deleteLater()` 而不是 `delete`？**

`onTaskFinished()` 由任务自身的 `finished` 信号触发。如果直接 `delete task`，而此时任务对象的其他槽函数还在调用栈中，就会导致**悬空指针崩溃**。`deleteLater()` 推迟到事件循环空闲时删除。

**长驻任务正常运行时不会触发此方法**——它们不 `emit finished()`。只有 `stop()` 被调用时才会发出 `finished(false, ...)`，但此时任务已经被 `cancelTask()` 从 `m_persistentTasks` 中移除了。

#### 3.2.6 cancelTask() — 取消任务（通用）

```cpp
bool Scheduler::cancelTask(const QString &taskId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_tasks.contains(taskId)) return false;
    
    SchedulerTask *task = m_tasks[taskId];
    
    // 根据任务类型从对应容器中移除
    if (m_persistentTasks.contains(task)) {
        task->stop();
        m_persistentTasks.remove(task);
    } else if (m_runningTasks.contains(task)) {
        task->stop();
        m_runningTasks.remove(task);
    }
    
    m_pendingQueue.removeAll(task);
    m_tasks.remove(taskId);
    task->deleteLater();
    
    return true;
}
```

此方法同时支持取消长驻任务和普通任务——通过检查任务在哪个容器中来决定清理逻辑。

#### 3.2.7 stop() — 停止调度器（应用退出时）

```cpp
void Scheduler::stop()
{
    QMutexLocker locker(&m_mutex);
    
    // 先停止所有长驻任务
    for (SchedulerTask *task : m_persistentTasks)
        task->stop();
    m_persistentTasks.clear();
    
    // 再停止所有普通任务
    for (SchedulerTask *task : m_runningTasks)
        task->stop();
    m_runningTasks.clear();
    m_pendingQueue.clear();
    
    // 释放所有任务对象
    qDeleteAll(m_tasks);
    m_tasks.clear();
    
    locker.unlock();
    
    // 停止线程
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}
```

停止顺序：**长驻任务 → 普通任务 → 释放内存 → 停止线程**。确保所有任务都被正确清理。

#### 3.2.8 信号转发机制

```cpp
void Scheduler::connectTaskSignals(SchedulerTask *task)
{
    connect(task, &SchedulerTask::stateChanged,
            this, &Scheduler::onTaskStateChanged);
    
    connect(task, &SchedulerTask::finished,
            this, &Scheduler::onTaskFinished);
    
    connect(task, &SchedulerTask::progress,
            this, [this, task](int percent, const QString &msg) {
        emit taskProgress(task->taskId(), percent, msg);
    });
    
    connect(task, &SchedulerTask::dataResult,
            this, [this](const QString &key, const QVariantMap &data) {
        emit taskDataResult(key, data);
    });
}
```

调度器充当**信号中转站**：

```
Task::progress(50, "已连接 40/80")
  → Scheduler::taskProgress("uuid-xxx", 50, "已连接 40/80")
    → MainWindow lambda → qDebug() / 更新进度条
```

UI 只需连接 Scheduler 的 4 个信号，无需知道具体 Task 对象。新增任务类型时，**Scheduler 代码不需要任何修改**。

#### 3.2.9 调度器内部状态全景图

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Scheduler (调度器线程)                         │
│                                                                       │
│  m_tasks (全局注册表):                                                 │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ "uuid-001" → InitPortDataTask    (普通，已完成并清理)              │ │
│  │ "uuid-002" → ConnectAllTask      (长驻，运行中)                   │ │
│  │ "uuid-003" → MonitorDataTask     (长驻，运行中) [未来]            │ │
│  │ "uuid-004" → FirmwareTask        (普通，运行中) [未来]            │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                       │
│  m_persistentTasks (长驻任务，不限数量):                                │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ [ConnectAllTask]  [MonitorDataTask]  ...                         │ │
│  │ (提交即启动，永不自动回收，不占并发槽位)                            │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                       │
│  m_runningTasks (普通任务，≤ 10):                                      │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ [FirmwareTask] [空闲] [空闲] [空闲] ... [空闲]                    │ │
│  │ (受 m_maxConcurrent 限制，完成后自动回收并调度下一个)               │ │
│  └─────────────────────────────────────────────────────────────────┘ │
│                                                                       │
│  m_pendingQueue (等待中的普通任务):                                     │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │ [SendCommandTask] → [InflateCtrlTask] → ...                      │ │
│  │ (FIFO 排队，等 m_runningTasks 有空位时调度)                        │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

---

### 3.3 ConnectAllTask — 连接监控任务（长驻）

**文件**：`scheduler/tasks/connect_all_task.h`、`scheduler/tasks/connect_all_task.cpp`

这是第一个**长驻任务**实现。它的职责是：
1. 启动所有 80 个 Master 的 TCP 连接（断开重连模式）
2. **持续监控**所有设备的连接状态变化
3. 将任何状态变化**实时上报**给 UI

声明为长驻任务：`bool isPersistent() const override { return true; }`

#### 3.3.1 为什么需要长驻？

设备在运行全过程中会经历**多种状态切换**：

```
正常流程:   Disconnected → Connecting → Connected
网络中断:   Connected → Disconnected → Reconnecting → Connected
连接失败:   Connecting → ConnectFailed → Reconnecting → Connecting → Connected
超时:       Connecting → ConnectTimeout → Reconnecting → ...
```

如果任务在「全部连接成功」后就结束，后续的断线、重连、超时等状态变化将**无人上报**，UI 上的设备状态会「冻结」在最后一次更新的值。

#### 3.3.2 内部状态

```cpp
QHash<QString, Mtm::ConnectionState> m_connectionStates;  // 80 个设备各自的连接状态
int m_totalCount = 0;       // 设备总数
int m_connectedCount = 0;   // 当前已连接数（会随断线/重连动态变化）
bool m_stopped = false;     // 停止标志（stop() 时设为 true，防止后续信号继续处理）
```

#### 3.3.3 start() — 启动流程

```cpp
void ConnectAllTask::start()
{
    setState(Running);
    m_stopped = false;
    m_connectedCount = 0;
    m_connectionStates.clear();
    
    ModbusTcpMasterManager *manager = ModbusTcpMasterManager::instance();
    QStringList ids = manager->masterIds();
    m_totalCount = ids.size();
    
    // 遍历所有 master
    for (const QString &id : ids) {
        ModbusTcpMaster *master = manager->getMaster(id);
        if (!master) continue;
        
        m_connectionStates[id] = Mtm::ConnectionState::Disconnected;
        
        // 监听状态变化（跨线程队列连接）
        connect(master, &ModbusTcpMaster::connectionStateChanged,
                this, &ConnectAllTask::onConnectionStateChanged,
                Qt::QueuedConnection);
        
        // 在 master 线程中启动连接
        QMetaObject::invokeMethod(master, "connectWithReconnect", Qt::QueuedConnection);
    }
    
    emit progress(0, QString("开始连接 %1 个设备").arg(m_totalCount));
    // 注意：start() 执行完后任务仍然是 Running 状态
    // 后续通过 onConnectionStateChanged 信号持续工作
}
```

**关键**：`start()` 返回后任务并没有结束。它注册了 80 个信号连接，每当 master 状态变化时，`onConnectionStateChanged` 就会在调度器线程中被调用。任务靠**事件驱动**持续工作。

#### 3.3.4 onConnectionStateChanged() — 状态变化处理

```cpp
void ConnectAllTask::onConnectionStateChanged(Mtm::ConnectionState state)
{
    if (m_stopped) return;
    
    ModbusTcpMaster *master = qobject_cast<ModbusTcpMaster*>(sender());
    if (!master) return;
    
    QString id = master->id();
    Mtm::ConnectionState oldState = m_connectionStates.value(id, Mtm::ConnectionState::Disconnected);
    
    if (oldState == state) return;  // 防御性过滤：状态未变化则忽略
    
    m_connectionStates[id] = state;
    
    // 更新已连接计数（只关心是否跨越 Connected 边界）
    if (state == Mtm::ConnectionState::Connected && oldState != Mtm::ConnectionState::Connected) {
        m_connectedCount++;
    } else if (state != Mtm::ConnectionState::Connected && oldState == Mtm::ConnectionState::Connected) {
        m_connectedCount--;
    }
    
    // 统一上报该设备的最新状态
    QVariantMap data;
    data["qrcode"] = id;
    data["status"] = Mtm::connectionStateToString(state);
    emit dataResult(id, data);
    
    // 上报整体连接进度
    reportConnectionSummary();
}
```

**统一处理所有状态**——不按状态类型分支，而是：
1. 计数器只关心 Connected 边界（进入 +1，离开 -1）
2. 所有状态统一转字符串上报
3. 每次变化都刷新整体进度

#### 3.3.5 reportConnectionSummary()

```cpp
void ConnectAllTask::reportConnectionSummary()
{
    int percent = (m_totalCount > 0) ? (m_connectedCount * 100 / m_totalCount) : 0;
    emit progress(percent, QString("已连接 %1/%2").arg(m_connectedCount).arg(m_totalCount));
}
```

`percent` 是**双向变化**的——设备断线后百分比会**回落**（如 100% → 98%），这是长驻任务与普通任务的重要区别。

#### 3.3.6 stop() — 停止监控

```cpp
void ConnectAllTask::stop()
{
    m_stopped = true;
    
    // 断开所有信号连接（防止后续状态变化继续触发）
    ModbusTcpMasterManager *manager = ModbusTcpMasterManager::instance();
    QStringList ids = manager->masterIds();
    for (const QString &id : ids) {
        ModbusTcpMaster *master = manager->getMaster(id);
        if (master) {
            disconnect(master, &ModbusTcpMaster::connectionStateChanged,
                       this, &ConnectAllTask::onConnectionStateChanged);
        }
    }
    
    setState(Cancelled);
    emit finished(false, "任务被取消");
}
```

**注意**：`stop()` 只停止状态监控，**不断开 TCP 连接**。master 的断开重连模式仍在后台运行。

---

### 3.4 InitPortDataTask — 初始化端口数据表格任务（普通）

**文件**：`scheduler/tasks/init_port_data_task.h`、`scheduler/tasks/init_port_data_task.cpp`

这是一个典型的**普通任务**——执行完毕后自动 `Finished` 并被调度器回收。它的职责是读取配置并生成 listWidget 的 80 行初始数据。

**与 ConnectAllTask 的对比**：

| 对比项 | InitPortDataTask | ConnectAllTask |
|--------|-----------------|----------------|
| `isPersistent()` | `false`（默认） | `true` |
| 管理容器 | `m_runningTasks` | `m_persistentTasks` |
| 占用并发槽位 | 是（短暂） | 否 |
| 结束方式 | `emit finished()` → 自动回收 | 不结束，需 `cancelTask()` |
| 数据上报 | 批量一次性推送 | 事件驱动持续推送 |

#### 3.4.1 为什么不在 UI 线程直接初始化？

虽然当前初始化逻辑较轻量，但遵循统一架构原则：

1. **一致性**：所有业务逻辑都通过调度器执行，UI 层只负责接收数据并渲染
2. **可扩展性**：未来初始化可能涉及从数据库、网络加载历史数据，此时必须在后台线程
3. **任务依赖**：`InitPortDataTask` 先提交（普通任务入队），`ConnectAllTask` 后提交（长驻任务立即启动）。由于 `InitPortDataTask` 执行极快（毫秒级），它的数据信号会在 `ConnectAllTask` 的第一个状态回调到达 UI 之前被处理完毕，保证 `m_qrcodeToRowMap` 映射已建立

#### 3.4.2 数据协议设计

任务通过 `dataResult(key, data)` 信号传递数据，`key` 固定为 `"init_port_data"`，`data` 中用 `action` 字段区分操作类型：

| action | 含义 | data 字段 |
|--------|------|----------|
| `"clear"` | 清空 listWidget 和映射表 | 无额外字段 |
| `"add_row"` | 添加一行 | `index`, `qrcode`, `status`, `pressure`, `flow`, `humidity` |

#### 3.4.3 完整代码

```cpp
void InitPortDataTask::start()
{
    setState(Running);
    
    AppConfig* config = AppConfig::instance();
    QVector<QString> qrcodes = config->getQRCodeConfigs();
    int deviceCount = qMin(qrcodes.size(), 80);
    
    if (deviceCount == 0) {
        setState(Finished);
        emit finished(true, "没有可初始化的设备数据");
        return;
    }
    
    // 第1步：发送清空指令
    QVariantMap clearData;
    clearData["action"] = "clear";
    emit dataResult("init_port_data", clearData);
    
    // 第2步：逐行发送初始化数据
    for (int i = 0; i < deviceCount; ++i) {
        QVariantMap rowData;
        rowData["action"] = "add_row";
        rowData["index"] = i;
        rowData["qrcode"] = qrcodes[i];
        rowData["status"] = "未连接";
        rowData["pressure"] = 0.0;
        rowData["flow"] = 0.0;
        rowData["humidity"] = 0.0;
        
        emit dataResult("init_port_data", rowData);
        
        int percent = ((i + 1) * 100) / deviceCount;
        emit progress(percent, QString("初始化端口数据 %1/%2").arg(i + 1).arg(deviceCount));
    }
    
    // 第3步：标记完成 → 调度器自动回收此任务
    setState(Finished);
    emit finished(true, QString("端口数据初始化完成，共 %1 行").arg(deviceCount));
}
```

#### 3.4.4 与 ConnectAllTask 的启动时序

```
initScheduler() 执行 (UI 线程):
  │
  ├── submitTask(InitPortDataTask)     普通任务：入 m_pendingQueue
  │     → scheduleNext()              从队列取出 → m_runningTasks
  │     → invokeMethod("start")       投递到调度器线程事件循环
  │
  ├── submitTask(ConnectAllTask)       长驻任务：直接 m_persistentTasks
  │     → invokeMethod("start")       投递到调度器线程事件循环
  │
  └── 函数返回，UI 线程继续

调度器线程事件循环 (按投递顺序执行):
  │
  ├── InitPortDataTask::start()        先执行（先投递的）
  │     ├── emit dataResult × 81       发送 clear + 80 行 add_row
  │     └── emit finished()            → onTaskFinished() → 回收
  │
  └── ConnectAllTask::start()          后执行
        ├── connect 80 个 master 信号
        └── invokeMethod × 80          启动所有设备连接
              │
              │  (master 状态回调到达时)
              ▼
        onConnectionStateChanged()     此时 m_qrcodeToRowMap 已经建好
```

> **为什么不会出现时序问题？** `QMetaObject::invokeMethod(Qt::QueuedConnection)` 将调用投递到目标线程的事件队列末尾。`InitPortDataTask::start()` 和 `ConnectAllTask::start()` 按提交顺序依次投递，事件循环保证先进先执行。

---

### 3.5 MonitorDataTask — 实时数据监控任务（长驻）

**文件**：`scheduler/tasks/monitor_data_task/monitor_data_task.h`、`scheduler/tasks/monitor_data_task/monitor_data_task.cpp`

这是第二个**长驻任务**，职责是监听所有设备的 `ReadStatus` 命令响应，解析出压力、流量、湿度等实时数据并上报给 UI。

#### 3.5.1 工作原理

**关键设计**：MonitorDataTask **不主动发送命令**。每个 `ModbusTcpMaster` 内部的 `MtmTimerSender` 已经在以 1000ms 间隔自动发送 `ReadStatus` 命令（由 XML 配置 `<TimerSender>` 定义）。MonitorDataTask 只需要**被动监听** `commandCompleted` 信号即可。

```
MtmTimerSender (master 内部)       MonitorDataTask (调度器线程)        UI 线程
─────────────────────────         ────────────────────────          ────────

每 1000ms 自动发送 ReadStatus
        │
        ▼
master.commandCompleted(cmd, Success, "")
        │
        │  Qt::QueuedConnection
        ▼
onCommandCompleted()
  ├─ 过滤: cmd->name() == "ReadStatus" && Success
  ├─ parseReadStatusResponse(cmd)
  │     → 解析 28 字节响应帧
  │     → 提取 humidity/temperature/pressure/flow 等
  │
  └─ emit dataResult("monitor_data", {
       qrcode: "12034",
       humidity: 45.23,
       inletPressure: 101.5,
       outFlow: 12.8,
       ...
     })
        │  Qt::AutoConnection (跨线程)
        ▼
  MainWindow lambda
    → update80PortData("12034", 101.5, 12.8, 45.23)
    → listWidget 更新: "12034  状态:Connected  压力:101.5  流量:12.8  湿度:45.23"
```

#### 3.5.2 与 ConnectAllTask 的对比

| 对比项 | ConnectAllTask | MonitorDataTask |
|--------|---------------|-----------------|
| 监听信号 | `connectionStateChanged` | `commandCompleted` |
| 触发频率 | 事件驱动（连接/断开时） | 周期性（每 1000ms 一次 × 80 设备） |
| 数据量 | 轻量（仅状态字符串） | 较大（28 字节帧，解析出 20+ 字段） |
| UI 更新方法 | `updateDeviceStatus()` | `update80PortData()` |
| dataResult key | 设备 QRCode | `"monitor_data"` |

#### 3.5.3 ReadStatus 响应帧解析

Command '3' (ReadStatus) 响应帧结构（28 字节）：

| 偏移 | 字段 | 字节数 | 类型 | 说明 |
|------|------|--------|------|------|
| 0 | start | 1 | u8 | 帧头 |
| 1 | len | 1 | u8 | 长度 |
| 2 | id | 2 | u16 | 设备ID |
| 4 | cmd | 1 | u8 | 命令码 |
| 5 | deviceStatus | 1 | bitmap | b0:Manual b1:Alarm b2:AutoPurge b3:IdlePurge b4:ContinuousPurge |
| 6 | alarmCode | 1 | bitmap | b0:温湿度传感器 b1:MFC b2:VMFC b3:Reserved b4:Screen |
| 7 | foupFlag | 1 | u8 | 0x01=有FOUP, 0x00=无FOUP |
| 8 | foupStatus | 1 | bitmap | b0-b2:Sensor b3:FOUP_Type b4-b7:INFOpad |
| 9 | valveStatus | 1 | bitmap | b0-b3: Valve1~Valve4 开关状态 |
| 10 | humidity | 2 | u16/100 | 湿度 (%) |
| 12 | temperature | 2 | u16/100 | 温度 (°C) |
| 14 | inletPressure | 2 | u16/100 | 入口压力 (KPa) |
| 16 | outletPressure | 2 | u16/100 | 出口压力 (KPa) |
| 18 | outFlow | 2 | u16/100 | 出口流量 (L/min) |
| 20 | inFlow | 2 | u16/100 | 入口流量 (L/min) |
| 22 | outSetFlow | 2 | u16/100 | 出口设定流量 (L/min) |
| 24 | inSetFlow | 2 | u16/100 | 入口设定流量 (L/min) |
| 26 | crc | 2 | - | CRC校验 |

`parseReadStatusResponse()` 使用 `cmd->responseFieldValue(fieldName)` 获取各字段的原始字节，通过辅助函数解析为具体数值：

```cpp
// 1 字节无符号整数
auto getU8 = [&cmd](const QString &field) -> quint8 {
    QByteArray val = cmd->responseFieldValue(field);
    return val.isEmpty() ? 0 : static_cast<quint8>(val.at(0));
};

// 2 字节大端序数值，除以 100.0
auto getU16AsDouble = [&cmd](const QString &field) -> double {
    QByteArray val = cmd->responseFieldValue(field);
    if (val.size() < 2) return 0.0;
    quint16 raw = (static_cast<quint8>(val.at(0)) << 8) | static_cast<quint8>(val.at(1));
    return raw / 100.0;
};
```

#### 3.5.4 dataResult 数据字段

MonitorDataTask 的 `dataResult` 信号 key 为 `"monitor_data"`，data 包含：

| 字段 | 类型 | 说明 | UI 使用 |
|------|------|------|---------|
| `qrcode` | QString | 设备 QRCode | 定位 listWidget 行号 |
| `humidity` | double | 湿度 (%) | → `update80PortData` 的 `h` 参数 |
| `inletPressure` | double | 入口压力 (KPa) | → `update80PortData` 的 `p` 参数 |
| `outFlow` | double | 出口流量 (L/min) | → `update80PortData` 的 `f` 参数 |
| `temperature` | double | 温度 (°C) | 可扩展显示 |
| `outletPressure` | double | 出口压力 (KPa) | 可扩展显示 |
| `inFlow` | double | 入口流量 (L/min) | 可扩展显示 |
| `isManualMode` | bool | 手动模式标志 | 可扩展显示 |
| `hasDeviceAlarm` | bool | 设备告警标志 | 可扩展告警提示 |
| ... | ... | 更多状态/告警字段 | 可扩展 |

> **设计优势**：虽然 UI 当前只使用 `inletPressure`、`outFlow`、`humidity` 三个字段，但 MonitorDataTask 解析并上报了所有 20+ 个字段。未来扩展 UI 显示更多数据时，**无需修改任务代码**，只需在 MainWindow 的 lambda 中读取更多字段即可。

---

## 四、UI 层集成

### 4.1 initScheduler() — 完整代码

```cpp
void MainWindow::initScheduler()
{
    Scheduler* scheduler = Scheduler::instance();
    scheduler->start();
    
    // ─── 1. 数据结果分发 ───
    connect(scheduler, &Scheduler::taskDataResult,
            this, [this](const QString &key, const QVariantMap &data) {
        
        // InitPortDataTask 的数据
        if (key == "init_port_data") {
            QString action = data.value("action").toString();
            if (action == "clear") {
                ui->listWidget->clear();
                m_qrcodeToRowMap.clear();
            } else if (action == "add_row") {
                int index = data.value("index").toInt();
                QString qrcode = data.value("qrcode").toString();
                QString status = data.value("status").toString();
                double p = data.value("pressure").toDouble();
                double f = data.value("flow").toDouble();
                double h = data.value("humidity").toDouble();
                
                QString rowText = QString("%1\t状态:%2\t压力:%3\t流量:%4\t湿度:%5")
                                 .arg(qrcode).arg(status)
                                 .arg(p, 0, 'f', 1).arg(f, 0, 'f', 1).arg(h, 0, 'f', 1);
                
                QListWidgetItem* item = new QListWidgetItem(rowText);
                ui->listWidget->addItem(item);
                m_qrcodeToRowMap[qrcode] = index;
            }
            return;
        }
        
        // ConnectAllTask 的数据
        QString qrcode = data.value("qrcode").toString();
        QString status = data.value("status").toString();
        if (!qrcode.isEmpty() && !status.isEmpty()) {
            updateDeviceStatus(qrcode, status);
        }
        
        // 未来：MonitorDataTask 的数据
        // if (key == "monitor_data") { ... }
    });
    
    // ─── 2. 进度日志 ───
    connect(scheduler, &Scheduler::taskProgress,
            this, [](const QString &taskId, int percent, const QString &msg) {
        qDebug() << "[MainWindow] 任务进度:" << taskId << percent << "%" << msg;
    });
    
    // ─── 3. 完成日志 ───
    connect(scheduler, &Scheduler::taskFinished,
            this, [](const QString &taskId, bool success, const QString &msg) {
        qDebug() << "[MainWindow] 任务完成:" << taskId << "成功:" << success << msg;
    });
    
    // ─── 4. 提交任务 ───
    InitPortDataTask *initPortTask = new InitPortDataTask();  // 普通任务
    scheduler->submitTask(initPortTask);
    
    ConnectAllTask *connectTask = new ConnectAllTask();        // 长驻任务
    scheduler->submitTask(connectTask);
}
```

### 4.2 数据流完整路径

#### 路径 A：端口数据初始化（普通任务，一次性完成）

```
调度器线程                                        UI 线程
──────────                                        ────────
InitPortDataTask::start()
  │
  ├─ emit dataResult("init_port_data",
  │   {action:"clear"})
  │     │  Qt::QueuedConnection
  │     └→ MainWindow: listWidget->clear()
  │                    m_qrcodeToRowMap.clear()
  │
  ├─ emit dataResult("init_port_data",
  │   {action:"add_row", index:0,
  │    qrcode:"12034", status:"未连接", ...})
  │     │  Qt::QueuedConnection
  │     └→ MainWindow: listWidget->addItem(...)
  │                    m_qrcodeToRowMap["12034"] = 0
  │
  ├─ ... (重复80次)
  │
  └─ emit finished(true, "完成")
       │
       └→ Scheduler::onTaskFinished()
            → m_runningTasks.remove()
            → task->deleteLater()     ← 任务被回收
            → scheduleNext()          ← 释放并发槽位
```

#### 路径 B：设备连接状态变化（长驻任务，持续运行）

```
Master 工作线程               调度器线程                        UI 线程
─────────────               ──────────                        ────────

TCP 连接成功 ─→ master.connectionStateChanged(Connected)
                  │
                  │ Qt::QueuedConnection
                  ▼
              ConnectAllTask::onConnectionStateChanged()
                  │
                  ├─ m_connectedCount++
                  │
                  ├─ emit dataResult("12034",
                  │   {"qrcode":"12034", "status":"Connected"})
                  │     │ Qt::AutoConnection (跨线程自动队列)
                  │     └→ MainWindow lambda
                  │          → updateDeviceStatus("12034", "Connected")
                  │          → listWidget 第 0 行: "12034 状态:Connected ..."
                  │
                  └─ emit progress(50, "已连接 40/80")
                        │
                        └→ MainWindow lambda → qDebug()

─── 5 分钟后，设备断线 ───

TCP 断开 ─→ master.connectionStateChanged(Disconnected)
                  │
                  ▼
              ConnectAllTask::onConnectionStateChanged()
                  │
                  ├─ m_connectedCount--              ← 计数回落
                  │
                  ├─ emit dataResult("12034",
                  │   {"qrcode":"12034", "status":"Disconnected"})
                  │     └→ UI 更新为 "状态:Disconnected"
                  │
                  └─ emit progress(49, "已连接 39/80")  ← 百分比回落
```

---

## 五、文件结构

```
scheduler/
├── scheduler_task.h              # 任务基类（纯头文件，定义 isPersistent() 等接口）
├── scheduler.h                   # 调度器头文件（双轨管理：m_persistentTasks + m_runningTasks）
├── scheduler.cpp                 # 调度器实现
├── scheduler.pri                 # qmake 模块文件
├── README.md                     # 本文档
└── tasks/
    ├── connect_all_task.h        # 连接监控任务（长驻）— 头文件
    ├── connect_all_task.cpp      # 连接监控任务（长驻）— 实现
    ├── init_port_data_task.h     # 初始化端口数据任务（普通）— 头文件
    ├── init_port_data_task.cpp   # 初始化端口数据任务（普通）— 实现
    └── monitor_data_task/        # 实时数据监控任务（长驻）
        ├── monitor_data_task.h   # 头文件
        └── monitor_data_task.cpp # 实现
```

---

## 六、如何扩展新任务

### 6.1 新增普通任务（3 步）

**第 1 步**：创建任务类

```cpp
// scheduler/tasks/firmware_upgrade_task.h
#include "../scheduler_task.h"

class FirmwareUpgradeTask : public SchedulerTask
{
    Q_OBJECT
public:
    explicit FirmwareUpgradeTask(const QString &masterId, QObject *parent = nullptr);
    
    void start() override;
    void stop() override;
    QString taskType() const override { return "FirmwareUpgradeTask"; }
    // isPersistent() 默认返回 false，无需重写
};
```

**第 2 步**：更新 `scheduler.pri`

```
HEADERS += $$PWD/tasks/firmware_upgrade_task.h
SOURCES += $$PWD/tasks/firmware_upgrade_task.cpp
```

**第 3 步**：提交任务

```cpp
auto *task = new FirmwareUpgradeTask("12034");
Scheduler::instance()->submitTask(task);
```

### 6.2 新增长驻任务（额外 1 步）

在第 1 步的基础上，只需多重写一个方法：

```cpp
class MonitorDataTask : public SchedulerTask
{
    Q_OBJECT
public:
    // ...
    bool isPersistent() const override { return true; }  // ← 唯一区别
};
```

调度器会自动：
- 将其放入 `m_persistentTasks`（而非 `m_runningTasks`）
- 立即启动（而非入队等待）
- 不占用并发槽位
- 不自动回收

### 6.3 扩展 UI 数据分发

在 `MainWindow::initScheduler()` 的 `taskDataResult` lambda 中添加新的 `key` 分支：

```cpp
// 已有
if (key == "init_port_data") { ... }

// 新增
if (key == "monitor_data") {
    QString qrcode = data.value("qrcode").toString();
    double pressure = data.value("pressure").toDouble();
    // ... 更新 UI
    return;
}
```

**不需要修改 Scheduler 或 SchedulerTask 的任何代码。**

---

## 七、线程安全总结

| 操作 | 线程安全机制 | 涉及线程 |
|------|------------|---------|
| `submitTask()` / `cancelTask()` | `QMutex` + `QMutexLocker` | UI → 调度器 |
| 启动任务 `start()` | `QMetaObject::invokeMethod(Qt::QueuedConnection)` | 调度器内部 |
| 调用 Master 方法 | `QMetaObject::invokeMethod(Qt::QueuedConnection)` | 调度器 → Master 工作线程 |
| Master 状态回调 | `connect(..., Qt::QueuedConnection)` | Master 工作线程 → 调度器 |
| 数据传递到 UI | `Qt::AutoConnection`（跨线程自动队列化） | 调度器 → UI |
| 任务对象删除 | `deleteLater()` | 调度器线程事件循环 |
| 长驻任务停止标志 | `m_stopped = true` + 信号断开 | 调度器线程内部 |

**关键原则**：

1. **永远不在 UI 线程中直接操作 Master 对象** — 通过 `QMetaObject::invokeMethod` 代理
2. **永远不在非 UI 线程中直接操作 QWidget** — 通过信号/槽跨线程传递数据
3. **使用 `deleteLater()` 而非 `delete`** — 防止在信号处理过程中销毁对象

---

## 八、应用退出与资源清理

### 8.1 问题现象

关闭 MainWindow 后，**应用进程不退出，后台线程持续运行**。在任务管理器中可以看到进程仍然存在，CPU 占用不为零。

### 8.2 根因分析

应用中存在**三层后台线程**，它们不会因为窗口关闭而自动停止：

| 线程 | 持有者 | 存活原因 |
|------|--------|---------|
| SchedulerThread | `Scheduler` 单例的 `m_thread` | 事件循环中有长驻任务（ConnectAllTask、MonitorDataTask）在持续处理信号 |
| Master 工作线程 1~N | `ModbusTcpMasterManager` 线程池 | 每个线程运行事件循环，处理 TCP 连接和 Modbus 指令收发 |
| TimerSender 定时器 | 每个 `ModbusTcpMaster` 内部 | 每 1000ms 发送 ReadStatus，定时器阻止线程事件循环退出 |

Qt 的 `QThread` 事件循环在 `quit()` 被调用前不会自行退出。`Scheduler` 和 `ModbusTcpMasterManager` 都是堆上的单例对象，不会在 `main()` 返回时被自动析构。

### 8.3 清理顺序要求

资源清理必须严格按照**自顶向下**的依赖顺序执行：

```
步骤1: Scheduler::stop()
  ├── 停止长驻任务 (ConnectAllTask, MonitorDataTask)
  │     └── task->stop() 中断开与 Master 的信号连接
  │         ⚠️ 需要访问 ModbusTcpMasterManager::instance()
  ├── 停止普通任务
  ├── 释放所有任务对象
  └── m_thread.quit() + wait()  → 调度器线程退出

步骤2: delete ModbusTcpMasterManager
  ├── 在各工作线程中 disconnect() 所有 Master
  ├── master->deleteLater() 
  ├── 所有工作线程 quit() + wait(3000)
  └── 超时则 terminate() 强制终止
```

**⚠️ 顺序不能颠倒**：如果先删除 `ModbusTcpMasterManager`，`ConnectAllTask::stop()` 和 `MonitorDataTask::stop()` 中调用 `ModbusTcpMasterManager::instance()` 会访问已删除的单例（或创建一个新的空实例），导致未定义行为。

### 8.4 解决方案

利用 `QCoreApplication::aboutToQuit` 信号的 **FIFO 触发顺序**（按 `connect()` 调用先后排列）：

```cpp
// MainWindow::initScheduler() 中 —— 先连接
connect(qApp, &QCoreApplication::aboutToQuit, this, []() {
    Scheduler::instance()->stop();  // 步骤1：先停调度器
});

// ModbusTcpMasterManager::instance() 中 —— 后连接
QObject::connect(qApp, &QCoreApplication::aboutToQuit, []() {
    delete s_instance;              // 步骤2：再清理 Manager
    s_instance = nullptr;
});
```

由于 `initScheduler()` 在 `MainWindow` 构造函数中执行，而 `ModbusTcpMasterManager::instance()` 在 `InitPortDataTask`（调度器线程）中首次调用，**Scheduler 的清理连接一定先于 Manager 的清理连接**，保证了正确的销毁顺序。

### 8.5 退出时序图

```
用户点击关闭按钮
       │
       ▼
QApplication 检测到最后一个窗口关闭
       │
       ▼
aboutToQuit 信号触发（FIFO 顺序）
       │
       ├─ [连接1] Scheduler::stop()
       │    ├── ConnectAllTask::stop()      ← 安全访问 Manager（仍存活）
       │    ├── MonitorDataTask::stop()     ← 安全访问 Manager（仍存活）
       │    ├── qDeleteAll(m_tasks)
       │    └── m_thread.quit() + wait()    ← 调度器线程退出
       │
       └─ [连接2] delete ModbusTcpMasterManager
            ├── disconnect() 所有 Master    ← BlockingQueuedConnection
            ├── master->deleteLater()
            ├── 工作线程 quit() + wait(3000)
            └── delete 线程对象
       │
       ▼
QApplication::exec() 返回
       │
       ▼
~MainWindow()  →  delete ui
       │
       ▼
进程正常退出
```

---

## 九、后续扩展方向

| 方向 | 说明 | 复杂度 |
|------|------|--------|
| ~~实时数据监控任务~~ | ✅ 已实现 `MonitorDataTask`（长驻），监听 ReadStatus 响应解析实时数据 | — |
| **优先级队列** | 将 `m_pendingQueue` 从 `QQueue` 改为优先级队列，按 `task->priority()` 排序 | 低 |
| **设备锁** | 添加 `QHash<QString, QMutex*> m_deviceLocks`，防止多个任务同时操作同一设备 | 中 |
| **周期任务** | 在 `onTaskFinished()` 中检查 `isRecurring()`，如果是周期任务则延迟后重新入队 | 低 |
| **任务依赖** | 支持任务 A 完成后自动触发任务 B（DAG 依赖图） | 高 |
| **任务分组** | 支持按 `taskType()` 批量取消同一类型的所有任务 | 低 |
| **运行状态查询** | 提供 `QStringList persistentTaskIds()` / `int runningTaskCount()` 等查询接口 | 低 |
