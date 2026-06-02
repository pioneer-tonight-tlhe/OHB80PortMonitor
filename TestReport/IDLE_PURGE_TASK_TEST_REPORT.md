# Idle Purge Task 测试报告

**测试日期**: 2026-05-21  
**测试人员**: 系统自动化测试  
**测试模块**: SetIdlePurgeTask (Idle Purge 参数批量设置任务)

---

## 一、测试概述

### 1.1 测试目的
验证 SetIdlePurgeTask 在不同设备状态下的行为，包括：
- 正常设备指令下发和响应
- 未连接设备的跳过处理
- 超时设备的重试机制
- 日志记录的完整性和格式

### 1.2 测试环境
- 设备数量: 3 台 (12001, 12002, 12004)
- 测试指令:
  - WriteIdlePurgeEnable (使能设置)
  - WriteIdlePurgeTime (充气时长设置)
  - WriteIdlePurgeInterval (充气间隔设置)

### 1.3 日志目录结构

```
logs/
├── scheduler/
│   └── set_idle_purge_task/           # SetIdlePurge 任务
│       ├── 12001.log                  # 设备 12001 的日志
│       ├── 12002.log                  # 设备 12002 的日志
│       ├── 12004.log                  # 设备 12004 的日志
│       └── ...
```

**日志路径格式：** `scheduler/set_idle_purge_task/<deviceId>`

---

## 二、测试用例

### 2.1 测试用例 1: 正常设备响应测试

**设备**: 12001  
**设备状态**: 在线连接

#### 测试记录

| 时间 | 指令 | 值 | 状态 | 响应时间 | 重试次数 | 请求帧 | 响应帧 |
|------|------|-----|------|----------|----------|--------|--------|
| 15:28:16 | WriteIdlePurgeEnable | 1 | 成功 | 16ms | 0 | 01 06 00 14 00 01 | 01 06 00 14 00 01 |
| 15:28:32 | WriteIdlePurgeTime | 10 | 成功 | 0ms | 0 | 01 06 00 15 00 0A | 01 06 00 15 00 0A |
| 15:28:35 | WriteIdlePurgeInterval | 5 | 成功 | 0ms | 0 | 01 06 00 16 00 05 | 01 06 00 16 00 05 |
| 15:44:28 | WriteIdlePurgeEnable | 1 | 成功 | 0ms | 0 | 01 06 00 14 00 01 | 01 06 00 14 00 01 |
| 16:05:26 | WriteIdlePurgeEnable | 1 | 成功 | 0ms | 0 | 01 06 00 14 00 01 | 01 06 00 14 00 01 |

**测试结果**: ✅ **通过**

**日志验证**:
- 发送时记录基本信息和请求帧
- 响应后记录完整指令信息（包含发送时间、响应时间、状态、重试次数、请求帧、响应帧）
- 16:05:26 测试新增了发送时的请求帧记录
- 所有日志立即 flush 到磁盘

---

### 2.2 测试用例 2: 未连接设备跳过测试

**设备**: 12002  
**设备状态**: 未连接

#### 测试记录

| 时间 | 指令 | 值 | 处理结果 | 日志级别 |
|------|------|-----|----------|----------|
| 15:28:16 | WriteIdlePurgeEnable | 1 | 跳过下发 | WARN |
| 15:28:32 | WriteIdlePurgeTime | 10 | 跳过下发 | WARN |
| 15:28:35 | WriteIdlePurgeInterval | 5 | 跳过下发 | WARN |
| 15:44:28 | WriteIdlePurgeEnable | 1 | 跳过下发 | WARN |
| 16:05:26 | WriteIdlePurgeEnable | 1 | 跳过下发 | WARN |

**测试结果**: ✅ **通过**

**日志验证**:
- 设备未连接时正确跳过下发
- 记录 WARN 级别日志: `[SetIdlePurgeTask][start] 设备 12002 未连接，跳过下发`
- 设备级别的日志文件正常生成
- 日志立即 flush 到磁盘

---

### 2.3 测试用例 3: 超时设备重试机制测试

**设备**: 12004  
**设备状态**: 在线但不响应

#### 测试记录

