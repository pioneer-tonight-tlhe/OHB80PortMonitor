# Idle 任务调试文档（仅 Idle）

本文档只说明 Idle 相关日志，不包含 `system` 等非 Idle 日志。

Idle 子功能按执行顺序说明：
1. `WriteIdlePurgeEnable`（Idle Purge Enable）
2. `WriteIdlePurgeTime`（Purge Duration）
3. `WriteIdlePurgeInterval`（Purge Interval）

---

## 1. WriteIdlePurgeEnable（Idle Purge Enable）

### 1.1 运行日志（operation_log）写入内容
写入库表：`bin/x64/databases/logdb.db` -> `operation_log`

1. 任务开始：
```text
SetIdlePurge task started: Idle Purge Enable = enable|disable
```

2. 设备失败（每台失败设备一条）：
```text
[QRCode:<id>]: SetIdlePurge Idle Purge Enable=enable|disable task failed
```

3. 任务结束汇总：
- 全成功：
```text
SetIdlePurge Idle Purge Enable=enable|disable task completed: <N> devices succeeded
```
- 有失败：
```text
SetIdlePurge Idle Purge Enable=enable|disable task finished: <S> succeeded, <F> failed
```

### 1.2 调试日志（communicate_log）写入内容
写入库表：`bin/x64/databases/logdb.db` -> `communicate_log`

- `command_id = WriteIdlePurgeEnable`
- 每次下发一条通信记录，包含：
  - `send_time`
  - `response_time`
  - `qr_code`
  - `exec_status`（0=Success, 1=Timeout, 2=Retry, 3=Send Failed）
  - `retry_count`
  - `send_frame`
  - `response_frame`
  - `description`

常见失败示例：
```text
exec_status=1, retry_count=5, description=等待RTU响应超时
```

### 1.3 调试日志（文件）写入内容
`SetIdlePurgeTask` 会写 Idle 细节日志：

1. 跳过下发：
```text
[SetIdlePurgeTask][QRCode:xxxx] 跳过下发
指令: WriteIdlePurgeEnable
原因: ...
```

2. 指令完成（成功/失败都写，失败走 warn）：
```text
[QRCode:xxxx] [SetIdlePurgeTask] 指令下发完成
指令: WriteIdlePurgeEnable
请求帧: ...
响应帧: ...
```

理论子功能文件路径：
- `bin/x64/log/<YYYY-MM-DD>/scheduler/set_idle_purge_task/set_idle_purge_enable.log`
- 失败分流文件（warn）：`.../set_idle_purge_enable_warn.log`

如未看到独立子文件，直接在 `trace*.log` 检索 `SetIdlePurgeTask|WriteIdlePurgeEnable`。

---

## 2. WriteIdlePurgeTime（Purge Duration）

### 2.1 运行日志（operation_log）写入内容
写入格式与 Enable 相同，仅属性名变化为 `Purge Duration`：

1. 任务开始：
```text
SetIdlePurge task started: Purge Duration = <value> s
```

2. 设备失败：
```text
[QRCode:<id>]: SetIdlePurge Purge Duration=<value> s task failed
```

3. 任务结束：
- 全成功：
```text
SetIdlePurge Purge Duration=<value> s task completed: <N> devices succeeded
```
- 有失败：
```text
SetIdlePurge Purge Duration=<value> s task finished: <S> succeeded, <F> failed
```

### 2.2 调试日志（communicate_log）写入内容
- `command_id = WriteIdlePurgeTime`
- 字段与状态含义同 `WriteIdlePurgeEnable`。

### 2.3 调试日志（文件）写入内容
1. 跳过下发：
```text
[SetIdlePurgeTask][QRCode:xxxx] 跳过下发
指令: WriteIdlePurgeTime
原因: ...
```

2. 指令完成：
```text
[QRCode:xxxx] [SetIdlePurgeTask] 指令下发完成
指令: WriteIdlePurgeTime
请求帧: ...
响应帧: ...
```

