# 软件层面调试测试报告

| 项目 | 内容 |
|------|------|
| 测试日期 | 2026-05-22 |
| 测试类型 | 仅软件层面的调试测试 |
| 紧急类型 | 普通 |

---

## 1. Idle Purge

### 1.1 设置 Idle 使能指令（WriteIdlePurgeEnable）

| 场景 | 设备 | 结果 | 耗时 |
|------|------|------|------|
| 网络断开 | 12002 | 失败（设备未连接，跳过下发） | < 1s |
| 设备不响应 | 12002 | 失败（超时，重试 5 次） | ~7s |
| 正常 | 12001 | 成功 | 110 ms |

#### 场景一：网络断开

```
[2026-05-22 15:41:09] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12002
  属性: Idle Purge Enable
  值: enable
  时间: 2026-05-22 15:41:09
[2026-05-22 15:41:09] [WARN] [SetIdlePurgeTask][start] 设备 12002 未连接，跳过下发
[2026-05-22 15:41:09] [WARN] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12002
  属性: Idle Purge Enable
  值: enable
  结果: 失败
  时间: 2026-05-22 15:41:09
```

#### 场景二：设备不响应

```
[2026-05-22 15:46:33] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12002
  属性: Idle Purge Enable
  值: enable
  时间: 2026-05-22 15:46:33
[2026-05-22 15:46:33] [INFO] [SetIdlePurgeTask][start] 向设备 12002 发送 WriteIdlePurgeEnable 真实值=enable 写入值=1
[2026-05-22 15:46:35] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (0/5)
[2026-05-22 15:46:36] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (1/5)
[2026-05-22 15:46:37] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (2/5)
[2026-05-22 15:46:38] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (3/5)
[2026-05-22 15:46:39] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (4/5)
[2026-05-22 15:46:40] [WARN] [SetIdlePurgeTask][onCommandFinished] 设备 12002 设置失败: timedOut=1 checksumError=0 deviceBusy=0
[2026-05-22 15:46:40] [WARN] [SetIdlePurgeTask][onCommandFinished] 失败指令详细信息:
  ID: WriteIdlePurgeEnable
  发送时间: 2026-05-22 15:46:39.230
  响应时间: 
  使用时间: -
  状态: 超时
  重试次数: 5
  指令UUID: 400
  请求帧: 01 06 00 14 00 01 08 0E
  响应帧: 超时, 等待RTU响应超时, 01 06 00 14 00 01
  请求帧CRC: 08 0E
  响应帧CRC: 无
[2026-05-22 15:46:40] [WARN] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12002
  属性: Idle Purge Enable
  值: enable
  结果: 失败
  时间: 2026-05-22 15:46:40
```

#### 场景三：正常

```
[2026-05-22 15:44:16] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12001
  属性: Idle Purge Enable
  值: enable
  时间: 2026-05-22 15:44:16
[2026-05-22 15:44:16] [INFO] [SetIdlePurgeTask][start] 向设备 12001 发送 WriteIdlePurgeEnable 真实值=enable 写入值=1
[2026-05-22 15:44:16] [INFO] [SetIdlePurgeTask][onCommandFinished] 设备 12001 设置成功
[2026-05-22 15:44:16] [INFO] [SetIdlePurgeTask][onCommandFinished] 指令详细信息:
  ID: WriteIdlePurgeEnable
  发送时间: 2026-05-22 15:44:16.401
  响应时间: 2026-05-22 15:44:16.511
  使用时间: 110 ms
  状态: 成功
  重试次数: 0
  指令UUID: 479
  请求帧: 01 06 00 14 00 01 08 0E
  响应帧: 01 06 00 14 00 01 08 0E
  请求帧CRC: 08 0E
  响应帧CRC: 08 0E
[2026-05-22 15:44:16] [INFO] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12001
  属性: Idle Purge Enable
  值: enable
  结果: 成功
  时间: 2026-05-22 15:44:16
```

---

### 1.2 设置 Idle 充气持续时间（WriteIdlePurgeTime）

| 场景 | 设备 | 结果 | 耗时 |
|------|------|------|------|
| 网络断开 | 12002 | 失败（设备未连接，跳过下发） | < 1s |
| 设备不响应 | 12002 | 失败（超时，重试 5 次） | ~7s |
| 正常 | 12001 | 成功 | 110 ms |

#### 场景一：网络断开

```
[2026-05-22 15:41:11] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12002
  属性: Purge Duration
  值: 10 s
  时间: 2026-05-22 15:41:11
[2026-05-22 15:41:11] [WARN] [SetIdlePurgeTask][start] 设备 12002 未连接，跳过下发
[2026-05-22 15:41:11] [WARN] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12002
  属性: Purge Duration
  值: 10 s
  结果: 失败
  时间: 2026-05-22 15:41:11
```

