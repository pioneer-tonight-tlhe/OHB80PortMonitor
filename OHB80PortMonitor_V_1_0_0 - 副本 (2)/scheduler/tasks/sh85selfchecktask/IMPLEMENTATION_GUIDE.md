# 实现85自检调度任务

## 第一步：基础构建

### 1. 继承 SchedulerTask 基类

```cpp
#include "scheduler_task.h"

class SH85PeriodicSelfCheckTask2 : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SH85PeriodicSelfCheckTask2(QObject *parent = nullptr);
    ~SH85PeriodicSelfCheckTask2() override;
};
```

### 2. 必须实现的接口

- `QString taskType() const override` - 返回任务类型标识
- `void start() override` - 启动任务
- `void stop() override` - 停止任务

#### 状态更新：使用 SchedulerTask::setState()

在任务生命周期中，使用调度基类提供的 `setState(SchedulerTask::State)` 更新任务状态，以便外部能够通过 `stateChanged` 信号感知状态变化。常见用法：

```cpp
void start() override {
    // 进入运行态，便于调度器/外部订阅者感知
    setState(SchedulerTask::Running);
    // ... 执行启动逻辑 ...
}

void stop() override {
    // 根据结束语义选择 Finished / Cancelled / Failed
    setState(SchedulerTask::Finished);
    // 可按需发出 finished(success, msg)
    emit finished(true, QStringLiteral("task stopped"));
}
```

#### UI 友好状态查询：QString currentState()

在基础构建阶段，建议提供一个便捷的字符串状态查询接口，基于调度基类 `state()` 转换为可展示的文本，供 UI 直接使用。

接口示例：

```cpp
// 返回 UI 友好的任务状态文案（由 SchedulerTask::state() 映射而来）
QString currentState() const;
```

实现要点：
- 状态来源：`SchedulerTask::state()`（Pending/Running/Paused/Finished/Failed/Cancelled）
- 文案形式：按项目约定输出中文或英文（例如："待开始"、"运行中"、"已完成" 等）
- 放置位置：与 `start()/stop()` 同属于“基础构建”阶段接口

### 3. 常驻任务标记

重写 `isPersistent()` 返回 `true`：

```cpp
bool isPersistent() const override { return true; }
```

### 4. 构建配置

在 `scheduler.pri` 中添加文件：

```pri
# HEADERS
$$PWD/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.h

# SOURCES
$$PWD/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.cpp
```

## 第二步：添加配置接口

添加配置控制接口，用于设置任务属性：

```cpp
// 配置控制接口
void setEnabled(bool enabled);
bool isEnabled() const { return m_enabled; }

void setPeriod(int value, const QString& unit);
int periodSeconds() const { return m_periodSec; }

// 单设备自检配置：传入指定设备 QR 码；传入空串表示关闭单设备模式
void setSingleDevice(const QString& qrcode);

// 配置成员
private:
    bool m_enabled = true;
    int  m_periodSec = 300;
    // 单设备模式配置
    bool m_singleDeviceMode = false;   // 是否仅针对单一设备执行
    QString m_singleDeviceQrcode;      // 单设备模式下的目标设备 QR 码
```

注意：这些方法只是设置简单成员变量，不涉及线程亲和性问题，因此不需要 Q_INVOKABLE 宏。

## 第三步：筛选设备

添加筛选方法，过滤出可进行85自检的设备：

```cpp
// 筛选可进行85自检的设备
QStringList filterAvailableDevices();
```

筛选条件：
- 非disable设备（enable = true）
- 非foup in设备（foupIn = false）
- 非未连接设备（isConnected = true）

筛选结果存储在成员变量中，方便后续使用：

```cpp
QStringList m_availableDevices;  // 可进行85自检的设备列表
```

单设备模式：当仅需对某一设备执行自检时，不需要在筛选前做额外分支，直接让待筛选列表只包含该设备，之后复用统一筛选逻辑：

```cpp
// 获取待筛选设备列表：单设备模式仅包含目标设备，否则为全量
QStringList allQrcodes = m_singleDeviceMode
        ? QStringList{ m_singleDeviceQrcode }
        : SharedData::getAllFoupQRCodes();
```

## 第四步：启动延时

使用 BootDelayTimer 实现任务开始前的阻塞等待功能（其他任务不会被阻塞）：

```cpp
private slots:
    void onBootDelayTimeout();

signals:
    void bootDelayCountdown(int remainingSeconds);  // 启动延时倒计时信号，供外部订阅

private:
    BootDelayTimer* m_bootDelay = nullptr;
```

在构造函数中初始化 BootDelayTimer 并连接信号：

```cpp
m_bootDelay = new BootDelayTimer(this);
connect(m_bootDelay, &BootDelayTimer::timeout, this, &SH85PeriodicSelfCheckTask2::onBootDelayTimeout);
connect(m_bootDelay, &BootDelayTimer::countdown, this, &SH85PeriodicSelfCheckTask2::bootDelayCountdown);
```

在 start() 方法中启动延时：

```cpp
void SH85PeriodicSelfCheckTask2::start()
{
    m_bootDelay->startSeconds(30);
}
```

在超时槽函数中执行任务逻辑：

```cpp
void SH85PeriodicSelfCheckTask2::onBootDelayTimeout()
{
    filterAvailableDevices();
}
```

## 完整示例

## 第五步：核心业务功能
### 1.启动所有可用设备并连接信号

在启动延时结束后，遍历已筛选的设备，获取各自的 `SH85SelfChecker`，连接需要转发的信号并启动自检：