理论子功能文件路径：
- `bin/x64/log/<YYYY-MM-DD>/scheduler/set_idle_purge_task/set_purge_duration.log`
- warn 分流：`.../set_purge_duration_warn.log`

如未看到独立子文件，直接在 `trace*.log` 检索 `SetIdlePurgeTask|WriteIdlePurgeTime`。

---

## 3. WriteIdlePurgeInterval（Purge Interval）

### 3.1 运行日志（operation_log）写入内容
写入格式与前两项一致，仅属性名变化为 `Purge Interval`：

1. 任务开始：
```text
SetIdlePurge task started: Purge Interval = <value> s
```

2. 设备失败：
```text
[QRCode:<id>]: SetIdlePurge Purge Interval=<value> s task failed
```

3. 任务结束：
- 全成功：
```text
SetIdlePurge Purge Interval=<value> s task completed: <N> devices succeeded
```
- 有失败：
```text
SetIdlePurge Purge Interval=<value> s task finished: <S> succeeded, <F> failed
```

### 3.2 调试日志（communicate_log）写入内容
- `command_id = WriteIdlePurgeInterval`
- 字段与状态含义同 `WriteIdlePurgeEnable`。

### 3.3 调试日志（文件）写入内容
1. 跳过下发：
```text
[SetIdlePurgeTask][QRCode:xxxx] 跳过下发
指令: WriteIdlePurgeInterval
原因: ...
```

2. 指令完成：
```text
[QRCode:xxxx] [SetIdlePurgeTask] 指令下发完成
指令: WriteIdlePurgeInterval
请求帧: ...
响应帧: ...
```

理论子功能文件路径：
- `bin/x64/log/<YYYY-MM-DD>/scheduler/set_idle_purge_task/set_purge_interval.log`
- warn 分流：`.../set_purge_interval_warn.log`

如未看到独立子文件，直接在 `trace*.log` 检索 `SetIdlePurgeTask|WriteIdlePurgeInterval`。

---

## 4. 按子功能查看日志的查询模板

### 4.1 运行日志（operation_log）
```powershell
sqlite3 "bin/x64/databases/logdb.db" "select id,occur_time,log_type,description from operation_log where description like '%SetIdlePurge%' order by id desc limit 200;"
```

### 4.2 通信调试日志（communicate_log）

1. Enable：
```powershell
sqlite3 "bin/x64/databases/logdb.db" "select id,send_time,response_time,command_id,qr_code,exec_status,retry_count,hex(send_frame),hex(response_frame),description from communicate_log where command_id='WriteIdlePurgeEnable' order by id desc limit 200;"
```

2. PurgeTime：
```powershell
sqlite3 "bin/x64/databases/logdb.db" "select id,send_time,response_time,command_id,qr_code,exec_status,retry_count,hex(send_frame),hex(response_frame),description from communicate_log where command_id='WriteIdlePurgeTime' order by id desc limit 200;"
```

3. PurgeInterval：
```powershell
sqlite3 "bin/x64/databases/logdb.db" "select id,send_time,response_time,command_id,qr_code,exec_status,retry_count,hex(send_frame),hex(response_frame),description from communicate_log where command_id='WriteIdlePurgeInterval' order by id desc limit 200;"
```

### 4.3 文件调试日志（trace 检索）

1. Enable：
```powershell
rg -n -g "trace*.log" "SetIdlePurgeTask|WriteIdlePurgeEnable" "bin/x64/log/<YYYY-MM-DD>"
```

2. PurgeTime：
```powershell
rg -n -g "trace*.log" "SetIdlePurgeTask|WriteIdlePurgeTime" "bin/x64/log/<YYYY-MM-DD>"
```

3. PurgeInterval：
```powershell
rg -n -g "trace*.log" "SetIdlePurgeTask|WriteIdlePurgeInterval" "bin/x64/log/<YYYY-MM-DD>"
```