#### 场景二：设备不响应

```
[2026-05-22 15:46:45] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12002
  属性: Purge Duration
  值: 10 s
  时间: 2026-05-22 15:46:45
[2026-05-22 15:46:45] [INFO] [SetIdlePurgeTask][start] 向设备 12002 发送 WriteIdlePurgeTime 真实值=10 s 写入值=10
[2026-05-22 15:46:47] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (0/5)
[2026-05-22 15:46:48] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (1/5)
[2026-05-22 15:46:49] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (2/5)
[2026-05-22 15:46:50] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (3/5)
[2026-05-22 15:46:51] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (4/5)
[2026-05-22 15:46:52] [WARN] [SetIdlePurgeTask][onCommandFinished] 设备 12002 设置失败: timedOut=1 checksumError=0 deviceBusy=0
[2026-05-22 15:46:52] [WARN] [SetIdlePurgeTask][onCommandFinished] 失败指令详细信息:
  ID: WriteIdlePurgeTime
  发送时间: 2026-05-22 15:46:51.188
  响应时间: 
  使用时间: -
  状态: 超时
  重试次数: 5
  指令UUID: 479
  请求帧: 01 06 00 15 00 0A 18 09
  响应帧: 超时, 等待RTU响应超时, 01 06 00 15 00 0A
  请求帧CRC: 18 09
  响应帧CRC: 无
[2026-05-22 15:46:52] [WARN] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12002
  属性: Purge Duration
  值: 10 s
  结果: 失败
  时间: 2026-05-22 15:46:52
```

#### 场景三：正常

```
[2026-05-22 15:43:26] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12001
  属性: Purge Duration
  值: 10 s
  时间: 2026-05-22 15:43:26
[2026-05-22 15:43:26] [INFO] [SetIdlePurgeTask][start] 向设备 12001 发送 WriteIdlePurgeTime 真实值=10 s 写入值=10
[2026-05-22 15:43:26] [INFO] [SetIdlePurgeTask][onCommandFinished] 设备 12001 设置成功
[2026-05-22 15:43:26] [INFO] [SetIdlePurgeTask][onCommandFinished] 指令详细信息:
  ID: WriteIdlePurgeTime
  发送时间: 2026-05-22 15:43:26.151
  响应时间: 2026-05-22 15:43:26.261
  使用时间: 110 ms
  状态: 成功
  重试次数: 0
  指令UUID: 400
  请求帧: 01 06 00 15 00 0A 18 09
  响应帧: 01 06 00 15 00 0A 18 09
  请求帧CRC: 18 09
  响应帧CRC: 18 09
[2026-05-22 15:43:26] [INFO] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12001
  属性: Purge Duration
  值: 10 s
  结果: 成功
  时间: 2026-05-22 15:43:26
```

---

### 1.3 设置 Idle 充气时间间隔（WriteIdlePurgeInterval）

| 场景 | 设备 | 结果 | 耗时 |
|------|------|------|------|
| 网络断开 | 12001 | 失败（设备未连接，跳过下发） | < 1s |
| 设备不响应 | 12002 | 失败（超时，重试 5 次） | ~6s |
| 正常 | 12001 | 成功 | 110 ms |

#### 场景一：网络断开

```
[2026-05-22 15:25:26] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12001
  属性: Purge Interval
  值: 5 s
  时间: 2026-05-22 15:25:26
[2026-05-22 15:25:26] [WARN] [SetIdlePurgeTask][start] 设备 12001 未连接，跳过下发
[2026-05-22 15:25:33] [WARN] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12001
  属性: Purge Interval
  值: 5 s
  结果: 失败
  时间: 2026-05-22 15:25:33
```

#### 场景二：设备不响应

