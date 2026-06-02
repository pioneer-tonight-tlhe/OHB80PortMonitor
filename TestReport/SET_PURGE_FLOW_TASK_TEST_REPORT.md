# SetPurgeFlowTask 测试报告

**测试时间**: 2026-05-21 17:23 - 17:25  
**任务名称**: SetPurgeFlowTask（设置 Purge 流量）  
**测试设备**: 12001, 12002, 12008  
**测试参数**: 流量 35 L/Min（寄存器值 3500）

---

## 测试环境

- **任务文件**: `scheduler/tasks/set_purge_flow_task.h` / `.cpp`
- **指令 ID**: WritePurgeFlow (FC 0x06, addr 0x0000)
- **寄存器倍率**: flow × 100

### 日志目录结构

```
logs/
├── scheduler/
│   └── set_purge_flow_task/           # SetPurgeFlow 任务
│       ├── 12001.log                  # 设备 12001 的日志
│       ├── 12002.log                  # 设备 12002 的日志
│       ├── 12008.log                  # 设备 12008 的日志
│       └── ...
```

**日志路径格式：** `scheduler/set_purge_flow_task/<deviceId>`

---

## 测试场景

### 场景 1: 设备不可用（断开连接）

**测试设备**: 12001  
**预期行为**: 跳过下发，记录失败日志，写入运行日志

**日志验证**:
```log
[2026-05-21 17:23:34] [WARN] [SetPurgeFlowTask][start] 设备 12001 不可用，跳过下发
```

**验证结果**: ✅ 通过
- 设备级别日志正确记录
- 日志路径: `scheduler/set_purge_flow_task/12001`
- 立即 flush 确保日志写入

---

### 场景 2: 设备超时重试

**测试设备**: 12002  
**预期行为**: 超时后自动重试，记录每次重试，最终标记失败

**日志验证**:
```log
[2026-05-21 17:23:34] [INFO] [SetPurgeFlowTask][start] 向设备 12002 发送 WritePurgeFlow 真实值=35 L/Min 写入值=3500
[2026-05-21 17:23:35] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (0/5)
[2026-05-21 17:23:36] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (1/5)
[2026-05-21 17:23:37] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (2/5)
[2026-05-21 17:23:38] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (3/5)
[2026-05-21 17:23:39] [WARN] [SetPurgeFlowTask][onCommandTimeoutRetry] 设备 12002 指令超时，正在重试 (4/5)
[2026-05-21 17:23:40] [WARN] [SetPurgeFlowTask][onCommandFinished] 设备 12002 设置失败: timedOut=1 checksumError=0 deviceBusy=0
```

**指令详情**:
```
  ID: WritePurgeFlow
  发送时间: 2026-05-21 17:23:39.782
  响应时间: 
  使用时间: -
  状态: 超时
  重试次数: 5
  指令UUID: 400
  请求帧: 01 06 00 00 0D AC
  响应帧: 超时, 等待RTU响应超时
```

**验证结果**: ✅ 通过
- 重试机制正常工作（重试 5 次）
- 每次重试都有日志记录
- 时间格式精确到毫秒
- 显示使用时间（超时时显示 "-"）
- 区分真实值和写入值
- 记录完整的指令信息（请求帧、失败原因）

---

### 场景 3: 设备成功响应

**测试设备**: 12008  
**预期行为**: 指令发送成功，记录响应时间，标记成功

**日志验证**:
```log
[2026-05-21 17:24:50] [INFO] [SetPurgeFlowTask][start] 向设备 12008 发送 WritePurgeFlow 真实值=35 L/Min 写入值=3500
[2026-05-21 17:24:51] [INFO] [SetPurgeFlowTask][onCommandFinished] 设备 12008 指令信息:
  ID: WritePurgeFlow
  发送时间: 2026-05-21 17:24:50.948
  响应时间: 2026-05-21 17:24:51.011
  使用时间: 63 ms
  状态: 成功
  重试次数: 0
  指令UUID: 406
  请求帧: 01 06 00 00 0D AC
  响应帧: 01 06 00 00 0D AC
[2026-05-21 17:24:51] [INFO] [SetPurgeFlowTask][onCommandFinished] 设备 12008 设置成功
```

**验证结果**: ✅ 通过
- 指令信息完整
- 时间格式精确到毫秒（.948, .011）
- 显示使用时间（63 ms）
- 请求帧和响应帧正确
- 真实值和写入值区分显示
- 无单独的请求帧日志（已移除）
