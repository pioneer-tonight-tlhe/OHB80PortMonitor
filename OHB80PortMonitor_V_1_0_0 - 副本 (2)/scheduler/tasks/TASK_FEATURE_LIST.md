# Scheduler Tasks 功能清单

> 本文档列出 `scheduler/tasks/` 目录下所有调度任务的分类与功能说明。

---

## 一、常驻任务（Persistent Tasks）

常驻任务在系统运行期间持续存在，不会被销毁。

| # | 任务类 | 功能描述 |
|---|--------|---------|
| 1 | **AlarmDispatchTask** | 警报调度常驻任务。接收业务侧提交的警报/解除事件，按 alarmId 去重，持久化到 SQLite，并派发 `alarmPublished` / `alarmResolved` 信号供 UI 订阅。 |
| 2 | **MonitorDataTask** | 监控数据采集常驻任务。周期性向所有已注册设备下发监控指令，监听响应并发射 `commandFinished` 信号（含请求帧、响应帧、时间戳等），同时记录通讯日志。 |
| 3 | **NetworkStatusTask** | 网络连接状态监控常驻任务。监听所有 ModbusTcpMaster 的连接状态变化，启动自动重连，状态变化时根据 masterId 获取对应设备信息并设置告警。 |
| 4 | **OperationDispatchTask** | 操作调度常驻任务。接收业务侧提交的操作日志（Message / Warn / Error），直接落入 SQLite 操作日志表，并通过 `operationLogInserted` 信号通知 UI 实时显示。 |

---

## 二、日志查询任务（Log Query Tasks）

| # | 任务类 | 功能描述 |
|---|--------|---------|
| 5 | **AlarmLogQueryTask** | 警报日志查询任务。支持按页码、页大小、警报等级、QRCode、警报类型、解决状态、时间范围等条件分页查询警报日志。 |
| 6 | **CommunicateLogQueryTask** | 通讯日志查询任务。支持按页码、页大小、指令ID、QRCode、执行状态、重试次数、时间范围、排序方式等条件分页查询通讯日志。 |
| 7 | **OperationLogQueryTask** | 操作日志查询任务。支持按时间范围、日志类型、关键字搜索等条件分页查询，返回当前页记录、匹配ID集合、总记录数、总命中数、首条命中位置等。 |

---

## 三、设备配置任务（Device Configuration Tasks）

| # | 任务类 | 功能描述 |
|---|--------|---------|
| 8 | **SetFirmwareConfigTask** | 设置固件配置任务。支持设置 PrepareTimeout、WaitingTime、SendInterval、TransferTimeout、PostTransferWaitTime 等固件参数，仅被调用的参数才会应用到所有设备。 |
| 9 | **SetHumidityOffsetTask** | 设置湿度校准参数任务。支持下发湿度偏移阈值（WriteHumidityOffsetThreshold）和湿度偏移量（WriteHumidityOffset），寄存器值 = 百分比 × 100。 |
| 10 | **SetIdlePurgeTask** | 设置空闲吹扫参数任务。批量写入 Idle Purge 相关属性（如启用/禁用、间隔时间等）到目标设备。 |
| 11 | **SetPneumaticValvePressureTask** | 设置气控阀压力任务。向目标设备写入压力值（单位 bar），寄存器值 = pressureBar × 10000，5 秒整体超时。 |
| 12 | **SetPurgeFlowTask** | 设置吹扫流量任务。通过 WritePurgeFlow 指令（FC 0x06, addr 0x0000）设置 VEFC 流量大小，寄存器值 = flow × 100，仅 FOUP IN 时有效。 |
| 13 | **SetUIRefreshTimeTask** | 设置 UI 刷新时间任务。通过 WriteUIRefreshTime 指令（FC 0x10, addr 0x0004）设置下位机屏幕的 log 界面和属性页面显示时长（秒）。 |
| 14 | **SetVEFCGasTypeTask** | 设置 VEFC 气体类型任务。通过 WriteVEFCGasType 指令（FC 0x06, addr 0x0001）写入气体介质类型（CDA / N2 / Ar / CO2 / O2），掉电保持。 |

---

## 四、设备操作任务（Device Operation Tasks）

| # | 任务类 | 功能描述 |
|---|--------|---------|
| 15 | **FirmwareUpgradeTask** | 固件升级任务。对指定设备列表执行固件升级，支持单个设备进度反馈（0-100%），通过 BinFileReader 读取 .bin 文件并下发。 |
| 16 | **InitCheckTask** | 初始化检查任务。监听所有 ModbusTcpMaster 的初始化指令完成信号，汇总检查是否存在失败的初始化指令，完成后发射 `allFinished` 信号。 |
| 17 | **ReadVEFCFlowUnitAndMediumStatusTask** | 读取 VEFC 流量单位/介质配置状态任务。通过 ReadVEFCFlowUnitAndMediumStatus 指令（FC 0x04, addr 0x0011）读取每台设备的单位/介质配置成功/失败状态。 |
| 18 | **SendCommandTask** | 通用指令下发任务。支持向指定 QRCode 列表或所有已注册设备下发任意业务指令（按指令名称 + 参数），统一处理响应和超时。 |
| 19 | **SH85SelfCheckTask** | SH85 湿度传感器自检任务。作为 SH85SelfChecker（Data 层自检器）的薄包装层，负责启动自检、转发倒计时信号（70s → 0）、状态文本更新（"Checking (N)"），完成后返回成功/失败及具体 Result。 |
| 20 | **UserManagementTask** | 用户管理任务。支持 Login / Logout / ChangePassword / AddUser / RemoveUser 五种操作，通过 UserManager 执行并返回结果。 |

---

## 五、任务分类汇总

| 分类 | 任务数 | 包含任务 |
|------|--------|---------|
| 常驻任务 | 4 | AlarmDispatchTask, MonitorDataTask, NetworkStatusTask, OperationDispatchTask |
| 日志查询 | 3 | AlarmLogQueryTask, CommunicateLogQueryTask, OperationLogQueryTask |
| 设备配置 | 7 | SetFirmwareConfigTask, SetHumidityOffsetTask, SetIdlePurgeTask, SetPneumaticValvePressureTask, SetPurgeFlowTask, SetUIRefreshTimeTask, SetVEFCGasTypeTask |
| 设备操作 | 6 | FirmwareUpgradeTask, InitCheckTask, ReadVEFCFlowUnitAndMediumStatusTask, SendCommandTask, SH85SelfCheckTask, UserManagementTask |
| **合计** | **20** | |