```
[2026-05-22 15:46:54] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12002
  属性: Purge Interval
  值: 5 s
  时间: 2026-05-22 15:46:54
[2026-05-22 15:46:54] [INFO] [SetIdlePurgeTask][start] 向设备 12002 发送 WriteIdlePurgeInterval 真实值=5 s 写入值=5
[2026-05-22 15:46:55] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (0/5)
[2026-05-22 15:46:56] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (1/5)
[2026-05-22 15:46:57] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (2/5)
[2026-05-22 15:46:58] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (3/5)
[2026-05-22 15:46:59] [WARN] [SetIdlePurgeTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (4/5)
[2026-05-22 15:47:00] [WARN] [SetIdlePurgeTask][onCommandFinished] 设备 12002 设置失败: timedOut=1 checksumError=0 deviceBusy=0
[2026-05-22 15:47:00] [WARN] [SetIdlePurgeTask][onCommandFinished] 失败指令详细信息:
  ID: WriteIdlePurgeInterval
  发送时间: 2026-05-22 15:46:59.904
  响应时间: 
  使用时间: -
  状态: 超时
  重试次数: 5
  指令UUID: 558
  请求帧: 01 06 00 16 00 05 A8 0D
  响应帧: 超时, 等待RTU响应超时, 01 06 00 16 00 05
  请求帧CRC: A8 0D
  响应帧CRC: 无
[2026-05-22 15:47:00] [WARN] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12002
  属性: Purge Interval
  值: 5 s
  结果: 失败
  时间: 2026-05-22 15:47:00
```

#### 场景三：正常

```
[2026-05-22 15:44:18] [INFO] ============================= SetIdlePurgeTask 任务开始 =============================
  设备: 12001
  属性: Purge Interval
  值: 5 s
  时间: 2026-05-22 15:44:18
[2026-05-22 15:44:18] [INFO] [SetIdlePurgeTask][start] 向设备 12001 发送 WriteIdlePurgeInterval 真实值=5 s 写入值=5
[2026-05-22 15:44:18] [INFO] [SetIdlePurgeTask][onCommandFinished] 设备 12001 设置成功
[2026-05-22 15:44:18] [INFO] [SetIdlePurgeTask][onCommandFinished] 指令详细信息:
  ID: WriteIdlePurgeInterval
  发送时间: 2026-05-22 15:44:18.120
  响应时间: 2026-05-22 15:44:18.230
  使用时间: 110 ms
  状态: 成功
  重试次数: 0
  指令UUID: 558
  请求帧: 01 06 00 16 00 05 A8 0D
  响应帧: 01 06 00 16 00 05 A8 0D
  请求帧CRC: A8 0D
  响应帧CRC: A8 0D
[2026-05-22 15:44:18] [INFO] ============================= SetIdlePurgeTask 任务结束 =============================
  设备: 12001
  属性: Purge Interval
  值: 5 s
  结果: 成功
  时间: 2026-05-22 15:44:18
```

---

## 2. SetPurgeFlow（WritePurgeFlow）

| 场景 | 设备 | 结果 | 耗时 |
|------|------|------|------|
| 网络断开 | 12001 | 失败（设备不可用，跳过下发） | < 1s |
| 设备不响应 | 12003 | 失败（超时，重试 5 次） | ~7s |
| 正常 | 12001 | 成功 | 109 ms |

#### 场景一：网络断开

```
[2026-05-22 15:53:11] [INFO] ============================= SetPurgeFlowTask 任务开始 =============================
  设备: 12001
  流量: 35 L/Min
  时间: 2026-05-22 15:53:11
[2026-05-22 15:53:11] [WARN] [SetPurgeFlowTask][start] 设备 12001 不可用，跳过下发
[2026-05-22 15:53:18] [WARN] ============================= SetPurgeFlowTask 任务结束 =============================
  设备: 12001
  流量: 35 L/Min
  结果: 失败
  时间: 2026-05-22 15:53:18
```

#### 场景二：设备不响应

```
[2026-05-22 15:53:11] [INFO] ============================= SetPurgeFlowTask 任务开始 =============================
  设备: 12003
  流量: 35 L/Min
  时间: 2026-05-22 15:53:11
[2026-05-22 15:53:11] [INFO] [SetPurgeFlowTask][start] 向设备 12003 发送 WritePurgeFlow 真实值=35 L/Min 写入值=3500
[2026-05-22 15:53:12] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12003 指令超时，正在重试 (0/5)
[2026-05-22 15:53:13] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12003 指令超时，正在重试 (1/5)
[2026-05-22 15:53:14] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12003 指令超时，正在重试 (2/5)
[2026-05-22 15:53:15] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12003 指令超时，正在重试 (3/5)
[2026-05-22 15:53:16] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12003 指令超时，正在重试 (4/5)
[2026-05-22 15:53:17] [WARN] [SetPurgeFlowTask][onCommandFinished] 设备 12003 设置失败: timedOut=1 checksumError=0 deviceBusy=0
[2026-05-22 15:53:17] [WARN] [SetPurgeFlowTask][onCommandFinished] 失败指令详细信息:
  ID: WritePurgeFlow
  发送时间: 2026-05-22 15:53:16.886
  响应时间: 
  使用时间: -
  状态: 超时
  重试次数: 5
  指令UUID: 401
  请求帧: 01 06 00 00 0D AC 8D 27
  响应帧: 超时, 等待RTU响应超时, 01 06 00 00 0D AC
  请求帧CRC: 8D 27
  响应帧CRC: 无
[2026-05-22 15:53:18] [WARN] ============================= SetPurgeFlowTask 任务结束 =============================
  设备: 12003
  流量: 35 L/Min
  结果: 失败
  时间: 2026-05-22 15:53:18
```

