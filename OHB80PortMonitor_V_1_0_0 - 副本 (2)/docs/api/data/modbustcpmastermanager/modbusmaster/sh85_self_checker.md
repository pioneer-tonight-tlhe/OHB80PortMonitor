# SH85SelfChecker API 文档

## 模块概述

`SH85SelfChecker` 是 SH85 湿度传感器自检器，作为 `ModbusTcpMaster` 的子控件存在于 data 层。它通过状态机驱动完整的自检流程，无需依赖 SchedulerTask，可被任意线程调用。

每个 `ModbusTcpMaster` 实例自带一个 `SH85SelfChecker`，与 `connector` / `sender` / `periodicSender` 等子控件保持一致的获取方式（`master->selfChecker()`），支持多设备并行自检。

## 类定义

```cpp
class SH85SelfChecker : public QObject
{
    Q_OBJECT

public:
    enum class State;
    enum class Result;

    explicit SH85SelfChecker(ModbusTcpMaster* master, QObject* parent = nullptr);
    ~SH85SelfChecker() override;

    bool start();
    void stop();
    bool isRunning() const;
    State currentState() const;

    static QString stateToString(State s);
    static QString resultToString(Result r);

signals:
    void started(const QString& masterId);
    void stateChanged(SH85SelfChecker::State state, const QString& masterId);
    void errorOccurred(SH85SelfChecker::Result result,
                       const QString& message,
                       const QString& masterId);
    void finished(bool success,
                  SH85SelfChecker::Result result,
                  const QString& message,
                  const QString& masterId);

private:
    // ... 私有实现
};
```

## 枚举类型

### State - 状态机状态

| 枚举值 | 说明 |
|--------|------|
| `Idle` | 空闲，未启动 |
| `StartingSelfCheck` | 已下发 StartSelfCheck，等待响应 |
| `WaitingPhase1` | StartSelfCheck 成功，等待 5s 计时 |
| `ReadingStatusEarly` | 已下发阶段 1 的 ReadSelfCheckStatus，等待响应 |
| `WaitingPhase2` | 阶段 1 通过，等待 55s 计时 |
| `PollingStatus` | 进入 10s 轮询窗口 |
| `Done` | 流程结束（成功/失败/取消） |

### Result - 自检结果

| 枚举值 | 说明 | 对应 CH_1 值 |
|--------|------|--------------|
| `Success` | 自检成功 | 2 |
| `StartCommandFailed` | StartSelfCheck 指令下发失败 | - |
| `ReadEarlyCommandFailed` | 阶段 1 ReadSelfCheckStatus 下发失败 | - |
| `DeviceNotEntered` | 阶段 1 CH_1 == 0，设备未进入自检 | 0 |
| `FirmwareAbnormal` | 底层固件异常（CH_1 非预期值） | 其他 |
| `ReadPollCommandFailed` | 阶段 2 轮询 ReadSelfCheckStatus 下发失败 | - |
| `HumidityExceeded` | CH_1 == 3，湿度超标失败 | 3 |
| `SensorCommError` | CH_1 == 4，SH85 传感器通讯故障 | 4 |
| `ThresholdParamError` | CH_1 == 5，阈值参数错误（湿度下限阈值 <= 0） | 5 |
| `Timeout` | 10s 轮询窗口仍未拿到终态值 | - |
| `Cancelled` | 外部调用 stop() 主动取消 | - |

## 公共 API

### 构造函数

```cpp
explicit SH85SelfChecker(ModbusTcpMaster* master, QObject* parent = nullptr);
```

**参数说明**
- `master`：所属的 ModbusTcpMaster（必须非空，作为 parent）
- `parent`：父对象（默认为 master）

**异常说明**
- 断言失败：若 `master` 为 nullptr

### start()

```cpp
bool start();
```

**功能说明**：启动自检流程

**返回值说明**
- `true`：启动成功
- `false`：启动失败（前置条件不满足）

**前置条件**
- master 已连接
- 指令池中存在 StartSelfCheck / ReadSelfCheckStatus 模板
- 当前状态为 Idle / Done

**异常说明**
- 无异常，前置条件不满足时直接返回 false 且不发任何信号

### stop()

```cpp
void stop();
```

**功能说明**：主动取消自检流程

**参数说明**：无

**返回值说明**：无

**异常说明**
- 无异常，取消后将发出 finished 信号（result = Cancelled），不发 errorOccurred

### isRunning()

```cpp
bool isRunning() const;
```

**功能说明**：当前是否正在自检

