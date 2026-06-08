# 任务日志对应关系文档

本文档记录各个 Scheduler Task 与运行日志（OperationDispatchTask）、警报日志（AlarmDispatchTask）的对应关系。

---

## ReadVEFCFlowUnitAndMediumStatusTask

### 功能
读取 VEFC 流量单元和介质状态。

### 运行日志（OperationDispatchTask）

#### 启动日志
- **位置**: `start()` 方法
- **条件**: 任务正常启动，设备列表不为空且指令池包含所需指令
- **级别**: `Message`
- **内容**: `ReadVEFCStatus task started: X devices`
- **参数**: X - 设备数量

#### 早期失败日志
- **位置**: `start()` 方法
- **条件**: 设备列表为空
- **级别**: `Warn`
- **内容**: `ReadVEFCStatus task failed: device identification failed`

- **位置**: `start()` 方法
- **条件**: 指令池中找不到所需指令
- **级别**: `Warn`
- **内容**: `ReadVEFCStatus task failed: feature not implemented`

#### 超时日志
- **位置**: `onTimeout()` 方法
- **条件**: 任务超时，有设备未响应
- **级别**: `Error`
- **内容**: `ReadVEFCStatus task timeout: X devices did not respond (device1, device2, ...)`
- **参数**: X - 未响应设备数量，device1, device2, ... - 具体设备列表

#### 完成日志
- **位置**: `forceFinish()` 方法
- **条件**: 任务完成，所有设备成功
- **级别**: `Message`
- **内容**: `ReadVEFCStatus task completed: X/Y devices succeeded`
- **参数**: X - 成功设备数量，Y - 总设备数量

- **位置**: `forceFinish()` 方法
- **条件**: 任务完成，有设备失败
- **级别**: `Error`
- **内容**: `ReadVEFCStatus task failed: X/Y devices succeeded. Failed devices: device1 (reason); device2 (reason); ...`
- **参数**: 
  - X - 成功设备数量
  - Y - 总设备数量
  - device1, device2, ... - 失败设备列表
  - reason - 失败原因：
    - `communication failed` - 通信失败（unitRaw=0, mediumRaw=0）
    - `unit abnormal` - 单元状态异常
    - `medium abnormal` - 介质状态异常
    - `unit abnormal, medium abnormal` - 单元和介质均异常

### 警报日志（AlarmDispatchTask）
无

---

## NetworkStatusTask

### 功能
监控所有 ModbusTcpMaster 的网络连接状态，处理设备离线告警和恢复。

### 运行日志（OperationDispatchTask）

#### 设备离线告警解决日志
- **位置**: `onStatusChanged()` 方法
- **条件**: 设备从断开/连接中/错误状态变为已连接状态，且成功调用 `AlarmDispatchTask::submitResolve` 解决设备离线告警
- **级别**: `Message`
- **内容**: `[DeviceOffline] Device {masterId} connection restored, alarm resolved`
- **参数**: {masterId} - 设备二维码 ID

#### 设备离线告警提交日志
- **位置**: `onStatusChanged()` 方法
- **条件**: 设备从已连接状态跌落（变为断开/连接中/错误状态），且成功调用 `AlarmDispatchTask::submitAlarm` 提交设备离线告警
- **级别**: `Error`
- **内容**: `[DeviceOffline] Device {masterId} connection lost, alarm submitted`
- **参数**: {masterId} - 设备二维码 ID

#### WriteQRCode 指令下发日志
- **位置**: `submitWriteQRCode()` 方法
- **条件**: 设备连接成功后，成功下发 WriteQRCode 指令
- **级别**: `Message`
- **内容**: `[WriteQRCode] Device {masterId} → QRCode={qrcodeValue}`
- **参数**:
  - {masterId} - 设备二维码 ID
  - {qrcodeValue} - 写入的二维码数值

### 警报日志（AlarmDispatchTask）

#### 设备离线告警提交
- **位置**: `onStatusChanged()` 方法
- **条件**: 设备从已连接状态跌落（变为断开/连接中/错误状态）
- **告警类型**: `DeviceOffline`
- **告警来源**: `Device`
- **描述**: `Device {masterId} connection lost`

#### 设备离线告警解决
- **位置**: `onStatusChanged()` 方法
- **条件**: 设备从断开/连接中/错误状态变为已连接状态
- **告警类型**: `DeviceOffline`
- **告警来源**: `Device`

---