| 时间 | 指令 | 值 | 状态 | 重试次数 | 响应帧 |
|------|------|-----|------|----------|--------|
| 16:04:34 | WriteIdlePurgeEnable | 1 | 超时 | 5 | 超时, 等待RTU响应超时 |
| 16:06:51 | WriteIdlePurgeTime | 10 | 超时 | 5 | 超时, 等待RTU响应超时 |
| 16:07:01 | WriteIdlePurgeInterval | 5 | 超时 | 5 | 超时, 等待RTU响应超时 |

**重试日志示例** (WriteIdlePurgeEnable):
```
16:04:34 [INFO] 向设备 12004 发送 WriteIdlePurgeEnable 值=1
16:04:34 [INFO] 请求帧: 01 06 00 14 00 01
16:04:36 [WARN] 设备 12004 指令超时，正在重试 (0/5)
16:04:37 [WARN] 设备 12004 指令超时，正在重试 (1/5)
16:04:38 [WARN] 设备 12004 指令超时，正在重试 (2/5)
16:04:39 [WARN] 设备 12004 指令超时，正在重试 (3/5)
16:04:40 [WARN] 设备 12004 指令超时，正在重试 (4/5)
16:04:41 [INFO] 设备 12004 设置失败: timedOut=1 checksumError=0 deviceBusy=0
16:04:41 [WARN] 失败指令详细信息: [完整指令信息]
```

**测试结果**: ✅ **通过**

**日志验证**:
- 发送时记录基本信息和请求帧
- 每次重试都记录 `onCommandTimeoutRetry` 日志，显示重试进度 (0/5 ~ 4/5)
- 重试次数耗尽后记录 `onCommandFinished` 失败日志
- 失败时记录完整的指令详细信息
- 响应帧字段正确显示失败原因: "超时, 等待RTU响应超时"
- 所有日志立即 flush 到磁盘

---

## 三、功能验证

### 3.1 连接 commandTimeoutRetry 信号
**验证结果**: ✅ 通过

- 正确连接 `ModbusCommandSender::commandTimeoutRetry` 到 `SetIdlePurgeTask::onCommandTimeoutRetry`
- 重试时记录设备级别日志: `[SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12004 指令超时，正在重试 (X/5)`
- 发射 `deviceRetrying` 信号通知 UI 层

### 3.2 UI 重试状态显示

![image-20260521163250848](C:\Users\Lenovo\AppData\Roaming\Typora\typora-user-images\image-20260521163250848.png)

**验证结果**: ✅ 通过

- UI 层正确连接 `deviceRetrying` 信号
- 状态标签宽度自适应，可显示完整重试信息
- 使用 `setStatusWaiting()` 显示重试状态

### 3.3 响应帧失败原因显示
**验证结果**: ✅ 通过

- `ModbusCommand::toLogString()` 在失败时优先显示失败原因
- 失败原因包括: 超时、校验错误、设备忙、errorMessage
- 成功时显示十六进制响应帧

---

## 四、日志格式验证

### 4.1 发送时日志格式
```
[时间] [INFO] [SetIdlePurgeTask][start] 向设备 12001 发送 WriteIdlePurgeEnable 值=1
[时间] [INFO] [SetIdlePurgeTask][start] 请求帧: 01 06 00 14 00 01
```

### 4.2 响应成功日志格式
```
[时间] [INFO] [SetIdlePurgeTask][onCommandFinished] 设备 12001 指令信息:
  ID: WriteIdlePurgeEnable
  发送时间: 2026-05-21 15:28:16
  响应时间: 2026-05-21 15:28:16
  状态: 成功
  重试次数: 0
  指令UUID: 400
  请求帧: 01 06 00 14 00 01
  响应帧: 01 06 00 14 00 01
```

### 4.3 响应失败日志格式
```
[时间] [WARN] [SetIdlePurgeTask][onCommandFinished] 设备 12004 设置失败: timedOut=1 checksumError=0 deviceBusy=0
[时间] [WARN] [SetIdlePurgeTask][onCommandFinished] 失败指令详细信息:
  ID: WriteIdlePurgeEnable
  发送时间: 2026-05-21 16:04:40
  响应时间: 
  状态: 超时
  重试次数: 5
  指令UUID: 402
  请求帧: 01 06 00 14 00 01
  响应帧: 超时, 等待RTU响应超时
```

### 4.4 重试日志格式
```
[时间] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12004 指令超时，正在重试 (0/5)
```