**参数说明**：无

**返回值说明**
- `true`：正在自检（状态非 Idle / Done）
- `false`：未在自检

### currentState()

```cpp
State currentState() const;
```

**功能说明**：获取当前状态

**参数说明**：无

**返回值说明**：当前状态机状态

### stateToString()

```cpp
static QString stateToString(State s);
```

**功能说明**：状态枚举转中文字符串（用于日志）

**参数说明**
- `s`：状态枚举值

**返回值说明**：状态的中文字符串描述

### resultToString()

```cpp
static QString resultToString(Result r);
```

**功能说明**：结果枚举转字符串（用于日志和信号文案）

**参数说明**
- `r`：结果枚举值

**返回值说明**：结果的字符串描述

## 信号

### started

```cpp
void started(const QString& masterId);
```

**触发时机**：自检流程已启动

**参数说明**
- `masterId`：Master 设备 ID

### stateChanged

```cpp
void stateChanged(SH85SelfChecker::State state, const QString& masterId);
```

**触发时机**：内部状态变更

**参数说明**
- `state`：新状态
- `masterId`：Master 设备 ID

### errorOccurred

```cpp
void errorOccurred(SH85SelfChecker::Result result,
                   const QString& message,
                   const QString& masterId);
```

**触发时机**：任意阶段失败均会发出

**参数说明**
- `result`：失败原因枚举
- `message`：错误描述
- `masterId`：Master 设备 ID

### finished

```cpp
void finished(bool success,
              SH85SelfChecker::Result result,
              const QString& message,
              const QString& masterId);
```

**触发时机**：自检流程结束（成功 / 失败 / 取消都会发出）

**参数说明**
- `success`：是否成功
- `result`：结果枚举
- `message`：描述信息
- `masterId`：Master 设备 ID

## 使用示例

### 基本使用

```cpp
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

// 获取 master 实例
ModbusTcpMaster* master = ModbusTcpMasterManager::instance().getMaster(qrcode);
if (!master) return;

// 获取自检器
SH85SelfChecker* checker = master->selfChecker();

// 连接信号
connect(checker, &SH85SelfChecker::started, [](const QString& masterId) {
    qDebug() << "自检已启动:" << masterId;
});

connect(checker, &SH85SelfChecker::stateChanged, [](SH85SelfChecker::State state, const QString& masterId) {
    qDebug() << "状态变更:" << SH85SelfChecker::stateToString(state) << masterId;
});

connect(checker, &SH85SelfChecker::errorOccurred, [](SH85SelfChecker::Result result, const QString& msg, const QString& masterId) {
    qWarning() << "自检失败:" << SH85SelfChecker::resultToString(result) << msg << masterId;
});

connect(checker, &SH85SelfChecker::finished, [](bool success, SH85SelfChecker::Result result, const QString& msg, const QString& masterId) {
    qDebug() << "自检结束:" << success << SH85SelfChecker::resultToString(result) << msg << masterId;
});

// 启动自检
if (!checker->start()) {
    qWarning() << "启动失败，前置条件不满足";
}
```

### 取消自检

```cpp
// 主动取消
checker->stop();
```

### 多设备并行自检

```cpp
// 对多个设备同时启动自检
for (const QString& qrcode : qrcodeList) {
    ModbusTcpMaster* master = ModbusTcpMasterManager::instance().getMaster(qrcode);
    if (master) {
        SH85SelfChecker* checker = master->selfChecker();
        checker->start();
    }
}
```

## 注意事项

1. **前置条件检查**：调用 `start()` 前需确保 master 已连接且指令池中存在所需指令模板，否则会返回 false 且不发信号。

2. **线程安全**：所有公共接口均可跨线程调用，内部使用 `QMetaObject::invokeMethod` 和 `Qt::QueuedConnection` 保证线程安全。

3. **重复启动**：若当前已在运行状态，`start()` 会直接返回 false，忽略重复启动请求。

4. **信号顺序**：失败时先发出 `errorOccurred` 信号，再发出 `finished` 信号；成功或取消时仅发出 `finished` 信号。

5. **状态查询**：可通过 `currentState()` 和 `isRunning()` 实时查询自检状态。

6. **资源清理**：自检结束（成功/失败/取消）后会自动断开 sender 信号槽并停止所有定时器，无需手动清理。

7. **多实例隔离**：每个 master 自带一个自检器实例，多设备自检互不干扰。

8. **日志记录**：所有关键操作（启动、状态变更、错误、结束）都会写入对应 master 的日志文件中。