#### 场景三：正常

```
[2026-05-22 15:55:32] [INFO] ============================= SetPurgeFlowTask 任务开始 =============================
  设备: 12001
  流量: 35 L/Min
  时间: 2026-05-22 15:55:32
[2026-05-22 15:55:32] [INFO] [SetPurgeFlowTask][start] 向设备 12001 发送 WritePurgeFlow 真实值=35 L/Min 写入值=3500
[2026-05-22 15:55:32] [INFO] [SetPurgeFlowTask][onCommandFinished] 设备 12001 设置成功
[2026-05-22 15:55:32] [INFO] [SetPurgeFlowTask][onCommandFinished] 指令详细信息:
  ID: WritePurgeFlow
  发送时间: 2026-05-22 15:55:32.511
  响应时间: 2026-05-22 15:55:32.620
  使用时间: 109 ms
  状态: 成功
  重试次数: 0
  指令UUID: 400
  请求帧: 01 06 00 00 0D AC 8D 27
  响应帧: 01 06 00 00 0D AC 8D 27
  请求帧CRC: 8D 27
  响应帧CRC: 8D 27
[2026-05-22 15:55:32] [INFO] ============================= SetPurgeFlowTask 任务结束 =============================
  设备: 12001
  流量: 35 L/Min
  结果: 成功
  时间: 2026-05-22 15:55:32
```

---

## 3. SetHumidityOffsetTask（WriteHumidityOffset）

### 3.1 WriteHumidityOffsetThreshold

| 场景 | 设备 | 结果 | 耗时 |
|------|------|------|------|
| 网络断开 | 12001 | 失败（设备不可用，跳过下发） | < 1s |
| 设备不响应 | 12002 | 失败（超时，重试 5 次） | ~7s |
| 正常 | 12001 | 成功 | 110 ms |

#### 场景一：网络断开

```
[2026-05-22 18:58:58] [INFO] ============================= SetHumidityOffsetTask 任务开始 =============================
  设备: 12001
  Offset: 0%
  时间: 2026-05-22 18:58:58
[2026-05-22 18:58:58] [WARN] [SetHumidityOffsetTask][start] 设备 12001 不可用，跳过下发
[2026-05-22 18:59:05] [WARN] ============================= SetHumidityOffsetTask 任务结束 =============================
  设备: 12001
  Offset: 0%
  结果: 失败
  时间: 2026-05-22 18:59:05
```

#### 场景二：设备不响应

```
[2026-05-22 18:58:58] [INFO] ============================= SetHumidityOffsetTask 任务开始 =============================
  设备: 12002
  Offset: 0%
  时间: 2026-05-22 18:58:58
[2026-05-22 18:58:58] [INFO] [SetHumidityOffsetTask][start] 向设备 12002 下发 1 条子指令
[2026-05-22 18:59:00] [WARN] [SetHumidityOffsetTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (0/5)
[2026-05-22 18:59:01] [WARN] [SetHumidityOffsetTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (1/5)
[2026-05-22 18:59:02] [WARN] [SetHumidityOffsetTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (2/5)
[2026-05-22 18:59:03] [WARN] [SetHumidityOffsetTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (3/5)
[2026-05-22 18:59:04] [WARN] [SetHumidityOffsetTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (4/5)
[2026-05-22 18:59:05] [WARN] [SetHumidityOffsetTask][onCommandFinished] 设备 12002 子指令 Offset 失败: timedOut=1 checksumError=0 deviceBusy=0
[2026-05-22 18:59:05] [WARN] [SetHumidityOffsetTask][onCommandFinished] 失败指令详细信息:
  ID: WriteHumidityOffset
  发送时间: 2026-05-22 18:59:04.269
  响应时间: 
  使用时间: -
  状态: 超时
  重试次数: 5
  指令UUID: 400
  请求帧: 01 06 00 18 00 00 09 CD
  响应帧: 超时, 等待RTU响应超时, 01 06 00 18 00 00
[2026-05-22 18:59:05] [WARN] ============================= SetHumidityOffsetTask 任务结束 =============================
  设备: 12002
  Offset: 0%
  结果: 失败
  时间: 2026-05-22 18:59:05
```

#### 场景三：正常