```cpp
void SH85PeriodicSelfCheckTask2::startAvailableDeviceChecks()
{
    // 遍历所有可用设备，连接自检信号并启动自检
    // 1. 获取设备 master 并校验连接状态
    // 2. 获取 SH85SelfChecker 并连接三个关键信号
    // 3. 启动自检
    auto& mgr = ModbusTcpMasterManager::instance();
    for (const QString& qrcode : qAsConst(m_availableDevices)) {
        ModbusTcpMaster* master = mgr.getMaster(qrcode);
        if (!master || !master->isConnected()) {
            continue;  // 跳过未连接设备
        }

        SH85SelfChecker* checker = master->selfChecker();
        if (!checker) {
            continue;  // 跳过无自检器设备
        }

        // 转发倒计时信号
        connect(checker, &SH85SelfChecker::countdownTick,
                this, &SH85PeriodicSelfCheckTask2::countdownTick, Qt::QueuedConnection);
        // 转发状态变更信号
        connect(checker, &SH85SelfChecker::stateChanged,
                this, &SH85PeriodicSelfCheckTask2::selfCheckerStateChanged, Qt::QueuedConnection);
        // 转发完成信号
        connect(checker, &SH85SelfChecker::finished,
                this, [this](bool success, SH85SelfChecker::Result /*result*/, const QString& message, const QString& masterId){
                    emit oneFinished(masterId, success, message);
                }, Qt::QueuedConnection);
        // 转发指令完成信号（用于通讯日志）
        connect(checker, &SH85SelfChecker::commandCompleted,
                this, &SH85PeriodicSelfCheckTask2::commandCompleted, Qt::QueuedConnection);
        // 转发指令重试信号
        connect(checker, &SH85SelfChecker::commandRetrying,
                this, &SH85PeriodicSelfCheckTask2::commandRetrying, Qt::QueuedConnection);
        // 转发错误信号
        connect(checker, &SH85SelfChecker::errorOccurred,
                this, &SH85PeriodicSelfCheckTask2::errorOccurred, Qt::QueuedConnection);

        if (!checker->start()) {
            continue;  // 跳过启动失败设备
        }
    }
}
```

## 头文件

```cpp
#ifndef SH85_PERIODIC_SELF_CHECK_TASK2_H
#define SH85_PERIODIC_SELF_CHECK_TASK2_H

#include "scheduler_task.h"
#include "boot_delay_timer.h"

class SH85PeriodicSelfCheckTask2 : public SchedulerTask
{
    Q_OBJECT
signals:
    // ---- 转发单设备自检信号 ----
    void countdownTick(int remainingSeconds, const QString& masterId);
    void selfCheckerStateChanged(SH85SelfChecker::State state, const QString& masterId);
    void oneFinished(const QString& masterId, bool success, const QString& description);

    // ---- 转发指令完成信号（用于写入通讯日志） ----
    void commandCompleted(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);
    void errorOccurred(SH85SelfChecker::Result result, const QString& message, const QString& masterId);


private:
    // ---- 筛选出的设备列表 ----
    QStringList m_availableDevices;  // 可进行85自检的设备列表
};

#endif // SH85_PERIODIC_SELF_CHECK_TASK2_H
```

## 实现文件

```cpp
void SH85PeriodicSelfCheckTask2::onBootDelayTimeout()
{
    // 启动延时结束，筛选可用设备
    filterAvailableDevices();
    qDebug() << "[Scheduler][SH85PeriodicSelfCheckTask2] 启动延时结束，开始执行任务";
    
    // 启动所有可用设备的自检
    startAvailableDeviceChecks();  
}

void SH85PeriodicSelfCheckTask2::startAvailableDeviceChecks()
{
    // 遍历所有可用设备，连接自检信号并启动自检
    // 1. 获取设备 master 并校验连接状态
    // 2. 获取 SH85SelfChecker 并连接三个关键信号
    // 3. 启动自检
    auto& mgr = ModbusTcpMasterManager::instance();
    for (const QString& qrcode : qAsConst(m_availableDevices)) {
        ModbusTcpMaster* master = mgr.getMaster(qrcode);
        if (!master || !master->isConnected()) {
            continue;  // 跳过未连接设备
        }

        SH85SelfChecker* checker = master->selfChecker();
        if (!checker) {
            continue;  // 跳过无自检器设备
        }

        // 转发倒计时信号
        connect(checker, &SH85SelfChecker::countdownTick,
                this, &SH85PeriodicSelfCheckTask2::countdownTick, Qt::QueuedConnection);
        // 转发状态变更信号
        connect(checker, &SH85SelfChecker::stateChanged,
                this, &SH85PeriodicSelfCheckTask2::selfCheckerStateChanged, Qt::QueuedConnection);
        // 转发完成信号
        connect(checker, &SH85SelfChecker::finished,
                this, [this](bool success, SH85SelfChecker::Result /*result*/, const QString& message, const QString& masterId){
                    emit oneFinished(masterId, success, message);
                }, Qt::QueuedConnection);
        // 转发指令完成信号（用于通讯日志）
        connect(checker, &SH85SelfChecker::commandCompleted,
                this, &SH85PeriodicSelfCheckTask2::commandCompleted, Qt::QueuedConnection);
        // 转发指令重试信号
        connect(checker, &SH85SelfChecker::commandRetrying,
                this, &SH85PeriodicSelfCheckTask2::commandRetrying, Qt::QueuedConnection);
        // 转发错误信号
        connect(checker, &SH85SelfChecker::errorOccurred,
                this, &SH85PeriodicSelfCheckTask2::errorOccurred, Qt::QueuedConnection);

        if (!checker->start()) {
            continue;  // 跳过启动失败设备
        }
    }
}
```