```
[2026-05-22 19:02:29] [INFO] ============================= SetHumidityOffsetTask 任务开始 =============================
  设备: 12001
  Offset: 0%
  时间: 2026-05-22 19:02:29
[2026-05-22 19:02:29] [INFO] [SetHumidityOffsetTask][start] 向设备 12001 下发 1 条子指令
[2026-05-22 19:02:29] [INFO] [SetHumidityOffsetTask][onCommandFinished] 设备 12001 子指令 Offset 成功
[2026-05-22 19:02:29] [INFO] [SetHumidityOffsetTask][onCommandFinished] 指令详细信息:
  ID: WriteHumidityOffset
  发送时间: 2026-05-22 19:02:29.386
  响应时间: 2026-05-22 19:02:29.496
  使用时间: 110 ms
  状态: 成功
  重试次数: 0
  指令UUID: 400
  请求帧: 01 06 00 18 00 00 09 CD
  响应帧: 01 06 00 18 00 00 09 CD
[2026-05-22 19:02:29] [INFO] ============================= SetHumidityOffsetTask 任务结束 =============================
  设备: 12001
  Offset: 0%
  结果: 成功
  时间: 2026-05-22 19:02:29
```

---

## 4. 磁盘异常处理机制

| 场景 | 磁盘使用率 | 结果 | 备注 |
|------|-----------|------|------|
| 正常 | 33.9% | 无需清理 | |
| 磁盘溢出 | > 90.0% | 触发清理，删除 158 条记录 | ⚠️ 应清空所有日志，实际仅删除 158 条 |

> **⚠️ 已知问题**：当前清理机制达到 90% 阈值后仅删除部分日志记录，未能清空所有日志。正确行为应为：**磁盘使用率达到 90% 阈值时，清空所有日志记录**。

#### 场景一：正常

```
[2026-05-22 19:11:05] [INFO] 磁盘空间检测: 使用率 33.9% (已用 0.34 GB / 1.00 GB), 阈值 90%, 需要清理: 否
```

#### 场景二：磁盘溢出

```
[2026-05-22 19:04:38] [INFO] 磁盘空间检查定时器已启动，间隔: 60000ms, 阈值: 90%
[2026-05-22 19:05:38] [INFO] 磁盘空间检测: 使用率 100.0% (已用 1.00 GB / 1.00 GB), 阈值 90%, 需要清理: 是
[2026-05-22 19:05:38] [WARN] 磁盘使用率超过阈值，触发日志清理
[2026-05-22 19:05:38] [INFO] 正在清理日志: 2026-05-22 00:00:00 至 2026-06-22 23:59:59, 涉及月份: [2026-05, 2026-06], 预计删除记录数: 158, 数据库最早: 2026-05-22, 最新: 2026-05-22
[2026-05-22 19:05:38] [INFO] 清理成功: 已删除 2026-05-22 00:00:00 至 2026-06-22 23:59:59 的日志记录, 涉及月份: [2026-05, 2026-06], 删除记录数: 158
```

---

## 测试结论汇总

| 功能模块 | 测试项 | 网络断开 | 设备不响应 | 正常 |
|----------|--------|----------|-----------|------|
| Idle Purge | WriteIdlePurgeEnable | ✅ 正确失败 | ✅ 超时重试后失败 | ✅ 成功 |
| Idle Purge | WriteIdlePurgeTime | ✅ 正确失败 | ✅ 超时重试后失败 | ✅ 成功 |
| Idle Purge | WriteIdlePurgeInterval | ✅ 正确失败 | ✅ 超时重试后失败 | ✅ 成功 |
| Purge Flow | WritePurgeFlow | ✅ 正确失败 | ✅ 超时重试后失败 | ✅ 成功 |
| Humidity Offset | WriteHumidityOffset | ✅ 正确失败 | ✅ 超时重试后失败 | ✅ 成功 |
| 磁盘管理 | 磁盘空间检测与清理 | - | - | ⚠️ 清理触发正常，但未清空所有日志（见已知问题） |

### 验证要点

- **日志格式**：所有任务均按统一格式输出，包含任务开始/结束边界标记、设备信息、指令详情
- **超时重试**：设备不响应时正确触发 5 次重试（0/5 ~ 4/5），最终标记为失败
- **指令详情**：成功和失败均输出完整指令信息（ID、时间、状态、请求帧、响应帧）
- **CRC 校验码**：请求帧包含 CRC，响应帧在成功时包含 CRC、超时时显示"无"
- **磁盘清理**：磁盘使用率超过 90% 阈值时自动触发日志清理，日志记录清理范围和删除数量。⚠️ 但未清空所有日志，需改进为达到阈值时清空全部日志
