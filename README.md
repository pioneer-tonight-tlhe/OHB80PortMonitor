# OHB80PortMonitor
80port ohb 充氮设备监控上位机

**当前版本：待发布**

## 项目文档
详细的项目框架文档请参阅：[PROJECT_STRUCTURE.md](./OHB80PortMonitor_V_1_0_0/docs/PROJECT_STRUCTURE.md)

---

## 更新日志

## 待发布

### 湿度配置页迁移到 DebugPage
- 修改时间：2026-06-17
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：将原 ConfigPage 中的湿度偏移配置入口迁移到 DebugPage，统一归入调试类功能区，便于维护和调试。
- 功能点明细：
  - `ConfigPage` 移除 `HumidityOffsetSettingWidget` 及对应导航按钮。
  - `DebugPage` 新增 `HumidityOffsetSettingWidget` 及对应导航按钮。
  - 湿度配置的读写逻辑保持不变，仅调整页面入口位置。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/configpage.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/configpage.h`
  - `OHB80PortMonitor_V_1_0_0/ui/configpage.ui`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.h`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.ui`
  - `README.md`
- 兼容性影响：原湿度配置入口位置变更，功能逻辑与下发协议不变。
- 验证情况：已完成 64 位 Debug 构建验证，完整链接需关闭正在运行的 `OHB80PortMonitor_V_1_0_0.exe` 后执行。

### AlarmPage History QRCode 范围动态化
- 修改时间：2026-06-17
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：将 AlarmPage 的 History 查询页中 QRCode SpinBox 范围改为从共享内存读取全量设备二维码，并按数值最小值/最大值动态设置。
- 功能点明细：
  - 通过 `SharedData::getAllQrcodes()` 获取整机 QRCode 列表。
  - 过滤可转为数字的 QRCode，按数值最小值/最大值设置 `spinBoxQRCode` 范围。
  - 当共享数据中无有效 QRCode 时，回退到默认范围。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `README.md`
- 兼容性影响：History 查询 QRCode 输入范围将随共享设备配置自动变化。
- 验证情况：已完成 `debug/alarmlogwidget.o` 强制重编译验证。

## v0.5.7

### 调整通信日志权限过滤查询逻辑
- 修改时间：2026-06-16
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：将通信日志 `History Log` 的用户权限过滤从 UI 展示层下沉到 SQL 查询层，保证分页总数和当前页数据都只统计当前登录用户可见的记录。
- 功能点明细：
  - `ComunicateLogWidget` 创建历史查询任务时传入当前登录用户权限，不再在 `setHistoryLogData()` 中对 SQL 返回结果做二次过滤。
  - `CommunicateLogQueryTask` 新增 `maxUserPermission` 参数，并传递到分页查询和条件总数查询。
  - `CommunicateLogDBCon` / `CommunicateLogSqlLogic` 的通信日志条件查询接口增加 `maxUserPermission` 条件。
  - `communicate_log_queries.sql` 的 `query_page_with_conditions` 和 `query_total_count_with_conditions` 增加 `user_permission <= ?`，避免低权限用户分页总数包含高权限记录。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/communicatelogquerytask/communicatelogquerytask.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/communicatelogquerytask/communicatelogquerytask.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogsqllogic.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/communicate_log_queries.sql`
  - `README.md`
- 兼容性影响：低权限用户查询通信日志历史记录时，不再出现总页数/总记录数包含不可见高权限记录导致的空页或少行问题。
- 验证情况：已通过受影响的 `comunicatelogwidget`、`communicatelogquerytask`、`communicatelogdbcon`、`communicatelogsqllogic` 及对应 moc 目标文件编译；完整链接需关闭正在运行的 `OHB80PortMonitor_V_1_0_0.exe` 后执行。
- 备注：`communicate_log_queries.sql` 位于 `bin/x64`，当前受 `.gitignore` 忽略，发布包需要同步该运行时 SQL 文件。

## v0.5.6

### 调整运行日志权限过滤查询逻辑
- 修改时间：2026-06-15
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：将运行日志历史查询的用户权限过滤从 UI 层下沉到数据库查询层，保证分页总数、命中总数、表格显示和 Pre/Next 跳转使用同一批可见数据。
- 功能点明细：
  - `OperationLogQueryTask` 增加当前用户权限参数，并传递给分页、统计、命中定位和上下条查询。
  - `OperationLogDBCon` / `OperationLogSqlLogic` 的运行日志查询接口增加 `maxUserPermission` 条件。
  - `operation_log_queries.sql` 中运行日志分页、统计和命中查询增加 `user_permission <= ?` 条件。
  - `OperationLogWidget` 移除历史表 UI 二次权限过滤，登录权限变化后自动刷新实时日志和已有历史查询。
  - `History Log` 查询拆分为“基础条件分页展示”和“keyword 命中统计/导航”，修复模糊查询后 X/Y 分母为 0、Pre/Next 不可用以及首尾命中无法循环跳转的问题。
  - `History Log` 增加 `Record No.` SpinBox 和 `Jump` 按钮，可输入第 N 条 keyword 命中记录并自动切换到对应页面。
  - `operation_log_queries.sql` 新增 `query_matched_id_by_position`，按当前权限、时间范围、日志类型和 keyword 直接定位第 N 条命中记录，避免通过多次 Next 逐条跳转。
  - `Live Log` 实时显示增加稳定性约束：界面最多保留最近 2000 条可见日志，新日志按权限过滤后增量追加，超限批量裁剪旧记录，避免 UI 模型无限增长。
  - 按 Qt C++ 命名规范和头文件注释规范整理运行日志控件、查询任务和数据库连接层头文件。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.ui`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operationlogquerytask/operationlogquerytask.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operationlogquerytask/operationlogquerytask.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogsqllogic.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/operation_log_queries.sql`
  - `README.md`
- 兼容性影响：运行日志历史查询结果会随当前登录权限变化；低权限用户不再计入或跳转到高权限日志记录。
- 验证情况：已通过受影响的 `operationlogwidget`、`operationlogdbcon`、`operationlogsqllogic` 及对应 moc 目标文件编译；完整链接需关闭正在运行的 `OHB80PortMonitor_V_1_0_0.exe` 后执行。
- 备注：`operation_log_queries.sql` 位于 `bin/x64`，当前受 `.gitignore` 忽略，发布包需要同步该运行时 SQL 文件。

## v0.5.5

- 发布日期：2026-06-12

### DebugPage 新增 FOUP IN 真空阀保持调试项
- 修改时间：2026-06-12
- 变更类型：Added
- 开发人员：Simon（工号：13）
- 关联信息：`WriteFoupInVacuumExtractionEnable` / `ReadFoupInVacuumExtractionEnable`
- 功能概述：在 DebugPage 中新增 FOUP IN 真空阀保持模式调试项，可对单台设备读取和设置 `FoupInVacuumExtractionEnable`。
- 功能点明细：
  - 新增 `FoupInVacuumExtractionEnableWidget`，默认选中设备列表中的第一台设备二维码。
  - 读取时通过 `SendCommandTask` 下发 `ReadFoupInVacuumExtractionEnable` 指令，并将返回寄存器值同步到下拉选项。
  - 设置时通过 `SendCommandTask` 下发 `WriteFoupInVacuumExtractionEnable` 指令，写入值 `0` 表示默认模式，`1` 表示 FOUP IN 时保持 SW5 真空阀开启。
  - 新增 `All Devices` 批量操作项，可一次性对全部设备下发读取或设置指令，并通过表格弹窗展示每台设备的执行结果。
  - DebugPage 顶部新增 `FOUP Vacuum` 导航按钮，可快速定位到该调试项。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/foupinvacuumextractionenablewidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/foupinvacuumextractionenablewidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/debugsettingwidget.pri`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.h`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.ui`
  - `OHB80PortMonitor_V_1_0_0/bin/config/ModbusTcpMasterConfig.xml`
  - `README.md`
- 兼容性影响：不改变现有调试项和设备通讯流程；新增调试项依赖配置文件中已存在的两条 Modbus 指令。
- 备注：该功能仅提供调试页面手动读写入口，不自动改变设备参数。

---

## v0.5.4

- 发布日期：2026-06-12

### 磁盘高水位清理日志路径调整
- 修改时间：2026-06-12
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 关联信息：`DiskPressureCleanupTask`
- 功能概述：将磁盘高水位清理日志从 `log_db` 分类调整到 scheduler 调度任务分类。
- 功能点明细：
  - `DiskPressureCleanupTask` 的统一日志路径调整为 `scheduler/disk_pressure_cleanup_task/clean`。
  - 路径归属与任务位置保持一致，便于区分数据库月度清理日志和调度层磁盘压力清理日志。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/disk_pressure_cleanup_task/disk_pressure_cleanup_task.cpp`
  - `README.md`
- 兼容性影响：后续排查磁盘高水位清理记录时，需要查看新的 scheduler 日志目录。
- 备注：仅调整日志输出路径，不改变磁盘使用率阈值、清理顺序和保留策略。

### 磁盘高水位清理迁移到调度层

- 修改时间：2026-06-11
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 关联信息：N/A
- 功能概述：移除 `communicatelogdb` 逻辑层中磁盘使用率 `>= 90%` 时的额外清理机制，改由调度层统一负责磁盘高水位清理。
- 功能点明细：
  - 新增 `DiskPressureCleanupTask` 常驻调度任务，每 `60000ms` 检查一次数据库所在磁盘使用率。
  - 当磁盘使用率 `< 90%` 时不写入清理日志，避免每分钟产生无意义日志。
  - 当磁盘使用率 `>= 90%` 时，先清理 LoggerManager 日期日志目录，仅保留最近 2 个月（包含今天所在月份）的日志。
  - LoggerManager 清理后仍未低于阈值时，按优先级提交数据库清理：`communicatelogdb`、`vefcsensormonitordb`、`operationlogdb`、`alarmlogdb`。
  - 高水位数据库清理每次提交最早 1 个月删除请求，并保护每个数据库至少保留最近 6 个月日志。
  - 高水位清理统一记录到 `scheduler/disk_pressure_cleanup_task/clean`。
  - `communicatelogdb` 不再写入原磁盘阈值清理日志 `log_db/communicate_log_db/clean`。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/disk_pressure_cleanup_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/disk_pressure_cleanup_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/scheduler.pri`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.h`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.cpp`
  - `OHB80PortMonitor_V_1_0_0/tool/loggermananger/loggermanager.h`
  - `OHB80PortMonitor_V_1_0_0/tool/loggermananger/loggermanager.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogsqllogic.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitordbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitordbcon.cpp`
  - `README.md`
- 兼容性影响：常规月度清理策略不变；磁盘高水位清理由单库内部机制改为调度层统一机制，清理日志查看路径发生变化。
- 备注：本版本将通信日志磁盘阈值清理迁移到调度层统一处理。

---

## v0.5.3

- 发布日期：2026-06-11

### 常规日志数据库清理策略统一

- 修改时间：2026-06-11
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：统一常规日志数据库的月度清理策略，并将清理检查调整为每天凌晨执行一次。
- 功能点明细：
  - `operationlogdb`、`alarmlogdb`、`deviceparamlogdb`、`communicatelogdb` 统一调整为保留最近 6 个月数据。
  - 四个常规日志库每次触发清理时删除最早 1 个月的数据。
  - 通用 `LogCleanupScheduler` 调整为每分钟轮询时间，但仅在每天 `00:05` 所在凌晨小时内最多执行一次数据库清理检查。
  - 清理任务提交结果由删除回调返回，统一日志中明确记录“已提交删除任务”或“删除任务提交失败”。
  - `vefcsensormonitordb` 的保留策略不变，仍保留最近 1 个月采集数据。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/logcleanupscheduler.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/logcleanupscheduler.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitorsqllogic.cpp`
  - `README.md`
- 兼容性影响：常规日志数据库历史数据保留周期由 7 个月缩短为 6 个月；VEFC 采集数据库保留周期不变。
- 备注：本版本发布时通信日志数据库仍保留原磁盘阈值清理；该机制已在“待发布”记录中迁移到调度层统一处理。

### 常规日志数据库统一清理日志

- 修改时间：2026-06-11
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：四个常规日志数据库的月度清理记录统一写入同一个清理日志文件，便于集中查看清理触发与提交结果。
- 功能点明细：
  - `operationlogdb`、`alarmlogdb`、`deviceparamlogdb`、`communicatelogdb` 的月度清理日志统一写入 `log_db/database_cleanup/month_clean`。
  - 日志块统一记录数据库名称、表名、保留策略、每次清理月份、数据覆盖月份、最早记录、最新记录、清理范围和执行结果。
  - 未超过保留范围时，每天凌晨检查只记录一次“未触发清理”，避免每分钟刷屏。
  - 触发清理时记录“数据库定时清理触发”和“数据库定时清理提交完成/失败”两个阶段。
  - `vefcsensormonitordb` 仍使用独立日志路径 `log_db/vefc_sensor_monitor_db/month_clean`。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/logcleanupscheduler.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/logcleanupscheduler.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogsqllogic.cpp`
  - `README.md`
- 兼容性影响：常规日志数据库的月度清理记录位置发生变化，后续排查常规日志库清理情况需查看 `log_db/database_cleanup/month_clean`。
- 备注：本版本统一日志仅覆盖月度保留清理；通信日志磁盘阈值清理已在“待发布”记录中迁移到 `scheduler/disk_pressure_cleanup_task/clean`。

---

## v0.5.2

- 发布日期：2026-06-11

### VEFCSensorMonitor 采集数据库定时清理

- 修改时间：2026-06-11
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：在 VEFC 采集数据库逻辑层新增定时清理能力，仅保留最近 1 个月的采样数据。
- 功能点明细：
  - 在 `VEFCSensorMonitorSqlLogic` 初始化数据库连接后自动启动清理调度器。
  - 清理范围只针对 `vefc_sensor_monitor` 采集表，不影响其他日志库或业务表。
  - 调度器每 `60000ms` 检查一次数据库时间跨度，超过保留范围时删除最早 1 个月数据。
  - 复用通用 `LogCleanupScheduler`，并通过 `query_month_range` 查询当前采集库的最早、最晚记录时间。
  - 由于 VEFC 采集表使用毫秒时间戳存储，清理逻辑已适配字符串时间范围到时间戳删除区间的转换。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitorsqllogic.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitorsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/vefc_sensor_monitor_queries.sql`
  - `README.md`
- 兼容性影响：不修改 VEFC 采集表结构和现有采样写入流程；仅新增逻辑层自动清理行为，运行后历史采样数据将最多保留 1 个月。
- 备注：本次清理策略仅作用于 VEFC 采集数据库，不扩展到其他日志数据库。

### VEFCSensorMonitor 采集数据库代码规范整理

- 修改时间：2026-06-11
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：按 `Qt C++ 头文件注释规范.md` 和 `Qt C++ 命名规范.md` 整理 VEFC 采集数据库连接层与逻辑层代码。
- 功能点明细：
  - 为 `VEFCSensorMonitorDBCon` 和 `VEFCSensorMonitorSqlLogic` 头文件补充标准文件头注释、`@class`、`@brief` 和设计目标。
  - 按规范重新整理类内分区，明确构造析构、生命周期、写入查询、信号槽和成员变量区域。
  - 为关键成员变量补充逐项中文职责注释，便于后续维护时快速理解线程、连接和清理调度器边界。
  - 将局部配置变量命名从 `cfg` 调整为更清晰的 `cleanupConfig`，提升代码可读性。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitordbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitorsqllogic.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitorsqllogic.cpp`
  - `README.md`
- 兼容性影响：仅涉及注释、分区和命名可读性整理，不改变对外接口和运行行为。
- 备注：本次规范化范围聚焦 VEFC 采集数据库连接层相关文件，后续可按同一规范继续覆盖其他模块。

---

## v0.5.1

- 发布日期：2026-06-11

### VEFCSensorMonitor 寿命日统计日志

- 修改时间：2026-06-11
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：调整 VEFC 传感器寿命日统计日志输出格式与日志分工，方便在 `summary.log` 中直接对比本次开机首条记录与当天统计结果。
- 功能点明细：
  - 每天 `00:05` 后自动统计前一天数据，同一次程序运行中同一统计日期只写入一次。
  - 所有设备统计写入 `scheduler/vefc_sensor_monitor_task/summary.log`，并通过“设备统计开始 / 设备统计结束”区分设备边界。
  - 设备输出顺序按 `QRCode` 转数字后升序排列，便于和现场设备编号对应。
  - 统计字段使用中文名称，当前固定输出：`采样总数`、`软件第一次打开记录`、`VEFC压力平均值`、`VEFC温度平均值`、`今日第一次记录`、`今日最后一次记录`。
  - `软件第一次打开记录` 定义为“本次软件启动后，该设备第一次成功采样到的记录”，作为日报对比项同步打印。
  - 无数据设备也会写入统计块；没有对应记录时字段显示为 `N/A`。
  - `devices.log` 仅保留设备指令通讯日志，例如 `command retrying`、`command finished`、`requestFrame`、`responseFrame`，不再记录统计结果。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_daily_stats.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_daily_stats.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.cpp`
  - `README.md`
- 兼容性影响：不改变 VEFC 原始采样写入逻辑和数据库表结构；仅调整 VEFC 统计日志输出位置与内容编排。
- 备注：本次为日志格式与日志职责调整版本，不涉及新增统计口径。

---

## v0.5.0

- 发布日期：2026-06-09

### VEFCSensorMonitor 寿命日统计日志

- 修改时间：2026-06-09
- 变更类型：Added
- 开发人员：Simon（工号：13）
- 功能概述：新增 VEFC 传感器寿命日统计日志能力，通过数据库原始采样记录生成每天每台设备的寿命观察指标。
- 功能点明细：
  - 新增 `VEFCSensorMonitorDailyStatsService` 与 `DailyStatsCalculator`，负责统计计算和日志格式化。
  - 每天 `00:05` 后自动统计前一天数据，同一次程序运行中同一统计日期只写入一次。
  - 所有设备统计写入同一个 `daily_stats.log` 文件，并通过“设备统计开始 / 设备统计结束”区分设备边界。
  - 设备输出顺序按 `QRCode` 转数字后升序排列，便于和现场设备编号对应。
  - 统计字段使用中文名称，包括 VEFC 压力平均值、VEFC 压力标准差、VEFC 温度平均值、VEFC 温度最大值、实际流量平均值、实际流量标准差、流量气压比平均值。
  - 每台设备额外记录“今日第一次记录”和“今日最后一次记录”，用于观察一天内气体气压、实际流量、VEFC 压力、VEFC 温度的变化。
  - 无数据设备也会写入日志，统计值显示为 `N/A`，避免误判为漏统计。
  - 为 `VEFCSensorMonitorDBCon` 增加按时间范围查询原始记录的接口，供日统计模块读取数据库数据。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_daily_stats.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_daily_stats.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitordbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitordbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitorsqllogic.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/vefcsensormonitordb/vefcsensormonitorsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/vefc_sensor_monitor_queries.sql`
  - `OHB80PortMonitor_V_1_0_0/scheduler/scheduler.pri`
  - `README.md`
- 兼容性影响：不改变 VEFC 原始采样写入逻辑和数据库表结构；新增日志文件和只读查询接口。运行时 SQL 文件位于 ignored 的 `bin/x64/databases` 目录，提交和部署时需要单独确认同步。
- 备注：本版本暂不统计“异常采样占比”，后续如果要做异常率，需要先定义明确的阈值规则。

---

## v0.4.0

- 发布日期：2026-06-09

### VEFCSensorMonitor 监控业务框架

- 修改时间：2026-06-09
- 变更类型：Added
- 开发人员：Simon（工号：13）
- 功能概述：新增 `VEFCSensorMonitorTask` 监控业务框架，用于采集 80 台设备的 VEFC 压力、VEFC 温度、气体气压和实际流量。
- 功能点明细：
  - 每台设备通过 `ReadVEFCPressure` 和 `ReadVEFCTemperature` 两条业务指令读取传感器数据。
  - 从 `FoupOfOHBInfo` 读取对应设备的进气压力和进气流量，组合生成 `VEFCSensorMonitorRecord`。
  - 当同一设备的两条指令都成功返回后，立即写入数据库，不再等待整轮设备全部完成后再批量落库。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.cpp`
  - `README.md`
- 兼容性影响：变更了 `VEFCSensorMonitorTask` 的落库时机，由整轮批量写入改为单设备即时写入；数据库表结构不变。
- 备注：当前保留原有轮次汇总日志，用于继续观察每轮成功、失败和跳过设备情况。

### VEFCSensorMonitorTask 结构重构

- 修改时间：2026-06-09
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：参考 `SH85PeriodicSelfCheckTask3` 的头文件模板和职责拆分方式，重构 `VEFCSensorMonitorTask` 的类结构。
- 功能点明细：
  - 将公共类型定义拆分到 `VEFCSensorMonitorTypes`。
  - 将设备筛选职责拆分到 `VEFCSensorMonitorDeviceSelector`。
  - 将轮次上下文和汇总职责拆分到 `VEFCSensorMonitorRoundContext`。
  - 将日志输出职责拆分到 `VEFCSensorMonitorLogService`。
  - 将 sender 连接、指令提交和 pending command 跟踪拆分到 `VEFCSensorMonitorRoundRunner`。
  - 将 `VEFCSensorMonitorTask` 本体收敛为调度壳，仅保留周期触发、状态切换、信号转发和轮次收口。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_types.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_device_selector.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_device_selector.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_round_context.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_round_context.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_log_service.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_log_service.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_round_runner.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_round_runner.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/scheduler.pri`
  - `README.md`
- 兼容性影响：
  - 不改变 `VEFCSensorMonitorTask` 的任务类型。
  - 不改变周期执行方式。
  - 不改变“单设备两条指令成功后立即落库”的对外语义。
  - 变化主要在内部结构分层。
- 备注：本次调整的目标是与 `SH85PeriodicSelfCheckTask3` 保持一致的设计风格，便于后续扩展更多 VEFC 监控子能力。

---

## v0.3.1

- 发布日期：2026-06-09

### VEFCSensorMonitor 数据库脚本恢复

- 修改时间：2026-06-09
- 变更类型：Fixed
- 开发人员：Simon（工号：13）
- 功能概述：恢复 `VEFCSensorMonitorDBCon` 所需的运行时 SQL 脚本，并补回缺失的数据表创建语句。
- 功能点明细：
  - 恢复 `vefc_sensor_monitor_queries.sql`，补齐 `insert_record`、按时间删除、日报查询和最旧周数据查询脚本。
  - 在 `create_operation_log.sql` 中补回 `vefc_sensor_monitor` 建表语句与 `record_timestamp` 索引，确保数据库初始化时可自动创建对应数据表。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/vefc_sensor_monitor_queries.sql`
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/create_operation_log.sql`
  - `README.md`
- 兼容性影响：下次启动数据库初始化流程时会自动创建缺失的 `vefc_sensor_monitor` 表；现有表结构无需手工迁移。
- 备注：如果同时使用 `x32` 运行目录，需要同步补齐 `bin/x32/databases` 下的同名 SQL 文件。

---

## v0.3.0

- 发布日期：2026-06-09

### AlarmPage Live Log 排序

- 修改时间：2026-06-09
- 变更类型：Added
- 开发人员：Simon（工号：13）
- 功能概述：为 `AlarmPage` 的 `Live Log` 增加按设备 `QRCode` 的稳定排序显示。
- 功能点明细：
  - `Live Log` 按数字型 `QRCode` 升序排列，适配 `12001` 到 `12080` 的设备编号范围。
  - 当 `QRCode` 相同时，按记录到达顺序排序，保证同一设备的旧记录在前、新记录在后。
  - 保持超过 `100` 条后的清理规则不变，仅清理 `Resolved` 和 `No Need`，不删除 `Unresolved` 记录。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `README.md`
- 兼容性影响：仅调整 `Live Log` 展示顺序，不影响数据库记录内容和告警处理逻辑。
- 备注：该排序逻辑默认依赖 `QRCode` 为数字字符串。

---

## v0.2.3

- 发布日期：2026-06-08

### AlarmPage Live Log 清理修复

- 修改时间：2026-06-08
- 变更类型：Fixed
- 开发人员：Simon（工号：13）
- 功能概述：修复 `AlarmPage` 的 `Live Log` 在连接恢复后未触发清理的问题。
- 功能点明细：
  - 修复 `Device Offline` 告警恢复后只更新为 `Resolved`、但不触发清理的异常。
  - 当 `Live Log` 超过 `100` 条时，在新告警插入和告警恢复两个时机都会尝试清理 `Resolved` 和 `No Need` 记录。
  - 明确保证清理逻辑不会删除 `Unresolved` 未解决告警。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `README.md`
- 兼容性影响：仅修复界面层清理时机，不影响数据库中的历史告警记录。
- 备注：该修复属于 bug 修复，目的是避免已恢复的绿色记录长期占用 `Live Log` 明细。

---

## v0.2.2

- 发布日期：2026-06-08

### AlarmPage 告警样式调整

- 修改时间：2026-06-08
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：统一调整 `AlarmPage` 中 `Live Log` 和 `History Log` 的状态色与交互表现。
- 功能点明细：
  - 未解决告警 `Unresolved` 的背景色调整为 `#DC143C`。
  - 已解决告警 `Resolved` 的背景色调整为 `#32CD32`。
  - `Unresolved` 和 `Resolved` 状态文字统一使用白色字体，提升可读性。
  - `NoNeed` 状态保持原有黄色背景不变。
  - `Live Log` 和 `History Log` 表格点击后不进入选中状态。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `README.md`
- 兼容性影响：仅涉及界面样式与交互调整，不影响告警数据本身。
- 备注：本次调整重点是提升报警状态在大屏和明细表中的识别度。

---

## v0.2.1

- 发布日期：2026-06-08

### 警报屏蔽逻辑集成

- 修改时间：2026-06-08
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：在 `AlarmDispatchTask` 中集成警报屏蔽逻辑，使被屏蔽告警不再驱动 FOUP 报警显示。
- 功能点明细：
  - 在 `AlarmDispatchTask` 中接入 `AlarmConfig`，判断告警类型是否处于屏蔽状态。
  - 被屏蔽告警不会参与 `FoupOfOHBInfo::hasAlarm` 和 `alarmId` 的选择，不再驱动 FOUP 显示为报警状态。
  - 被屏蔽告警仍然正常落库，并继续参与活跃告警跟踪与去重。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.cpp`
  - `README.md`
- 兼容性影响：会改变 FOUP 报警灯的显示来源，但不会影响告警日志入库和活跃告警集合。
- 备注：该版本的屏蔽含义是“屏蔽 FOUP 报警展示”，不是“丢弃告警记录”。

---

## v0.2.0

- 发布日期：2026-06-08

### 警报屏蔽配置

- 修改时间：2026-06-08
- 变更类型：Added
- 开发人员：Simon（工号：13）
- 功能概述：新增告警屏蔽配置能力，支持通过配置文件管理可屏蔽的警报类型。
- 功能点明细：
  - 新增 `AlarmConfig` 类，用于读取、写入和查询警报屏蔽配置。
  - 新增 `alarm.ini` 配置文件，支持配置如 `VEFCAbnormal`、`VEEPAbnormal` 等屏蔽项。
  - 提供读取被屏蔽告警集合、设置屏蔽状态、检查是否被屏蔽等接口。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/bin/config/alarm.ini`
  - `OHB80PortMonitor_V_1_0_0/config/alarmconfig.h`
  - `OHB80PortMonitor_V_1_0_0/config/alarmconfig.cpp`
  - `OHB80PortMonitor_V_1_0_0/config/config.pri`
  - `README.md`
- 兼容性影响：默认配置下不会屏蔽任何告警；启用配置后才会影响对应告警的展示行为。
- 备注：`AlarmConfig` 已纳入工程编译配置，避免运行时调用存在但链接缺失的问题。

---

## v0.1.1

- 发布日期：2026-06-08

### SH85 自检报告 UI 优化

- 修改时间：2026-06-08
- 变更类型：Changed
- 开发人员：Simon（工号：13）
- 功能概述：优化 `SH85SelfCheckReportDialog` 的明细表显示和自检状态文案。
- 功能点明细：
  - `Live Log` 和 `History Log` 表格设置为不可选中，避免点击后出现高亮选中态。
  - `History Log` 的 `Description` 字段根据自检结果设置背景色，成功为 `#32CD32`，失败为 `#CD143C`。
  - `SH85SelfChecker` 中的自检结果文案统一调整为英文显示。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85selfcheckreportdialog.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/sh85selfchecker.cpp`
  - `README.md`
- 兼容性影响：仅调整自检报告界面展示与文字，不影响自检流程和数据库内容。
- 备注：该版本主要提升自检报告在值班场景下的可读性。

---

## v0.1.0

- 发布日期：2026-06-08

### SH85 周期自检任务重构

- 修改时间：2026-06-08
- 变更类型：Added
- 开发人员：Simon（工号：13）
- 功能概述：完成 `SH85PeriodicSelfCheckTask3` 重构，建立更清晰的 SH85 周期自检调度框架。
- 功能点明细：
  - 新增 `SH85PeriodicSelfCheckTask3`，替代原 `SH85PeriodicSelfCheckTask`，保持外部接口和 UI 信号风格一致。
  - 将轮次上下文、设备筛选、日志服务和轮次执行拆分为独立模块，降低调度类复杂度。
  - 支持周期调度、单设备模式、启动延时、轮次计数控制、设备筛选、日志聚合与轮次汇总信号。
  - `SH85PeriodicSelfCheckSettingWidget` 更新为接入新的 Task3 实现。
- 改动文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task3.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task3.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_round_context.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_round_context.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_device_selector.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_device_selector.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_log_service.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_log_service.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_round_runner.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_round_runner.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_log_helper.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85selfchecktask/sh85_self_check_log_helper.cpp`
  - `README.md`
- 兼容性影响：自检调度实现整体升级，旧的 Task2 与日志辅助类被替换；对外功能保持兼容，但内部结构发生较大变化。
- 备注：该版本是后续自检扩展和报告能力拆分的基础版本。

---

### 2026-06-02 - Simon

#### SH85 周期自检：配置化与 UI 对接
- 配置文件新增段落（ohb_device.ini）：
  ```ini
  [sh85selfchecktask]
  enabled=true           ; 是否启用周期自检
  period_s=1800          ; 周期秒数（默认 1800 秒 = 30 分钟）
  ```
- 启动应用配置：`SharedData::initScheduler()` 在提交 `SH85PeriodicSelfCheckTask` 后，读取上述配置并应用：
  - `setPeriod(period_s, TimeUnit::Second)`
  - `setEnabled(enabled)`
- 配置读写接口：`app/ohbdeviceconfig.{h,cpp}`
  - 读取：`readSH85SelfCheckEnabled()`、`readSH85SelfCheckPeriodSeconds()`（默认与非法值回退均为 1800）
  - 写入：`setSH85SelfCheckEnabled(bool)`、`setSH85SelfCheckPeriodSeconds(int seconds)`
- 设置页 UI 对接：`ui/customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.cpp`
  - 绑定时从配置读取秒数并“智能选择最大单位”显示：
    - 能被 3600 整除 → 显示为 hour（值=period_s/3600）
    - 否则能被 60 整除 → 显示为 min（值=period_s/60）
    - 否则用 s（值=period_s）
  - 用户点击 Set：将值与单位换算为秒，写回 ini，并通过 `setPeriod(value, unit)` 实时生效
  - 启用开关：调用 `setEnabled()` 同步到任务，同时写回 `enabled` 到 ini
  - SpinBox 范围：`0..999999`，单位下拉 `s|min|hour`

#### NetworkStatusTask 重构与日志规范
- 重构 `start()` 为更清晰的私有方法：`startInitCheckTask()`、`startAutoReconnect()`、`processInitialStatusAndConnectSignals()`；并补充方法/成员中文注释。
- 新增二维码专用日志类 `QRCodeWriteLogger`：
  - 位置：`scheduler/tasks/network_status_task/network_status_task_qrcode_logger.{h,cpp}`
  - 能力：对 WriteQRCode 指令成功/失败输出多行结构化日志（含 TX/RX 原始帧、结果/诊断、时间戳、重试次数）。
  - 日志路径：`scheduler/network_status_task/qrcode/devices.log`
- 集成与精简：
  - `onWriteQRCodeFinished()` 中以 `QRCodeWriteLogger` 统一多行日志输出，`NetworkStatusTaskLogger` 的设备日志不再重复详细二维码结果，仅保留“开始/完成”等简要信息。
  - 继续使用通用 `NetworkStatusTaskLogger` 写入 summary/devices 常规日志。
- 连接状态变更日志（`onStatusChanged`）统一为：
  - 新状态为“已连接”时：INFO 级别，消息仅为“上一次状态->当前状态”（例：`已断开->已连接`）。
  - 新状态非“已连接”时：WARN 级别，消息仅为“上一次状态->当前状态”（例：`连接中->错误`）。
  - 去除额外 IP/提示性文字；抑制重复离线/连接中产生的噪声日志；仅在 Disconnected/Error 时提交离线告警。
- 构建集成：`scheduler/scheduler.pri` 已加入二维码专用日志类文件。
- 相关文件：
  - 修改：`OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task/network_status_task.{h,cpp}`
  - 新增：`OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task/network_status_task_qrcode_logger.{h,cpp}`
  - 修改：`OHB80PortMonitor_V_1_0_0/scheduler/scheduler.pri`

#### MonitorDataTask 设备日志降噪
- 移除 FOUP 在位状态变化的 INFO 日志，避免 devices.log 产生噪声。
  - 位置：`scheduler/tasks/monitor_data_task/monitor_data_task.cpp::updateFoupPresenceState`
  - 行为：仅维护内存状态（开始时间/计时清零），不再输出“FOUP 状态变化”日志
  - 影响：`scheduler/monitor_data_task/devices.log` 更聚焦于指令帧与解析/失败信息

#### PeriodicCommandSender 失败统一转发 commandCompleted
- 在一轮结束的 `onRoundComplete(failedCommands)` 中，逐条 `emit commandCompleted(cmd, masterId)` 转发失败指令，便于上层统一处理成功与失败。
  - 位置：`data/modbustcpmastermanager/modbustcpmaster/periodiccommandsender.cpp`
  - 之前：仅成功即时转发；失败仅在 roundFinished 中汇总，未逐条上报给上层
  - 现在：失败将在本轮结束时逐条转发，上层如 `MonitorDataTask::onCommandCompleted` 能记录失败日志（含 ReadFoupStatus 超时等）

#### AlarmDispatchTask Summary 周期输出
- 新增 `scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.{h,cpp}` 周期汇总输出功能：
  - 成员：`QTimer* m_summaryTimer`
  - 方法：[initSummaryTimer()](cci:1://file:///d:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h:16:4-16:28)、[startSummaryTimer()](cci:1://file:///d:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h:17:4-18:29)、[stopSummaryTimer()](cci:1://file:///d:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h:19:4-19:28)、[writeSummarySnapshot()](cci:1://file:///d:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h:12:4-13:32)
  - 配置：`[scheduler] alarm_dispatch_task.summary_period_ms`（默认 60000ms）
  - 行为：每周期（可配置）输出一次"当前未恢复错误汇总"，仅当存在未恢复告警时输出
  - 格式：`[timestamp] 当前未恢复错误汇总` + 按错误类型分组的设备 QRCode 列表（全部显示，不截断）
  - 级别：info
  - 日志路径：`scheduler/alarm_dispatch_task/summary.log`
- 优化：注释掉原有逐条告警/恢复的 summary 日志输出，保留设备维度日志
- 相关文件：
  - 修改：[OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h](cci:7://file:///d:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h:0:0-0:0)
  - 修改：[OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.cpp](cci:7://file:///d:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.cpp:0:0-0:0)
  - 配置：[bin/config/logger_config.ini](cci:7://file:///d:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/bin/config/logger_config.ini:0:0-0:0) 新增 `alarm_dispatch_task.summary_period_ms=60000`

#### LoggerConfig 单例与配置文件
- 新增 `config/loggerconfig.{h,cpp}`，基于项目 `Singleton` 模板实现线程安全单例，提供读取/写入 INI 的封装。
- 更新并使用 `bin/config/logger_config.ini`：
  - `[scheduler] monitor_data_task.summary` / `monitor_data_task.devices`
  - `[scheduler] alarm_dispatch_task.summary` / `alarm_dispatch_task.devices`
  - `[scheduler] vefc_sensor_monitor_task.summary` / `vefc_sensor_monitor_task.devices`

#### MonitorDataTask 日志修复
- `m_logger` 为指针类型，统一将成员访问由 `.` 修正为 `->`，消除编译错误并避免潜在崩溃。
- 涉及文件：`OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task/monitor_data_task.cpp`

#### Modbus 指令新增（VEFC 传感器）
- 在 `bin/config/ModbusTcpMasterConfig.xml` 的指令定义中新增：
  - `ReadVEFCPressure`（FC 04，寄存器 `0x0016`，单寄存器，值解析：`(256*hi+lo)/10` KPa）
  - `ReadVEFCTemperature`（FC 04，寄存器 `0x0017`，单寄存器，值解析：`(256*hi+lo)/100` ℃）

#### VEFC 监控类与统计能力
- 新增 `classes/vefcmonitorinfo.{h,cpp}`：
  - `VEFCData{gasPressure, actualFlow, sensorPressure, sensorTemperature, timestamp}`
  - 统计接口：最小/最大/平均/中位数/标准差；分时段（逐小时）与固定窗口平均值。
  - 重置接口精简为 `resetAll()`。
- `FoupOfOHBInfo` 集成 `VEFCMonitorInfo*` 指针及访问器：
  - 成员：`m_vefcMonitorInfo`，方法：`vefcMonitorInfo()` / `setVEFCMonitorInfo(...)`。
- 将“第一次开机值”语义调整为“今天第一条数据”：
  - 成员由 `m_bootData` 更名为 `m_dailyFirstData`；对应 getter/setter：`getDailyFirstData()` / `setDailyFirstData()`。
- 相关文件：
  - 新增：`OHB80PortMonitor_V_1_0_0/classes/vefcmonitorinfo.h`
  - 新增：`OHB80PortMonitor_V_1_0_0/classes/vefcmonitorinfo.cpp`
  - 修改：`OHB80PortMonitor_V_1_0_0/classes/foupofohbinfo.h`
  - 修改：`OHB80PortMonitor_V_1_0_0/classes/foupofohbinfo.cpp`
  - 声明：`OHB80PortMonitor_V_1_0_0/app/shareddata.h` 新增 `getVEFCMonitorInfo(const QString&)` 接口声明

#### 影响范围
- 配置：`bin/config/logger_config.ini`
- 日志：`config/loggerconfig.{h,cpp}`、`scheduler/tasks/monitor_data_task/monitor_data_task.cpp`
- 指令：`bin/config/ModbusTcpMasterConfig.xml`
- 数据结构：`classes/vefcmonitorinfo.{h,cpp}`、`classes/foupofohbinfo.{h,cpp}`、`app/shareddata.h`
---

## Scheduler 任务模块与专属日志接口

为便于维护与阅读，调度任务按子目录划分模块，并为每个任务提供专属日志接口与独立日志目录/开关。

### 模块总览（目录）
- `OHB80PortMonitor_V_1_0_0/scheduler/tasks/`（根目录）
  - `vefc_sensor_monitor_task/`（VEFC 传感器监控与落库）
  - `sh85selfchecktask/`（SH85 周期自检）
  - `network_status_task/`（网络状态监控）
  - `monitor_data_task/`（设备实时监控数据采集）
  - `alarm_dispatch_task/`（告警分发调度）

### 日志接口与目录约定
- 每个任务模块对应一个专属 Logger 封装（约定命名：`<TaskName>Logger`）。
- 日志输出目录结构：`scheduler/<task_dir>/{summary | <QRCode>}`。
  - `summary`：任务生命周期与汇总统计
  - `<QRCode>`：按设备维度记录明细（当该任务涉及设备维度）
- 配置开关（`bin/config/logger_config.ini`）：
  - 已存在：
    - `scheduler.monitor_data_task.summary` / `scheduler.monitor_data_task.devices`
    - `scheduler.alarm_dispatch_task.summary` / `scheduler.alarm_dispatch_task.devices`
    - `scheduler.vefc_sensor_monitor_task.summary` / `scheduler.vefc_sensor_monitor_task.devices`
  - 建议后续补充（如需）：
    - `scheduler.network_status_task.summary` / `scheduler.network_status_task.devices`
    - `scheduler.sh85selfchecktask.summary` / `scheduler.sh85selfchecktask.devices`

### 各模块说明（要点）
- **monitor_data_task**
  - 目标：周期采集设备数据、解析并维护 FOUP 状态、上报告警
  - 日志：`scheduler/monitor_data_task/summary`、`scheduler/monitor_data_task/<QRCode>`
  - 开关：`monitor_data_task.summary` / `monitor_data_task.devices`

- **alarm_dispatch_task**
  - 目标：统一接收/分发告警发生与恢复事件，负责持久化
  - 建议日志：`scheduler/alarm_dispatch_task/summary`、`scheduler/alarm_dispatch_task/<QRCode>`
  - 开关：`alarm_dispatch_task.summary` / `alarm_dispatch_task.devices`

- **network_status_task**
  - 目标：监控设备网络连接与状态变更
  - 建议日志：`scheduler/network_status_task/summary`、`scheduler/network_status_task/<QRCode>`
  - 开关（可按需在 INI 中补充）：`network_status_task.summary` / `network_status_task.devices`
  - 专用二维码日志：`scheduler/network_status_task/qrcode/devices.log`（多行结构化，含 TX/RX、结果、诊断、重试、时间戳）

- **sh85selfchecktask**
  - 目标：定期并行执行 SH85 自检，汇总每轮结果
  - 建议日志：`scheduler/sh85selfchecktask/summary`、`scheduler/sh85selfchecktask/<QRCode>`
  - 开关（可按需在 INI 中补充）：`sh85selfchecktask.summary` / `sh85selfchecktask.devices`

- **vefc_sensor_monitor_task**
  - 目标：采集并落库 VEFC 相关传感器数据（如输出压力/平均温度），提供统计报表输入
  - 日志：`scheduler/vefc_sensor_monitor_task/summary.log`、`scheduler/vefc_sensor_monitor_task/devices.log`
  - `summary.log`：任务生命周期、轮次汇总、软件第一次打开记录、每日统计块
  - `devices.log`：设备指令通讯日志（`command retrying` / `command finished` / `requestFrame` / `responseFrame`）
  - 开关：`vefc_sensor_monitor_task.summary` / `vefc_sensor_monitor_task.devices`


### 2026-05-21 - Simon
**数据库日志系统日志记录完善**

#### 背景
原系统中部分日志输出使用 qDebug，未统一使用 LoggerManager，导致日志记录不规范。数据库相关操作缺少详细的日志追踪，不利于问题排查。

#### 修改内容

**1. LogCleanupScheduler 日志增强**
- 移除同一天内不重复清理检查的逻辑，每次定时器触发都执行清理检查
- 添加详细的检测情况日志，包括最早日期、最新日期、覆盖月数、保留阈值和是否需要清理
- 在关键节点添加日志刷新，确保日志及时写入
- 移除 m_lastCheckedDate 成员变量。

**2. AlarmDispatchTask 日志标准化**
- 将所有 qDebug 输出替换为 LoggerManager::instance().log()
- 在构造、启动、停止等关键节点添加日志记录
- 在 submitAlarm 方法中添加详细的提交日志，包括警报ID、类型、级别、设备标识和描述
- 在 submitResolve 方法中添加解决提交日志
- 在持久化操作中添加日志记录，包括插入和更新操作
- 在数据库不可用时记录错误日志

**3. AlarmLogQueryTask 日志标准化**
- 将 qDebug 输出替换为 LoggerManager::instance().log()
- 在构造、启动、停止等关键节点添加日志记录
- 在查询执行中添加时间范围钳制日志
- 在数据库连接不可用时记录错误日志

**4. CommunicateLogSqlLogic 日志完善**
- 引入 Defer 机制确保函数退出时自动刷新日志
- 统一日志路径格式为 "log_db/{db_name}/..."
- 在磁盘空间检查中添加详细的日志记录
- 在定时器启动和磁盘检查中添加日志刷新

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/logcleanupscheduler.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarmlogquerytask.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogsqllogic.cpp`

---

### 2026-05-19 - Simon
**ComunicateLogWidget UI 优化：ExecStatus 字符串化 / 历史日志居中对齐**

#### 背景
通讯日志的 `ExecStatus` 字段原以数字形式显示（0/1/2/3），用户需查阅文档才能理解含义。历史日志文本对齐方式不统一，影响阅读体验。

#### 修改内容
- **`comunicatelogwidget.cpp`**：新增静态辅助函数 `execStatusToString(int status)`，将状态码映射为可读字符串：
  - 0 → "Success"
  - 1 → "Timeout"
  - 2 → "Retry"
  - 3 → "Send Failed"
- **实时日志**：`setLiveLogData()` 第 5 列（Exec Status）由直接显示数字改为调用 `execStatusToString(execStatus.toInt())`
- **历史日志**：`setHistoryLogData()` 第 4 列（Exec Status）改为调用 `execStatusToString(r.execStatus)`
- **历史日志居中对齐**：`setHistoryLogData()` 中对除 Description（第 8 列）外的所有字段（0-7 列）设置 `Qt::AlignCenter`，提升表格对齐一致性

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`

---

### 2026-05-19 - Simon
**MonitorDataTask：禁用设备参数日志写入**

#### 背景
`device_param_log` 数据库用于记录设备参数变化历史，但当前写入频率过高（每秒每台设备写入一次），且未被上层功能使用。为减少数据库写入压力，暂时禁用该日志写入功能。

#### 修改内容
- **`monitor_data_task.cpp`**：注释掉将 foup 属性写入 `device_param_log` 数据库的代码块（约 10 行），保留代码结构以便后续按需恢复

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task.cpp`

---

### 2026-05-19 - Simon
**ReadBoardEnable：新增读取板卡禁用状态指令 + DebugPage 集成**

#### 背景
新增通过 Modbus FC04 读取板卡当前禁用/正常状态的功能，配合已有的 `SetBoardEnable` 写指令，形成完整的读写闭环。UI 集成在 DebugPage 中。

#### 数据层（`ModbusTcpMasterConfig.xml`）
- **新增 `ReadBoardEnable` 指令**（FC 04, 寄存器 0x00FF, 读 1 个寄存器）
  - 请求帧：`01 04 00 FF 00 01 CRC`
  - 响应帧：`01 04 02 XX XX CRC`（CH_1：1=禁用，0=正常）

#### UI 层（`boardenablestatuswidget.{h,cpp}` 新增）
- **新增 `BoardEnableStatusWidget`**（DebugPage 下的 SettingWidget）
  - Target Device：ComboBox 显示已注册设备 QRCode
  - Read 按钮：读取指定设备状态
  - Read All 按钮：读取所有已注册设备状态
  - 通过 `SendCommandTask::dataResult` 解析响应 `registerValue`，弹窗显示每台设备结果（`Normal (Enabled)` / `Disabled`）

#### DebugPage 集成（`debugpage.{ui,h,cpp}`）
- 导航栏新增 `Board Enable` 按钮
- 添加 `m_boardEnableStatusWidget` 成员和初始化

#### 构建系统（`debugsettingwidget.pri`）
- 新增 `boardenablestatuswidget.{h,cpp}`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/bin/config/ModbusTcpMasterConfig.xml`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.ui`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.h`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/debugsettingwidget.pri`
- 新增文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/boardenablestatuswidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/boardenablestatuswidget.cpp`

---

### 2026-05-19 - Simon
**SendCommandTask：修复 FC06 寄存器值未同步 rawBytes / 新增未连接设备早退**

#### 背景
`SendCommandTask` 覆盖 `registerValue` 后，`buildRequestFrame()` 仍使用模板预建的 `rawBytes`，导致实际发送帧中寄存器值始终为模板值（如 `00 00`），与传入参数不符。

#### 修复内容
- **`send_command_task.cpp::start()`**：覆盖 `registerValue` 后同步更新：
  - `request.rawBytes[4..5]`（FC06 RegisterValue 字节位置）
  - `response.registerValue` 及 `response.rawBytes[4..5]`（FC06 响应为请求镜像，需同步以通过校验）
- **新增连接状态检查**：在获取 master 后调用 `master->isConnected()`，未连接设备直接标记为失败并跳过，避免向离线设备发送指令并阻塞超时等待

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/send_command_task.cpp`

---

### 2026-05-19 - Simon
**PneumaticValvePressureSettingWidget：Target Device 改用 ComboBox**

#### 背景
原 Target Device 使用 `QSpinBox` 手动输入 QRCode，不直观且容易输错。改为 `QComboBox` 从配置文件读取已注册设备列表，提升操作安全性和便捷性。

#### 修改内容
- **`pneumaticvalvepressuresettingwidget.h`**：`QSpinBox *m_qrcodeSpinBox` 改为 `QComboBox *m_comboBox`，新增 `<QComboBox>` 头文件
- **`pneumaticvalvepressuresettingwidget.cpp`**：
  - `initQrcodeItem()`：改用 `QComboBox`，通过 `OHBDeviceConfig::getInstance().readMasterDevices()` 填充设备列表
  - `onSetBtnClicked()`：从 `m_comboBox->currentText()` 获取 QRCode
  - `onSetAllBtnClicked()`：改用 `OHBDeviceConfig::getInstance().readQRCodes()` 获取全部设备列表

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/pneumaticvalvepressuresettingwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/pneumaticvalvepressuresettingwidget.cpp`

---

### 2026-05-19 - Simon
**ComunicateLogWidget：历史日志 CommandId 列修复**

#### 背景
历史日志第 3 列（CommandId）错误地显示了 `qrCode` 字段内容，应显示 `commandId`。

#### 修改内容
- `comunicatelogwidget.cpp::setHistoryLogData()`：第 3 列由 `r.qrCode` 改为 `r.commandId`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`

---

### 2026-05-18 - Simon
**MetaTypes：集中注册 SchedulerTask::State 元类型**

#### 背景
`SchedulerTask::State` 在跨线程队列连接（`stateChanged` 信号）中触发 Qt 警告 `Cannot queue arguments of type 'SchedulerTask::State'`，需注册元类型。原临时在构造函数中注册，现移至统一管理处。

#### 修改内容
- **`metatypes.cpp`**：在 `MetaTypes::registerTypes()` 中添加 `qRegisterMetaType<SchedulerTask::State>("SchedulerTask::State")`，并包含 `scheduler/scheduler_task.h`
- **`scheduler_task.h`**：移除构造函数中的临时注册代码，构造函数恢复简洁

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/app/metatypes.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/scheduler_task.h`

---

### 2026-05-18 - Simon
**DeviceEnableSettingWidget：板卡禁用使能指令下发**

#### 背景
`DeviceEnableSettingWidget` 原有功能仅在本地持久化设备 Enable/Disable 状态到 `ohb_device.ini`，未向设备下发 Modbus 指令。现新增通过 `SendCommandTask` 下发 `SetBoardEnable`（FC06, 寄存器 0x00FF）指令，且仅在指令成功后才持久化配置。

#### UI 层（`deviceenablesettingwidget.{h,cpp}`）
- **新增 `submitBoardEnableCommand()` 方法**：通过 `SendCommandTask` 向指定设备下发 `SetBoardEnable` 指令
  - UI "Enable" → 寄存器值 `0x0000`（板卡正常）
  - UI "Disable" → 寄存器值 `0x0001`（板卡禁用）
- **流程调整**：`onSetClicked()` 不再先持久化，改为先下发 Modbus 指令，在 `allFinished` 成功回调中才：
  1. 调用 `OHBDeviceConfig::setDeviceEnable()` 持久化到 `ohb_device.ini`
  2. 更新内存 `FoupOfOHBInfo::enable` 状态
  3. 弹窗提示成功/失败
- 指令失败时不修改配置文件，状态栏显示失败

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/deviceenablesettingwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/deviceenablesettingwidget.cpp`
- 依赖：
  - `scheduler/tasks/send_command_task.h`（已有通用任务，直接复用）
  - `bin/config/ModbusTcpMasterConfig.xml`（已有 `SetBoardEnable` 指令定义）

---

### 2026-05-18 - Simon
**SH85 周期自检 UI/Task 增量改进：英文化 / Participated 列 / Success 配色 / FOUP in 过滤 / 触屏滚动 / TX-RX 原始帧日志**

#### 背景
在前一版 `SH85 周期自检功能重构` 基础上，根据现场使用反馈做以下增量改进：UI 全英文化、增加设备参与状态实时反馈、Success 列条件配色、过滤处于 FOUP in 状态的设备、表格触屏滑动、并在 `SH85SelfChecker` 响应日志中记录 TX/RX 原始数据帧便于排查通讯问题。

#### UI 层（`sh85selfcheckreportdialog.{h,cpp}` / `sh85periodicselfchecksettingwidget.cpp`）
- **全英文化**：
  - SettingWidget 状态文案：`Self-check disabled` / `Self-checking (elapsed: Xs)` / `Waiting for next check (countdown: Xs)`
  - Live Log 表头：`QRCode` / `Execution Status` / `Countdown(s)` / `Success` / `Participated`
  - Live Log `Execution Status` 单元格：通过本地 `stateText()` 把 `SH85SelfChecker::stateToString()` 中文状态映射为英文（`Idle` / `Sending self-check command` / `Waiting 5s (before phase 1)` / `Reading self-check status (phase 1)` / `Waiting 55s (before phase 2)` / `Polling self-check status` / `Done`）
  - History Log 表头：`Last Check Start Time` / `Success Count` / `Failure Count` / `Participated` / `Description`
- **新增 Live Log `Participated` 列（第 5 列）**：
  - `onRoundStarted()` 默认全部置为 `Yes`
  - 仅在收到 `deviceParticipated(qrcode, false)` 时改为 `No`，UI 即时反馈无需等本轮结束
- **`Success` 列条件配色**：
  - 成功：背景 `#32CD32`（Lime Green），字体保持默认
  - 失败：背景 `#DC143C`（Crimson），字体白色
  - 轮次开始时背景重置为 `transparent`、字体重置为黑色
- **History Log 列宽**：`Last Check Start Time` 固定 `200px`，其他列 `Interactive` 可拖拽，最后一列 stretch
- **触屏滑动**：参考 `OperationLogWidget::enableTouchScroll`，对 Live/History 两个 `QTableView` 启用 `QScroller::LeftMouseButtonGesture` 拖动手势，滚动条 handle 默认显示明显颜色 `#D4D0C8`

#### 调度层（`sh85_periodic_self_check_task.{h,cpp}`）
- **新增信号** `deviceParticipated(const QString& qrcode, bool participated)`：在 `beginRound()` 遍历设备时即时发射，提前通知 UI 设备是否参与本轮
- **新增 FOUP in 过滤**：`beginRound()` 中若设备 `FoupOfOHBInfo::foupIn() == true`，跳过自检，描述置为 `FOUP in place, skipped`，并 `emit deviceParticipated(qrcode, false)`
- 未启用设备的过滤路径同步发射 `deviceParticipated(qrcode, false)`，参与设备发射 `deviceParticipated(qrcode, true)`

#### 数据层（`sh85selfchecker.cpp`）
- **TX/RX 原始帧写入日志**：在 `onCommandFinished()` 中无论响应成功（INFO）还是失败（WARN），日志均追加请求与响应的原始字节（`rawBytes + crc`，大写空格分隔的 HEX）：
  ```
  [data][SH85SelfChecker] 响应 state=xxx ok=xxx id=xxx masterId=xxx
  TX：XX XX XX XX ...
  RX：XX XX XX XX ...
  ```
- 新增内部辅助 `frameToHex(const ModbusFrame&)`

#### 信号连线（`sh85periodicselfchecksettingwidget.cpp`）
- 新增 `task::deviceParticipated` → `SH85SelfCheckReportDialog::onDeviceParticipated`（`Qt::QueuedConnection`）

#### 工具层（`tool/communicationrecorder/communicationrecorder.{h,cpp}`）
- **节流范围收窄**：`CommunicationRecorder` 之前对所有 Modbus 指令进行节流上报，现仅对 `THROTTLED_COMMAND_ID = "ReadFoupStatus"` 这一条高频指令做节流（FOUP in 1s / FOUP out 3s 阈值），其它指令在 `submitCommand()` 中**立即全量发射** `shouldEmit`，确保业务/低频指令日志不丢失。
- 新增私有常量 `static constexpr const char* THROTTLED_COMMAND_ID = "ReadFoupStatus";`，后续如需扩展可改为集合。
- `start()` / `stop()` / `onTick()` 行为保持原节流逻辑（仅服务于目标指令）。

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85_periodic_self_check_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85_periodic_self_check_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85selfcheckreportdialog.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85selfcheckreportdialog.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/sh85selfchecker.cpp`
  - `OHB80PortMonitor_V_1_0_0/tool/communicationrecorder/communicationrecorder.h`
  - `OHB80PortMonitor_V_1_0_0/tool/communicationrecorder/communicationrecorder.cpp`
  - `OHB80PortMonitor_V_1_0_0/bin/config/ModbusTcpMasterConfig.xml`（新增 `SetBoardEnable` 指令，功能码 06 / 寄存器 0x00FF）
- 文档：
  - `OHB80PortMonitor_V_1_0_0/docs/realize/sh85_periodic_self_check.md`（按当前实现重写）

---

### 2026-05-18 - Simon
**SH85 周期自检功能重构：并行执行 / 状态机 / 自检报告 Dialog**

#### 背景
原 `SH85PeriodicSelfCheckTask` 采用串行执行（一台设备完成后才启动下一台），UI 仅展示当前进度与上一轮结果，缺少按设备维度的实时与历史明细，且控制接口（仅 `setIntervalSeconds`）不够语义化。本次按新需求重构。

#### 调度层（`scheduler/tasks/sh85_periodic_self_check_task.{h,cpp}`）
- **状态机** `State { Stopped, Checking, WaitingNext }`，1Hz 定时器仅在 `WaitingNext` / `Checking` 期间启用
- **公开接口**：`setEnabled(bool)`、`setPeriod(int value, TimeUnit unit)`，`TimeUnit { Second, Minute, Hour }`
- **执行逻辑**（一轮）：
  1. 遍历所有 QRCode，过滤 `enable=false` 的设备（`Not enabled`，不参加）
  2. 网络未连接的可参加设备 → 直接判失败（`Device not connected`）
  3. 其余可用设备 **并行启动** `SH85SelfChecker`
  4. 所有设备结束后发出 `allFinished(SelfCheckSummary)`，进入 `WaitingNext`
- **信号**：
  - `countdownTick(int, masterId)`：转发自 `SH85SelfChecker::countdownTick`
  - `selfCheckerStateChanged(SH85SelfChecker::State, masterId)`：转发自 `SH85SelfChecker::stateChanged`
  - `oneFinished(masterId, success, description)`
  - `allFinished(SelfCheckSummary)`
  - `taskStateChanged(State)`、`elapsedTick(int)`、`intervalCountdown(int)`：UI 辅助信号
- **数据结构**：`DeviceResult{qrcode, participated, success, description}`、`SelfCheckSummary{startTime, endTime, successCount, failureCount, details}`

#### UI 层
- **`sh85periodicselfchecksettingwidget.{h,cpp}`** 重写，4 个 Item：
  - **启用开关**：`ComboBox` (`false` / `true`) → `task->setEnabled(...)`
  - **周期参数**：`SpinBox` + `ComboBox(s/min/hour)` + `Set` 按钮 → `task->setPeriod(value, unit)`
  - **自检状态**：只读 `LineEdit`，三种文案：
    - "自检功能未启用"
    - "自检中（执行：Xs）"
    - "等待下次自检中（倒计时：Xs）"
  - **查看报告**：`PushButton`，打开自检报告模态框
- **常驻任务**：由 `SharedData::initScheduler()` 创建并提交至调度器；UI 通过 `SharedData::getSH85PeriodicSelfCheckTask()` 获取实例并订阅信号；`setEnabled` / `setPeriod` 通过 `QMetaObject::invokeMethod` 跨线程下发
- **新增 `sh85selfcheckreportdialog.{h,cpp}`**：自检报告模态框（Dialog + TabWidget）
  - **Tab 1 Live Log**：80 行（项目所有 QRCode），列：QRCode / 执行状态 / 倒计时 / 是否成功
  - **Tab 2 History Log**：80 行（每个 QRCode 一行），列：上一次自检开始时间 / 成功次数 / 失败次数 / 是否参加 / Description
  - 数据仅保存在内存中（不持久化），按设备维度累计

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85_periodic_self_check_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/sh85_periodic_self_check_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/configsettingwidget.pri`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.h`（新增 `getSH85PeriodicSelfCheckTask()`）
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.cpp`（在 `initScheduler()` 中提交常驻任务）
- 新增文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85selfcheckreportdialog.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85selfcheckreportdialog.cpp`

---

### 2026-05-16 - Simon
**固件升级任务补发QRCode指令功能**

#### 背景
设备固件升级完成后，需要补发QRCode指令（WriteQRCode）以更新设备中的QRCode信息，确保固件升级后设备的身份标识正确。

#### 修改内容
- **FirmwareUpgradeTask**：在 `onUpgraderFinished()` 槽函数中，当设备固件升级成功（`success == true`）时，自动调用 `submitWriteQRCode(masterId)` 补发QRCode指令
- **submitWriteQRCode()** 方法实现：
  - 从 `ModbusTcpMasterManager` 获取设备 master 对象和 CommandPool
  - 从 CommandPool 克隆 WriteQRCode 指令模板
  - 将 masterId 转换为4字节大端序数据填充到指令中
  - 通过 `ModbusCommandSender::submit()` 发送指令
  - 记录待处理指令映射 `m_writeQRCodePendingMap`，用于响应回调匹配
- **onWriteQRCodeFinished()** 槽函数：处理 WriteQRCode 指令响应，记录成功或失败日志
- **资源管理**：在 `stop()` 方法中添加 WriteQRCode 信号连接的清理逻辑

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/firmware_upgrade_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/firmware_upgrade_task.cpp`

---

### 2026-05-16 - Simon
**三大日志控件 UI 体验优化：列对齐/列宽/权限可见性/Description 弹窗/Hex 显示**

#### 背景
三大日志控件（AlarmLogWidget / ComunicateLogWidget / OperationLogWidget）的实时日志（live log）和历史日志（history log）存在多处 UI 体验问题：
- 列对齐方式不统一，时间字段显示不完整
- 通讯日志的敏感字段对所有权限用户均可见
- 历史通讯日志多余 ID 列，QRCode 列位置不直观
- 警报历史日志的 "Is Resolved" 字段缺少颜色编码
- 通讯历史日志 Request/Response 字段以原始字节流显示，不便阅读
- Description 字段可能很长，单元格无法完整显示

#### 修改内容

**1. 列对齐与列宽统一**
- 三个 log 控件的 live log 和 history log 表格中，除 Description 字段外其余字段均居中对齐
- 时间字段（Send Time / Response Time / Occur Time / Resolve Time）等关键字段列宽调整：
  - ComunicateLogWidget Send Time / Response Time 调整为 240px，Description 缩减至 150px，确保高权限用户登录后时间字段完整显示

**2. 权限驱动的列可见性（ComunicateLogWidget）**
- 新增 `updateColumnVisibility()` 方法：Engineer (UserPermission >= 3) 以下用户隐藏敏感列（Response Time / Duration Ms / Exec Status / Retry Count / Request / Response）
- 在 `initLiveLog()`、`setHistoryLogData()` 及 `CommunicatePage` 接收 `UserManager::permissionChanged` 信号时调用，保证权限切换或数据刷新后列可见性正确
- `CommunicatePage` 新增 `onPermissionChanged` 槽，连接 `UserManager::permissionChanged` 信号

**3. 历史通讯日志列结构调整**
- 移除历史日志 "ID" 列
- 将 "QRCode" 列移至第 0 列（最前）
- 同步更新 `updateColumnVisibility()` 中的列索引

**4. AlarmLogWidget 历史日志 "Is Resolved" 背景色**
- `setHistoryLogData()` 中对 "Is Resolved" 字段（第 5 列）按枚举值染色，与 live log 配色保持一致：
  - `Unresolved` (0) → `QColor("#DC143C")` 红色，字体白色
  - `Resolved` (1) → `QColor("#32CD32")` 绿色，字体白色
  - `NoNeed` (2) → `QColor(255, 255, 200)` 黄色

**5. ComunicateLogWidget 历史日志 Request/Response 字段 Hex 显示**
- `setHistoryLogData()` 将 `sendFrame` / `responseFrame` (`QByteArray`) 通过 `toHex(' ').toUpper()` 转换为以空格分隔的大写十六进制字符串显示
- 显式包裹 `QString(...)` 以解决 `QStandardItem(QByteArray)` 重载二义性编译错误

**6. Description 字段单击弹窗**
- 三个 log 控件的 live log 和 history log 中，单击 Description 单元格弹出 `QMessageBox` 模态框显示完整内容
- AlarmLogWidget / ComunicateLogWidget 弹窗标题栏显示 `Description - <QRCode>`，便于辨识当前记录
- OperationLogWidget 在已有 `onHistoryLogClicked` 槽中新增 Description 列分支，不破坏原有匹配命中选中逻辑
- `QMessageBox` 设置 `Qt::TextSelectableByMouse`，文本可选中复制

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/communicatepage.h`
  - `OHB80PortMonitor_V_1_0_0/ui/communicatepage.cpp`

---

### 2026-05-16 - Simon
**三大日志控件按用户权限过滤记录**

#### 背景
`OperationRecord` / `AlarmRecord` / `CommunicateRecord` 三种日志记录均带有 `user_permission` 字段，UI 层未使用该字段过滤展示，低权限用户登录后能看到高权限用户产生的记录，存在权限泄露风险。

#### 过滤规则
层级过滤：仅展示 `record.userPermission <= currentPermission` 的记录。Root 可看所有；Guest 仅看 Guest 级。未登录时 `UserManager::currentPermission()` 默认返回 `Guest`，行为正确无需改动。

#### 修改内容
- **OperationLogWidget**：`onRecordInserted()` 实时过滤；`setHistoryLogData()` 历史查询过滤
- **AlarmLogWidget**：`onRecordInserted()` 实时过滤（`loadUnresolvedToLiveLog()` 通过该函数自动继承）；`setHistoryLogData()` 历史查询过滤
- **ComunicateLogWidget**：`setHistoryLogData()` 历史查询过滤（实时 `writeLog()` 不带 user_permission 字段，暂不过滤）

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`

#### 已知限制
- 历史查询过滤在 UI 层进行，分页计数（SQL `COUNT(*)`）仍含被过滤记录，存在轻微不一致
- 用户切换登录状态后，已展示的实时日志条目不会自动清理或追溯展示

---

### 2026-05-16 - Simon
**配置文件合并：QRCodeConfig + NetworkConfig 合并为 OHBDeviceConfig**

#### 背景
原有 `qrcode.ini`（二维码映射）和 `network.ini`（IP/Port）配置分散在两个文件中，对应两个独立的配置类 `QRCodeConfig` / `NetworkConfig`。每个 OHB 设备的完整信息（QRCode + IP + Port）被拆分管理，调用方需同时引用两个类，维护成本高。

#### 修改内容

**1. 配置文件合并**
- 新增 `bin/config/ohb_device.ini`，合并原 `qrcode.ini` + `network.ini` 内容
- 节结构：
  - `[MasterDevices] list=...`：主设备列表
  - `[OHB1] QRCode=12001 / Ip=... / Port=...`：每个设备一节，包含 QRCode + IP + Port
- 删除 `bin/config/qrcode.ini` 和 `bin/config/network.ini`

**2. 新增 OHBDeviceConfig 类（位于 app/ 目录）**
- `app/ohbdeviceconfig.{h,cpp}`：单例配置类，封装 ohb_device.ini 的读写
- 新增 `OHBDeviceInfo` 结构体（qrCode + ip + port）
- 主要接口：
  - `readDevices()` / `writeDevices()`：批量读写所有设备
  - `readQRCodes()`：仅读取 QR 码列表
  - `readMasterDevices()` / `writeMasterDevices()`：主设备列表
  - `getDeviceByQRCode()` / `getDeviceByMasterId()`：单设备查询

**3. AppConfig 同步重构**
- 移除 `getNetworkConfig()` / `getQRCodeConfig()` / `getNetworkConfigPath()` / `getQRCodeConfigPath()`
- 新增 `getOHBDeviceConfig()` / `getOHBDeviceConfigPath()`
- 移除对 `networkconfig.h` / `qrcodeconfig.h` 的依赖

**4. 调用方更新**
- `app/shareddata.cpp`：原来分别调用 `getNetworkConfig().readNetworkConfig()` + `getQRCodeConfig().readQRCodeMapping()`，合并为单次 `getOHBDeviceConfig().readDevices()`
- `ui/customwidget/configsettingwidget/sh85selfchecksettingwidget.cpp`：移除未使用的 `qrcodeconfig.h` 包含

**5. 构建系统**
- `app/app.pri`：加入 `ohbdeviceconfig.{h,cpp}`
- `config/config.pri`：移除 `qrcodeconfig.{h,cpp}` / `networkconfig.{h,cpp}`

#### 影响范围
- 新增文件：
  - `OHB80PortMonitor_V_1_0_0/app/ohbdeviceconfig.h`
  - `OHB80PortMonitor_V_1_0_0/app/ohbdeviceconfig.cpp`
  - `OHB80PortMonitor_V_1_0_0/bin/config/ohb_device.ini`
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/app/appconfig.h`
  - `OHB80PortMonitor_V_1_0_0/app/appconfig.cpp`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.cpp`
  - `OHB80PortMonitor_V_1_0_0/app/app.pri`
  - `OHB80PortMonitor_V_1_0_0/config/config.pri`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85selfchecksettingwidget.cpp`
- 删除文件：
  - `OHB80PortMonitor_V_1_0_0/config/qrcodeconfig.h`
  - `OHB80PortMonitor_V_1_0_0/config/qrcodeconfig.cpp`
  - `OHB80PortMonitor_V_1_0_0/config/networkconfig.h`
  - `OHB80PortMonitor_V_1_0_0/config/networkconfig.cpp`
  - `OHB80PortMonitor_V_1_0_0/bin/config/qrcode.ini`
  - `OHB80PortMonitor_V_1_0_0/bin/config/network.ini`

---

### 2026-05-16 - Simon
**AlarmLogWidget live log 表格 Is Resolved 字段背景色根据状态显示**

#### 修改内容
- `onRecordInserted()` 方法：根据 `isResolved` 枚举值仅对第 4 列（Is Resolved 字段）设置背景色
  - `Unresolved` (0): 鲜艳红色背景 `QColor("#DC143C")`，字体白色
  - `Resolved` (1): 绿色背景 `QColor("#32CD32")`，字体白色
  - `NoNeed` (2): 黄色背景 `QColor(255, 255, 200)`
- `onRecordResolved()` 方法：当记录被标记为已解决时，仅更新第 4 列背景色为绿色
- `loadUnresolvedToLiveLog()` 方法：通过调用 `onRecordInserted()` 自动继承背景色设置功能

#### 效果
- live log 表格的 Is Resolved 字段根据警报的解决状态显示不同的背景颜色，便于用户快速识别未解决的警报
- 未解决警报显示红色背景，已解决警报显示绿色背景，无需解决警报显示黄色背景
- 仅该字段有背景色，不影响其他列

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`

---

### 2026-05-16 - Simon
**新增设备可用性配置控件（DeviceEnableSettingWidget）**

#### 修改内容
- 创建 `deviceenablesettingwidget.{h,cpp}`，提供 OHB 设备可用性（Enable/Disable）配置功能
- UI 组成：
  - Target Device QRCode SpinBox：选择设备 QRCode，初始值使用 `SharedData::getAllQrcodes()` 第一个
  - Device Status ComboBox：Enable/Disable 两个选项
  - Set 按钮：设置按钮
- 实现功能：
  - 点击 Set 按钮时，根据 QRCode 调用 `SharedData::getFoupByQRCode()` 获取 `FoupOfOHBInfo*`
  - 调用 `setEnable()` 修改设备的 enable 属性
  - 添加成功/失败状态提示（`setStatusOK()` / `setStatusFailed()`）
- 集成到 ConfigPage：
  - 在 `configpage.ui` 添加 "Device Enable" 导航按钮
  - 在 `configpage.{h,cpp}` 添加成员变量和初始化代码
  - 在 `configsettingwidget.pri` 添加新文件到构建系统

#### 影响范围
- 新增文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/deviceenablesettingwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/deviceenablesettingwidget.cpp`
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/configpage.ui`
  - `OHB80PortMonitor_V_1_0_0/ui/configpage.h`
  - `OHB80PortMonitor_V_1_0_0/ui/configpage.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/configsettingwidget.pri`

---

### 2026-05-16 - Simon
**运行日志窗口显示问题修复**

#### 背景
运行日志窗口（OperationLogWidget）打开后，如果被主界面覆盖，点击公告栏无法再次显示。

#### 修改内容
- 将 OperationLogWidget 创建为无父窗口的独立窗口（`new OperationLogWidget(nullptr)`），避免窗口层级冲突
- 在 UIDemo6 析构函数中手动释放资源，避免内存泄漏

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/uidemo6.cpp`

---

### 2026-05-16 - Simon
**HomePage Disable 颜色标签样式设置统一**

#### 修改内容
- 将 labDisableColor 的样式设置从 UI 文件移到代码中
- 将 labDisableColor 添加到 StatusLabelPair 数组中，使用 `FrameDevice::DeviceStatus::Disable` 状态
- 通过循环统一设置样式，与其他状态标签保持一致

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/homepage.cpp`

---

### 2026-05-15 - Simon
**调整警报日志信息显示方式**

#### 背景
原项目中调度任务和 UI 层都直接调用 `OperationDispatchTask::logMessage()` 写入运行日志，造成职责混乱。部分任务在失败时不记录汇总日志，仅依赖 UI 层的冗余日志。

#### 修改内容

**1. 调度任务层运行日志标准化（7 个任务）**
- 参照 `SetPurgeFlowTask` 的日志模式，统一以下任务的日志写入方式：
  - `SetUIRefreshTimeTask`、`SetVEFCGasTypeTask`、`SetPneumaticValvePressureTask`、`SetIdlePurgeTask`、`SetHumidityOffsetTask`、`SetFirmwareConfigTask`、`ReadVEFCFlowUnitAndMediumStatusTask`
- 统一模式：
  - 任务启动时写一条 `Message` 级日志
  - 每设备失败在失败发生点立即写一条 `Error` 级日志（设备不可用、克隆失败、命令失败、超时未响应）
  - `forceFinish()` 中写汇总日志：
    - 成功：`Message` 级，格式为 `{TaskName} task completed: X devices succeeded`
    - 失败：`Error` 级，格式为 `{TaskName} task finished: X succeeded, Y failed`
  - `m_totalCount == 0` 路径改为走 `forceFinish`，确保汇总日志不遗漏

**3. UI 层运行日志写入完全移除（9 个文件）**
- `uirefreshtimesettingwidget.cpp`：移除失败设备汇总日志
- `vefcgastypesettingwidget.cpp`：移除失败设备汇总日志
- `pneumaticvalvepressuresettingwidget.cpp`：移除失败设备汇总日志
- `idlepurgesettingwidget.cpp`：移除失败设备汇总日志
- `humidityoffsetsettingwidget.cpp`：移除失败设备汇总日志
- `vefcflowunitmediumstatuswidget.cpp`：移除 VEFC 自检失败汇总日志
- `logindialog.cpp`：移除登录成功/失败日志
- `changepassworddialog.cpp`：移除密码修改成功/失败日志
- `useraccountlabel.cpp`：移除登出日志
- 同步清理不再需要的 `#include "scheduler/tasks/operation_dispatch_task.h"` 和 `#include "app/shareddata.h"`

**4. VEFCFlowUnitMediumStatusWidget 失败弹窗分类显示**
- 失败设备按 3 类分组：Communication Failed / Unit Config Failed / Medium Config Failed
- 汇总弹窗后，针对每个非空类别单独弹窗显示设备列表
- 保留 UI 层用于用户交互的 QMessageBox，不涉及运行日志

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_ui_refresh_time_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_vefc_gas_type_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_pneumatic_valve_pressure_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_idle_purge_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_humidity_offset_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_firmware_config_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/read_vefc_flow_unit_medium_status_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/uirefreshtimesettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/vefcgastypesettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/pneumaticvalvepressuresettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/idlepurgesettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/humidityoffsetsettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/vefcflowunitmediumstatuswidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/logindialog.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/changepassworddialog.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/useraccountlabel/useraccountlabel.cpp`

---

### 2026-05-15 - Simon
**SetUIRefreshTimeTask 参数扩展：从 2 参数改为 3 参数**

#### 背景
根据 CYTC_OHBP 通讯手册 6.3 节，UI 页面刷新时间配置需要 3 个参数：
- CH_1: logo 界面展示时间
- CH_2: 参数界面展示总时间
- CH_3: 参数界面切换时间

原实现只有 2 个参数（logScreenSec, propertyScreenSec），对应 2 寄存器/4 字节，不符合通讯规范。

#### 修改内容
- `ModbusTcpMasterConfig.xml`：WriteUIRefreshTime 指令 RegisterCount 从 00 02 改为 00 03，ByteCount 从 04 改为 06，响应帧 RegisterCount 同步更新为 00 03
- `SetUIRefreshTimeTask` 构造函数参数改为 `(qrcodes, logoSec, paramTotalSec, paramSwitchSec)`，`allFinished` 信号增加第 3 个参数，成员变量改为 `m_logoSec` / `m_paramTotalSec` / `m_paramSwitchSec`，`buildPayload()` 生成 6 字节大端数据
- `UIRefreshTimeSettingWidget` UI 从 2 个 SpinBox 改为 3 个 SpinBox：Logo Screen Duration、Param Screen Total Duration、Param Page Switch Interval

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/bin/config/ModbusTcpMasterConfig.xml`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_ui_refresh_time_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/uirefreshtimesettingwidget.{h,cpp}`

---

### 2026-05-15 - Simon
**Modbus 通讯防粘帧：启用 TCP_NODELAY 禁用 Nagle 算法**

#### 背景
线下批量任务（如 SetPurgeFlow / SetHumidityOffsetThreshold）下发时，抓包发现 80 设备中部分设备（如 12007）连接被对端 RST 关闭。Wireshark 分析显示客户端把多达 9 个 Modbus RTU 帧（每帧 8 字节）粘合成一个 72 字节 TCP segment 推送给 HF2211 串口转 TCP 网关，网关串口侧因粘帧无法正确分帧而 RST。

应用层 `ModbusCommandSender` 已经做了串行化（`m_hasPendingCommand` 互斥）以及 50ms 最小发送间隔节流，但内核 TCP 层在对端 ACK 慢时仍然按 **Nagle 算法** 把多个未 ACK 的小 write 合并成一个 segment 发送，应用层节流无法干预内核行为。

#### 修改内容

**1. ModbusCommandSender 加入 50ms 最小发送间隔节流**
- `modbuscommandsender.h`：新增 `MIN_SEND_INTERVAL_MS = 50` 常量与 `m_lastSendMs` / `m_dispatchScheduled` 成员
- `modbuscommandsender.cpp::dispatch()`：相邻两次 write 间隔 < 50ms 时改用 `QTimer::singleShot` 延后到时再 dispatch，避免应用层连续灌入小帧
- `modbuscommandsender.cpp::doSend()`：成功 write 后记录 `m_lastSendMs`
- `modbuscommandsender.cpp::stop()`：重置 `m_lastSendMs = 0` 避免重连后误节流

**2. ModbusConnecter 启用 TCP_NODELAY**（核心修复）
- `modbusconnecter.cpp` 构造函数中，挂接 `QTcpSocket::connected` 信号 lambda：
  - `setSocketOption(QAbstractSocket::LowDelayOption, 1)` 禁用 Nagle 算法
  - `setSocketOption(QAbstractSocket::KeepAliveOption, 1)` 启用 keepalive
  - lambda 必须先于 `onAsyncReconnectConnected` 连接，保证选项优先生效
- `modbusconnecter.cpp::performConnection()`：同步连接路径下 `waitForConnected` 成功后再次显式设置（与信号 lambda 幂等）

#### 效果
- 禁用 Nagle 后，每次 `socket->write()` 内核立即发为独立 TCP segment，不再等待已发数据的 ACK
- 配合 50ms 应用层节流，9 个 8 字节 Modbus RTU 帧将变成 9 个独立的 8 字节 TCP segment，HF2211 串口侧严格按 RTU 帧间隔接收，不再误判粘帧
- 即使设备响应慢、ACK 延迟大，多次 write 也不会被合并成一个超长 segment

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbuscommandsender.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbusconnecter.cpp`

---

### 2026-05-15 - Simon
**SetUIRefreshTimeTask 参数扩展：从 2 参数改为 3 参数（3 寄存器/6 字节）**

#### 背景
根据 CYTC_OHBP 通讯手册 6.3 节，UI 页面刷新时间配置需要 3 个参数：
- CH_1: logo 界面展示时间
- CH_2: 参数界面展示总时间
- CH_3: 参数界面切换时间

原实现只有 2 个参数（logScreenSec, propertyScreenSec），对应 2 寄存器/4 字节，不符合通讯规范。

#### 修改内容

**1. ModbusTcpMasterConfig.xml**
- WriteUIRefreshTime 指令：
  - RegisterCount: `00 02` → `00 03`
  - ByteCount: `04` → `06`
  - RegisterValue: `00 00 00 00` → `00 00 00 00 00 00`
  - 响应帧 RegisterCount 同步更新为 `00 03`

**2. SetUIRefreshTimeTask**
- 构造函数参数：`(qrcodes, logScreenSec, propertyScreenSec)` → `(qrcodes, logoSec, paramTotalSec, paramSwitchSec)`
- `allFinished` 信号：增加第 3 个参数 `paramSwitchSec`
- 成员变量：`m_logScreenSec` / `m_propertyScreenSec` → `m_logoSec` / `m_paramTotalSec` / `m_paramSwitchSec`
- `buildPayload()`: 生成 6 字节大端数据（CH_1 logo时间, CH_2 参数总时间, CH_3 切换时间）
- 所有日志格式同步更新为 3 参数格式

**3. UIRefreshTimeSettingWidget**
- UI 从 2 个 SpinBox 改为 3 个 SpinBox：
  - Logo Screen Duration — Logo screen display duration (seconds)
  - Param Screen Total Duration — Parameter screen total display duration (seconds)
  - Param Page Switch Interval — Parameter page switch interval (seconds)
- Set / Set All 按钮放在最后一个 item 上，传递 3 个参数
- 所有日志和提示信息同步更新

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/bin/config/ModbusTcpMasterConfig.xml`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_ui_refresh_time_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_ui_refresh_time_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/uirefreshtimesettingwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/uirefreshtimesettingwidget.cpp`

---

### 2026-05-15 - Simon
**Modbus 原始帧日志：收发帧写入 raw_data/{masterId}.log**

#### 修改内容
- `modbuscommandreceiver.h/cpp`：新增 `logRawFrame` 方法，在 `commandSucceeded` / `commandFailed` 信号发射前将请求/响应原始帧写入 `raw_data/{masterId}.log`
  - 成功：`[cmdId] TX: ...` / `[cmdId] RX: ...`
  - 失败：`[cmdId] TX: ...` / `[cmdId] ERROR: ...`
- `periodiccommandsender.cpp`：在 `disconnectDevice` 信号发射前写入 `raw_data/{masterId}.log`，记录连续失败达到阈值准备断开连接
- `periodiccommandsender.h`：`MAX_CONSECUTIVE_FAILURES` 从 10 调整为 100

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbuscommandreceiver.h`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbuscommandreceiver.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/periodiccommandsender.h`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/periodiccommandsender.cpp`

---

### 2026-05-15 - Simon
**SetPurgeFlowTask 运行日志重构：失败日志在发生点写入，总结日志在 emit 前写入**

#### 修改内容
- `set_purge_flow_task.cpp`：
  - 每设备失败日志在失败发生点立即写入（设备不可用、克隆失败、命令失败、超时未响应），不再集中到 `forceFinish` 处理
  - 任务总结日志在 `forceFinish` 中 `emit allFinished` 前统一写入（成功写 Message，失败写 Error）
  - `m_totalCount == 0` 路径改为走 `forceFinish`，确保总结日志不遗漏
- `purgeflowsettingwidget.cpp`：移除 UI 层的 `logMessage` 调用，运行日志统一由调度任务类负责

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_purge_flow_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/purgeflowsettingwidget.cpp`

---

### 2026-05-14 - Simon
**NetworkStatusTask DeviceOffline 告警提交条件放宽 + alarmpage 移除冗余提交**

#### 修改内容
- `network_status_task.cpp`：DeviceOffline 告警提交条件从 `lastStatus == Connected` 改为 `status ∈ {Disconnected, Error}`，避免从未连接成功的设备永远不触发告警
- `alarmpage.h/cpp`：移除 `onNetworkStatusChanged` 槽及重复的告警提交逻辑，DeviceOffline 告警统一由 NetworkStatusTask 内部处理

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/alarmpage.h`
  - `OHB80PortMonitor_V_1_0_0/ui/alarmpage.cpp`

---

### 2026-05-14 - Simon
**Modbus 协议文档更新：ReadFoupStatus CH_9 设备状态位定义**

#### 修改内容
- `ModbusTcpMasterConfig.xml`：CH_9 从"预留"更新为设备状态位定义
  - bit0: VEFC状态（0=OK，1=NG）
  - bit1: 温湿度传感器（0=OK，1=NG）
  - bit2: FOUP IN充气30min湿度是否达标（0=OK，1=NG）
- `commandresponseparser.h`：同步更新 `parseReadFoupStatus` 注释
- `commandresponseparser.cpp`：更新解析实现，解析 3 个设备状态位

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/bin/config/ModbusTcpMasterConfig.xml`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbuscommand/commandresponseparser.h`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbuscommand/commandresponseparser.cpp`

---

### 2026-05-14 - Simon
**新增设备告警类型：VEFC/VEEP/SH85 异常与湿度未达标**

#### 修改内容
在 `AlarmType` 枚举中新增 4 个告警类型：
- `VEFCAbnormal = 5001`：VEFC 异常（流量控制器异常），用户可见
- `VEEPAbnormal = 5002`：VEEP 异常（压力控制器异常），用户可见
- `SH85Abnormal = 5003`：85 异常（温湿度传感器异常），用户可见
- `HumidityNotReached = 5101`：湿度未达标（充氮半小时，湿度不达标），用户可见

同步更新：
- `alarmTypeName()`：添加显示名称
- `alarmTypeList()`：添加到 UI 下拉框列表
- `alarmTypeToLevel()`：设置级别为 `Error`
- `alarmTypeToResolvedStatus()`：设置解决状态为 `Unresolved`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/app/alarmtype.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task.cpp`

---

### 2026-05-14 - Simon
**MonitorDataTask 添加设备状态告警检测**

#### 修改内容
- `monitor_data_task.cpp`：在 `updateFoupInfo` 函数中添加设备状态告警检测
  - 从 ReadFoupStatus 响应中解析 CH_9 设备状态位（vefcStatus, tempHumStatus, humidityReached）
  - 根据 CH_9 位定义触发相应告警：
    - bit0 (vefcStatus): VEFC 异常 → VEFCAbnormal 告警
    - bit1 (tempHumStatus): 温湿度传感器异常 → SH85Abnormal 告警
    - bit2 (humidityReached): 湿度未达标 → HumidityNotReached 告警
  - 状态恢复时自动解决相应告警
  - 告警描述前添加 qrCode 前缀，格式为 `[qrcode: XXXXX]`（XXXXX 为设备编号，范围 12001~12080）
- 添加头文件：`alarmtype.h`, `alarminfo.h`, `alarm_dispatch_task.h`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/alarmpage.h`：移除冗余的 `onNetworkStatusChanged` 槽
  - `OHB80PortMonitor_V_1_0_0/ui/alarmpage.cpp`：DeviceOffline 告警的提交/解决由 NetworkStatusTask 内部统一处理，本页移除重复订阅

---

### 2026-05-14 - Simon
**AlarmDispatchTask 根据告警级别调用不同运行日志方法**

#### 修改内容
- `alarm_dispatch_task.cpp`：在 `submitAlarm` 和 `submitResolve` 函数中根据告警级别调用不同日志方法
  - NoNeed 类型调用 `logWarn()`
  - Error 和 Fatal 级别调用 `logError()`
  - Info 和 Warning 级别调用 `logMessage()`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`

---

### 2026-05-12 - Simon
**Live log 行数上限与裁剪策略调整**

#### 修改内容

**1. OperationLogWidget**
- `kLiveLogMaxRows` 从 500 调整为 2000
- 新增 `kLiveLogTrimBatch = 500`
- 当条数 > 2000 时，一次性 `removeRows(0, 500)` 批量清除最顶部的 500 条最旧记录（替代原来的逐条 `while + removeRow`）

**2. AlarmLogWidget**
- `kLiveLogMaxRows` 从 500 调整为 100
- 当条数 > 100 时，遍历 live log，清除所有 `Is Resolved == Resolved` 或 `NoNeed` 的记录（仅保留 `Unresolved` 未解决告警）
- 从底部向上扫描删除，避免行号偏移

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`

---

### 2026-05-12 - Simon
**日志表格滚动条 handle 默认显示为 hover 色，方便用户感知滚动位置**

#### 修改内容
- 在 `AlarmLogWidget` / `ComunicateLogWidget` / `OperationLogWidget` 的 `enableTouchScroll` lambda 中追加滚动条 stylesheet
- `QScrollBar::handle:vertical/horizontal` 背景色固定为 `#D4D0C8`（与主题 hover 色一致）
- 在原有 styleSheet 后追加而非覆盖，避免影响其他样式

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`

---

### 2026-05-12 - Simon
**ConfigPage / DebugPage 滚动区域支持触屏拖动滚动**

#### 修改内容
- 在 `ConfigPage::initUI()` 和 `DebugPage::initUI()` 中为 `ui->scrollArea->viewport()` 注册 `QScroller::grabGesture(... LeftMouseButtonGesture)`
- 配置 `QScrollerProperties`：
  - `DragStartDistance = 0.005`（更敏感的拖动起始）
  - `OvershootDragResistanceFactor = 0.3`
  - `OvershootScrollDistanceFactor = 0.1`
- 与日志 widget 的触屏滚动配置保持一致

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/configpage.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/debugpage.cpp`

---

### 2026-05-12 - Simon
**表格禁止编辑：统一设置所有日志相关表格为只读模式**

#### 修改内容

**1. TableWidgetManager**
- 在 `addTable()` 方法中添加 `setEditTriggers(QAbstractItemView::NoEditTriggers)`
- 确保所有通过 TableWidgetManager 创建的表格都禁止编辑单元格

**2. AlarmLogWidget**
- `tableViewLiveLog` 添加禁止编辑设置
- `tableViewHistoryLog` 添加禁止编辑设置

**3. ComunicateLogWidget**
- `tableViewLiveLog` 添加禁止编辑设置
- `tableViewHistoryLog` 添加禁止编辑设置

**4. OperationLogWidget**
- `tableViewLiveLog` 添加禁止编辑设置
- `tableViewHistoryLog` 已有禁止编辑设置（无需修改）

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/overheadcranetrack/tablewidgetmanager.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`

---

### 2026-05-10 - Simon
**批量设备任务失败日志拆分：每个失败设备单独写一条运行日志，封装 `logFailedDevice()` 助手**

#### 背景
原有批量设备任务（Read/Set 系列）在 `forceFinish()` 中只写一条合并的失败日志（如 `task failed: 2 succeeded, 5 failed`），无法在 OperationLogWidget 中精确定位是哪些设备失败。同时合并日志只用 `Warn` 级别，颜色不够醒目。

#### 修改内容

**1. ReadVEFCFlowUnitAndMediumStatusTask**
- 拆分合并失败日志：遍历 `m_resultMap` 找出 `commFailed=true` 的设备，每个设备单独写一条 `Error` 级运行日志
- 失败原因细化：`communication failed` / `unit abnormal` / `medium abnormal` / 多个用逗号连接
- 封装 `logFailedDevice(opTask, id, st)` 助手方法

**2. Set 系列任务统一处理（6 个）**
- `SetIdlePurgeTask` —— `SetIdlePurge {property} task failed: device {qr}`
- `SetPurgeFlowTask` —— `SetPurgeFlow flow={x} task failed: device {qr}`
- `SetPneumaticValvePressureTask` —— `SetPneumaticValvePressure {x} bar task failed: device {qr}`
- `SetHumidityOffsetTask` —— `SetHumidityOffset task failed: device {qr}`
- `SetUIRefreshTimeTask` —— 新增完成日志 + `SetUIRefreshTime log={x}s prop={y}s task failed: device {qr}`
- `SetVEFCGasTypeTask` —— 新增完成日志 + `SetVEFCGasType gasType={x} task failed: device {qr}`

**3. 统一模式**
- `.h`：添加 `class OperationDispatchTask;` 前向声明 + `void logFailedDevice(OperationDispatchTask*, const QString&);` 私有方法
- `.cpp`：`forceFinish()` 中根据 `allSuccess` 走分支：
  - 成功 → 写一条 `Message` 级完成日志（保留原合并形式）
  - 失败 → 遍历 `m_failedQrCodes` 调用 `logFailedDevice()` 写 `Error` 级日志
- `set_ui_refresh_time_task` / `set_vefc_gas_type_task` 之前没有 `OperationDispatchTask` 日志接入，新增 `app/shareddata.h` 与 `scheduler/tasks/operation_dispatch_task.h` 的 include

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/read_vefc_flow_unit_medium_status_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_idle_purge_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_purge_flow_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_pneumatic_valve_pressure_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_humidity_offset_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_ui_refresh_time_task.{h,cpp}`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_vefc_gas_type_task.{h,cpp}`

---

### 2026-05-10 - Simon
**OperationLogWidget 日志类型颜色编码：Warn / Error / Fatal 与 ScrollingTipLabel 风格统一**

#### 修改内容
- 新增 `logTypeForegroundColor(int)` 助手函数：
  - `Warn` → `#f5a623`（警报黄色）
  - `Error` / `Fatal` → `#c92a2a`（与 ScrollingTipLabel 报警色一致的暗红）
  - 其他类型返回无效 `QColor`，使用控件默认色
- 在 `onRecordInserted()` 实时日志和历史日志的 `appendRow` / 历史表填充处统一应用前景色（`QStandardItem::setForeground(QBrush)`）

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`

---

### 2026-05-10 - Simon
**修复启动早期运行日志在 OperationLogWidget 和 ScrollingTipLabel 中丢失的问题**

#### 背景
软件启动时，NetworkStatusTask 在 App::initialize() 阶段就开始派发运行日志（如 `[WriteQRCode]`），但此时 UI 控件尚未创建：
- UIDemo6（包含 ScrollingTipLabel）在 App::initialize() 之后才创建
- OperationLogWidget 是懒加载，用户点击公告栏时才创建
导致启动早期的日志信号发出时没有订阅者，日志彻底丢失。

#### 修改内容

**1. SharedData::initScheduler() 调整任务初始化顺序**
- 原顺序：NetworkStatusTask → MonitorDataTask → AlarmDispatchTask → OperationDispatchTask
- 新顺序：OperationDispatchTask → AlarmDispatchTask → NetworkStatusTask → MonitorDataTask
- 原因：NetworkStatusTask::start() 会立即调用 SharedData::getOperationDispatchTask() / getAlarmDispatchTask() 派发日志/告警，必须先创建这两个 dispatcher

**2. OperationDispatchTask 添加环形缓存机制**
- 添加 `m_recentRecords` 环形缓存（最大 500 条）
- 添加 `recentRecords()` 公共方法，线程安全获取最近日志
- 每次 `log()` 写入数据库时同时写入缓存
- 目的：为后续订阅的 UI 控件提供补播数据源

**3. OperationLogWidget::initLiveLog() 补播 backlog**
- 在 connect `operationLogInserted` 信号之前，先调用 `recentRecords()` 获取历史日志
- 遍历 backlog 并调用 `onRecordInserted()` 补播到 live log 表
- 确保用户打开 OperationLogWidget 时能看到启动到此刻的所有日志

**4. UIDemo6::connectTipLabelTask() 补播 backlog**
- 在 connect `operationLogInserted` 信号之前，先取 `recentRecords()` 的最后一条
- 调用 `submitOperationLog()` 喂给 ScrollingTipLabel
- 只取最后一条因为公告栏只显示最新一条操作日志

**5. NetworkStatusTask::start() 主动触发初始已连接设备的日志**
- 设备在信号挂接前可能已经完成连接（异步连接竞态），此时不会再触发 statusChanged 信号
- 在 start() 中检测 `currentStatus == Connected` 时，主动调用 `submitWriteQRCode()` 和记录连接恢复日志
- 使用 `Qt::QueuedConnection` 延迟执行，确保 OperationDispatchTask 和 UI 连接已就绪

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/uidemo6.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`

---

### 2026-05-10 - Simon
**AlarmDispatchTask 启动时恢复未解决警报并通知 UI，新增警报提交/解决运行日志记录**

#### 修改内容
- 在 `AlarmDispatchTask::loadActiveFromDb()` 中，为每个从数据库恢复的未解决警报：
  - 发出 `alarmPublished` 信号，让 ScrollingTipLabel 等订阅者显示恢复的警报
  - 调用 `OperationDispatchTask::logMessage()` 记录运行日志（直接显示警报描述）
- 在 emit 信号前先解锁互斥锁，避免死锁（emit 可能触发回调访问 `m_active`）
- 在 `submitAlarm()` 中添加运行日志记录（警报提交时记录描述）
- 在 `submitResolve()` 中添加运行日志记录（警报解决时记录 `[AlarmResolved] {description}`）
- 新增 `activeAlarms()` 方法：线程安全返回当前活跃警报快照
- `UIDemo6::connectTipLabelTask()` 在 connect `alarmPublished` 信号前，从 `activeAlarms()` 取出活跃警报列表补播给 ScrollingTipLabel，解决启动早期由 `loadActiveFromDb()` 恢复的未解决警报在 UI 上不显示的问题

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/uidemo6.cpp`

---

### 2026-05-10 - Simon
**NetworkStatusTask 新增设备离线告警运行日志记录**

#### 修改内容

**1. 设备离线告警解决时记录运行日志**
- 在 `NetworkStatusTask::onStatusChanged` 中，当设备连接成功并调用 `AlarmDispatchTask::submitResolve` 解决设备离线告警后
- 调用 `OperationDispatchTask::logMessage()` 记录运行日志
- 日志内容：`[DeviceOffline] Device {masterId} connection restored, alarm resolved`

**2. 设备离线告警提交时记录运行日志**
- 在 `NetworkStatusTask::onStatusChanged` 中，当设备从已连接状态跌落并调用 `AlarmDispatchTask::submitAlarm` 提交设备离线告警后
- 调用 `OperationDispatchTask::logError()` 记录错误日志
- 日志内容：`[DeviceOffline] Device {masterId} connection lost, alarm submitted`

**注意**: 后续将警报相关运行日志统一迁移到 AlarmDispatchTask 处理，NetworkStatusTask 不再重复记录。

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`

---

### 2026-05-10 - Simon
**ScrollingTipLabel 滚动算法重构：QPainter 像素级精确滚动 + 无缝循环**

#### 背景
原有滚动实现使用字符级近似滚动，存在以下问题：
1. 字符宽度估算不精确（固定 8 像素），滚动不平滑
2. 使用省略号拼接，视觉效果差
3. 初始先显示静态文本，定时器启动后跳到滚动状态，有闪烁
4. 滚动到末尾后直接重置，没有无缝循环效果

#### 修改内容

**1. 滚动算法重构**
- 使用 QPainter + QPixmap 实现像素级精确滚动
- 绘制两个文本副本实现无缝循环
- 第一个副本位置：xPos = -m_scrollOffset（从左边缘开始向左滚动）
- 第二个副本位置：xPos + m_textWidth + labelWidth/2（紧跟在第一个副本之后）
- 当 offset > textWidth + labelWidth/2 时重置为 0，第二个副本正好到达第一个副本的初始位置
- 滚动条件：文本尾部到达标签中间时，头部从右边缘重新进入
- 文本垂直居中显示

**2. 初始渲染优化**
- 提取 `renderScrollFrame()` 方法用于帧渲染
- `updateDisplay()` 在启动滚动前立即调用 `renderScrollFrame()` 渲染初始帧
- 避免先显示静态文本再跳到滚动状态的闪烁问题

**3. API 更新**
- `submitAlarmLog` 参数从 `QStringList + int` 改为 `AlarmRecord`
- `submitAlarmResolved` 参数从 `int` 改为 `AlarmRecord`
- `submitOperationLog` 参数从 `QStringList` 改为 `OperationRecord`
- 使用复合 key "qrCode|alarmType" 替代主键 ID，避免依赖异步 DB 主键

**4. 数据结构更新**
- `m_alarmQueue` 类型：`QQueue<int>` → `QQueue<QString>`（存储复合 key）
- `m_operationLog` 类型：`QStringList` → `OperationRecord`
- `m_alarmLogs` 类型：`QHash<int, QStringList>` → `QHash<QString, AlarmRecord>`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/scrollingtiplabel/scrollingtiplabel.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/scrollingtiplabel/scrollingtiplabel.cpp`
  - `OHB80PortMonitor_V_1_0_0/docs/api/scrollingtiplabel.md`
  - `OHB80PortMonitor_V_1_0_0/docs/realize/scrollingtiplabel.md`

---

### 2026-05-10 - Simon
**NetworkStatusTask 新增设备离线告警运行日志记录**

#### 修改内容

**1. 设备离线告警解决时记录运行日志**
- 在 `NetworkStatusTask::onStatusChanged` 中，当设备连接成功并调用 `AlarmDispatchTask::submitResolve` 解决设备离线告警后
- 调用 `OperationDispatchTask::logMessage()` 记录运行日志
- 日志内容：`[DeviceOffline] Device {masterId} connection restored, alarm resolved`

**2. 设备离线告警提交时记录运行日志**
- 在 `NetworkStatusTask::onStatusChanged` 中，当设备从已连接状态跌落并调用 `AlarmDispatchTask::submitAlarm` 提交设备离线告警后
- 调用 `OperationDispatchTask::logError()` 记录错误日志
- 日志内容：`[DeviceOffline] Device {masterId} connection lost, alarm submitted`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`

---

### 2026-05-08 - Simon
**新增 SH85SelfChecker：将 SH85 自检功能下沉到 data 层（每个 master 一个自检器）**

#### 背景
为支持多设备并行自检，将原本仅存在于 `SH85SelfCheckTask`（scheduler 层）中的自检流程下沉到 data 层，作为 `ModbusTcpMaster` 的子控件存在。每个 master 自带一个 `SH85SelfChecker`，与 `connector` / `sender` / `periodicSender` 等子控件保持一致的获取方式（`master->selfChecker()`）。

#### 修改内容

**1. 新增 SH85SelfChecker 类**
- 位置：`data/modbustcpmastermanager/modbustcpmaster/sh85selfchecker.{h,cpp}`
- 状态机驱动整套自检流程，无需依赖 SchedulerTask，可被任意线程调用
- 7 个状态：Idle / StartingSelfCheck / WaitingPhase1 / ReadingStatusEarly / WaitingPhase2 / PollingStatus / Done
- 11 种结果：Success / StartCommandFailed / ReadEarlyCommandFailed / DeviceNotEntered / FirmwareAbnormal / ReadPollCommandFailed / HumidityExceeded / SensorCommError / ThresholdParamError / Timeout / Cancelled

**2. 自检流程（共约 70s）**
- 阶段 0：下发 `StartSelfCheck`（`maxRetryCount=0`，不允许超时重发）
  - 失败 → `errorOccurred(StartCommandFailed)`
- 阶段 1：等待 5s，下发 `ReadSelfCheckStatus`
  - 失败 → `errorOccurred(ReadEarlyCommandFailed)`
  - `CH_1 == 0` → `errorOccurred(DeviceNotEntered)`（设备未进入自检功能）
  - `CH_1 != 0 && CH_1 != 1` → `errorOccurred(FirmwareAbnormal)`（底层固件异常）
  - `CH_1 == 1` → 进入阶段 2
- 阶段 2：等待 55s，进入 10s 轮询窗口循环下发 `ReadSelfCheckStatus`
  - 失败 → `errorOccurred(ReadPollCommandFailed)`
  - `CH_1 == 0` → `errorOccurred(FirmwareAbnormal)`
  - `CH_1 == 1` → 继续轮询
  - `CH_1 == 2` → `finished(success=true, Success)`
  - `CH_1 == 3` → `errorOccurred(HumidityExceeded)`
  - `CH_1 == 4` → `errorOccurred(SensorCommError)`
  - `CH_1 == 5` → `errorOccurred(ThresholdParamError)`
- 兜底：10s 轮询窗口超时 → `errorOccurred(Timeout)`

**3. 公共接口**
- `bool start()`：启动自检（前置条件不满足直接返回 false 不发信号）
- `void stop()`：主动取消，仅发出 `finished(Cancelled)`
- `bool isRunning()` / `State currentState()`
- 信号：`started` / `stateChanged` / `errorOccurred` / `finished`

**4. ModbusTcpMaster 集成**
- 构造时创建 `m_selfChecker = new SH85SelfChecker(this, this);`
- 新增 `SH85SelfChecker* selfChecker() const` getter
- 头文件添加 `class SH85SelfChecker;` 前向声明

**5. 实现细节**
- 通过 `ModbusTcpMasterManager::instance().commandPool()` 获取指令模板（与 `SH85SelfCheckTask` 一致）
- 通过 `QMetaObject::invokeMethod(sender, ..., Qt::QueuedConnection)` 跨线程提交指令
- 通过 `m_pendingUuid` 过滤 `commandFinished` 信号，确保只处理自身下发的响应
- 三个 `QTimer` 单次触发实现 5s/55s/10s 阶段时序
- `cleanup()` 在结束 / 取消时统一停止定时器并断开 sender 连接

#### 影响范围
- 新增文件：
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/sh85selfchecker.cpp`
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbustcpmaster.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbustcpmaster.pri`

---

### 2026-05-08 - Simon
**FoupOfOHBInfo 类重构：成员变量私有化 + Enable 机制**

#### 修改内容

**1. FoupOfOHBInfo 类封装重构**
- 将所有成员变量从 public 移到 private 区域
- 所有成员变量添加 m_ 前缀（m_qrCode、m_portId、m_ip 等）
- 为所有 19 个变量添加 getter 方法（如 qrCode()、portId() 等）
- 为所有变量添加 setter 方法（如 setQrCode()、setPortId() 等）
- 更新构造函数、拷贝构造函数和赋值运算符使用 m_ 前缀变量

**2. Enable 机制**
- 新增 m_enable 成员变量（bool 类型，true=可用，false=不可用）
- 当 m_enable == false 时，大部分 getter 方法返回 0 或空值
  - 数值类型（int、double、quint16、quint32）返回 0
  - bool 类型返回 false
  - QString 类型返回 ""
  - QTime 类型返回 QTime(0, 0, 0) 即 00:00:00
  - IdleState 类型返回 IdleState::Stopped
- 以下四个 getter 方法不受 enable 影响，始终返回实际值：
  - qrCode()、portId()、ip()、port()
- enable() 方法始终返回实际值，用于查询禁用状态

**3. FrameDevice Disable 状态**
- 在 DeviceStatus 枚举中新增 Disable 状态
- StatusColorMap 中添加 Disable 对应的灰色映射（QColor(128, 128, 128)）
- Disable 状态字体颜色设为白色，与灰色背景形成对比
- updateFoupInfo() 方法中添加 enable 检测逻辑
  - 当 Foup 类型且 enable() == false 时，设置状态为 Disable

**4. 全局更新所有使用 FoupOfOHBInfo 的文件**
- framedevice.cpp：所有直接访问改为 getter/setter 方法
- monitor_data_task.cpp：所有赋值改为 setter，读取改为 getter
- network_status_task.cpp：hasAlarm、alarmId、qrCode、startTime 改为 getter/setter
- devicemonitorwidget.cpp：所有字段访问改为 getter 方法
- setofohbinfo.cpp：qrCode、inletPressure、RH 改为 getter 方法
- shareddata.cpp：初始化时使用 setter 方法，读取时使用 getter 方法

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/classes/foupofohbinfo.h`
  - `OHB80PortMonitor_V_1_0_0/classes/foupofohbinfo.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/overheadcranetrack/framedevice.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/overheadcranetrack/framedevice.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/overheadcranetrack/devicemonitorwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/overheadcranetrack/cranemapwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/classes/setofohbinfo.cpp`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`

---

### 2026-05-08 - Simon
**Record 类型重构完成：DBCon 查询接口与 UI Widgets 全面升级**

#### 修改内容

**1. DBCon 查询接口返回类型更新**
- `AlarmLogDBCon::queryPageWithConditions` 返回 `QList<AlarmRecord>` 而非 `QList<QVariantMap>`
- `OperationLogDBCon::queryPagination/queryPaginationInRange/queryPageWithConditions` 返回 `QList<OperationRecord>`
- `CommunicateLogDBCon::queryPageWithConditions` 返回 `QList<CommunicateRecord>`
- `DeviceParamLogDBCon::queryPageWithConditions` 返回 `QList<DeviceParamRecord>`
- DBCon 层负责 QVariantMap → Record 转换

**2. 查询任务信号更新**
- `AlarmLogQueryTask::pageWithConditionsResult` 信号返回 `QList<AlarmRecord>`
- `OperationLogQueryTask::currentPageResult` 信号返回 `QList<OperationRecord>`
- `CommunicateLogQueryTask::pageWithConditionsResult` 信号返回 `QList<CommunicateRecord>`

**3. UI Widgets 全面更新**
- `AlarmLogWidget` 使用 `AlarmRecord` 类型
  - `onPageWithConditionsResult(const QList<AlarmRecord>&)`
  - `onRecordInserted(const AlarmRecord&)`
  - `setHistoryLogData(const QList<AlarmRecord>&)`
  - `loadUnresolvedToLiveLog()` 查询接口更新
- `OperationLogWidget` 使用 `OperationRecord` 类型
  - `onCurrentPageResult(const QList<OperationRecord>&)`
  - `onRecordInserted(const OperationRecord&)`
  - `setHistoryLogData(const QList<OperationRecord>&)`
- `ComunicateLogWidget` 使用 `CommunicateRecord` 类型
  - `onPageWithConditionsResult(const QList<CommunicateRecord>&)`
  - `setHistoryLogData(const QList<CommunicateRecord>&)`

**4. AlarmDispatchTask 修复**
- `loadActiveFromDb()` 方法更新：`QList<QVariantMap>` → `QList<AlarmRecord>`
- 字段访问方式：`row.value("xxx")` → `row.xxx`

**5. Record 类注释优化**
- `operationrecord.h` - 添加清晰字段注释（主键、日志类型、操作描述、用户权限等）
- `deviceparamrecord.h` - 添加清晰字段注释（设备二维码、记录时间、各压力/流量参数、FOUP 状态等）
- `communicaterecord.h` - 添加清晰字段注释（发送/响应时间、命令ID、执行状态、重试次数、帧数据等）
- 所有注释格式统一，包含字段含义、时间格式、枚举值说明

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarmlogquerytask.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarmlogquerytask.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operationlogquerytask.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operationlogquerytask.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/communicatelogquerytask.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/communicatelogquerytask.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/operationlogwidget/operationlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/comunicatelogwidget/comunicatelogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/classes/operationrecord.h`
  - `OHB80PortMonitor_V_1_0_0/classes/deviceparamrecord.h`
  - `OHB80PortMonitor_V_1_0_0/classes/communicaterecord.h`

---

### 2026-05-08 - Simon
**架构调整：引入 Record 类型，替代 QVariantMap 传递数据库记录**

#### 背景
为提高类型安全性和代码可读性，为四个数据库表（alarm_log / operation_log / communicate_log / device_param_log）引入对应的 Record 类（AlarmRecord / OperationRecord / CommunicateRecord / DeviceParamRecord）。这些 Record 类与数据库表结构严格对齐，替代原有的 QVariantMap 传递方式。

#### 修改内容

**1. 创建 Record 类**
- 新增 `classes/alarmrecord.h` - 与 alarm_log 表对齐
- 新增 `classes/operationrecord.h` - 与 operation_log 表对齐
- 新增 `classes/communicaterecord.h` - 与 communicate_log 表对齐
- 新增 `classes/deviceparamrecord.h` - 与 device_param_log 表对齐
- 所有 Record 类包含 reset() 方法，使用 Q_DECLARE_METATYPE 注册

**2. AlarmInfo 重构**
- AlarmInfo 现在包含 AlarmRecord 作为成员变量
- 与数据库表重合的字段（id/alarmLevel/alarmType/qrCode/occurTime/resolveTime/isResolved/description/userPermission）全部由 record 持有
- 业务字段（alarmSource/alarmId）保留在 AlarmInfo
- 访问方式：info.record.alarmLevel / info.record.qrCode 等
- 更新 generateAlarmId() 使用 record 字段

**3. AlarmDispatchTask 更新**
- normalize() / submitAlarm() / submitResolve() 使用 info.record.xxx 访问字段
- alarmLogInserted 信号改为携带 AlarmRecord 而非单独字段

**4. OperationDispatchTask 更新**
- log() 方法构造 OperationRecord 并发出 operationLogInserted 信号

**5. DBCon 信号层改造**
- AlarmLogDBCon::recordInserted 信号改为携带 AlarmRecord
- OperationLogDBCon::recordInserted 信号改为携带 OperationRecord
- CommunicateLogDBCon::recordInserted 信号改为携带 CommunicateRecord
- DeviceParamLogDBCon 新增 recordInserted 信号携带 DeviceParamRecord
- onWriteTaskCompleted() 实现改为构造 Record 并发出

**6. UIDemo6 更新**
- connectTipLabelTask() 中信号处理改为从 Record 提取字段

**7. NetworkStatusTask 更新**
- 使用 info.record.xxx 访问 AlarmInfo 字段

**8. metatypes 注册**
- 注册 AlarmRecord / OperationRecord / CommunicateRecord / DeviceParamRecord
- 注册 QList<AlarmRecord> / QList<OperationRecord> / QList<CommunicateRecord> / QList<DeviceParamRecord>
- 注册 AlarmInfo

**9. classes.pri 更新**
- 添加 4 个 Record 头文件

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/classes/alarmrecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/operationrecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/communicaterecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/deviceparamrecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/alarminfo.h`
  - `OHB80PortMonitor_V_1_0_0/classes/alarminfo.cpp`
  - `OHB80PortMonitor_V_1_0_0/classes/classes.pri`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/uidemo6.cpp`
  - `OHB80PortMonitor_V_1_0_0/app/metatypes.cpp`

---

### 2026-05-08 - Simon
**架构调整：移除 TipLabelTask，直接由 OperationDispatchTask 和 AlarmDispatchTask 发出信号**

#### 背景
为简化架构，移除中间层 TipLabelTask，改为直接由 OperationDispatchTask 和 AlarmDispatchTask 在数据库插入完成后发出信号，供 UIDemo6 的 ScrollingTipLabel 接收显示。

#### 修改内容

**1. 删除 TipLabelTask**
- 删除 `scheduler/tasks/tip_label_task.{h,cpp}` 文件
- 从 `scheduler/scheduler.pri` 中移除 TipLabelTask 引用

**2. OperationDispatchTask 添加信号**
- 新增信号：`operationLogInserted(const QString& occurTime, int logType, const QString& message)`
- 在 `log()` 方法中插入数据库后发出该信号

**3. AlarmDispatchTask 添加信号**
- 新增信号：`alarmLogInserted(const QString& occurTime, int alarmLevel, const QString& alarmType, const QString& description)`
- 在 `persistInsert()` 方法中插入数据库后发出该信号

**4. UIDemo6 连接信号**
- 修改 `connectTipLabelTask()` 方法，连接 OperationDispatchTask 和 AlarmDispatchTask 的信号
- 将信号转换为 ScrollingTipLabel 需要的格式并调用对应方法

**5. SharedData 清理**
- 移除 TipLabelTask 相关的头文件引用和静态成员
- 移除 `getTipLabelTask()` 方法

**6. MonitorDataTask 清理**
- 移除 TipLabelTask 头文件引用
- 移除对 TipLabelTask 的调用

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/tip_label_task.h`（删除）
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/tip_label_task.cpp`（删除）
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/scheduler.pri`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.h`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/uidemo6.cpp`

---

### 2026-05-08 - Simon
**架构调整：引入 Record 类型，替代 QVariantMap 传递数据库记录**

#### 背景
为提高类型安全性和代码可读性，为四个数据库表（alarm_log / operation_log / communicate_log / device_param_log）引入对应的 Record 类（AlarmRecord / OperationRecord / CommunicateRecord / DeviceParamRecord）。这些 Record 类与数据库表结构严格对齐，替代原有的 QVariantMap 传递方式。

#### 修改内容

**1. 创建 Record 类**
- 新增 `classes/alarmrecord.h` - 与 alarm_log 表对齐
- 新增 `classes/operationrecord.h` - 与 operation_log 表对齐
- 新增 `classes/communicaterecord.h` - 与 communicate_log 表对齐
- 新增 `classes/deviceparamrecord.h` - 与 device_param_log 表对齐
- 所有 Record 类包含 reset() 方法，使用 Q_DECLARE_METATYPE 注册

**2. AlarmInfo 重构**
- AlarmInfo 现在包含 AlarmRecord 作为成员变量
- 与数据库表重合的字段（id/alarmLevel/alarmType/qrCode/occurTime/resolveTime/isResolved/description/userPermission）全部由 record 持有
- 业务字段（alarmSource/alarmId）保留在 AlarmInfo
- 访问方式：info.record.alarmLevel / info.record.qrCode 等
- 更新 generateAlarmId() 使用 record 字段

**3. AlarmDispatchTask 更新**
- normalize() / submitAlarm() / submitResolve() 使用 info.record.xxx 访问字段
- alarmLogInserted 信号改为携带 AlarmRecord 而非单独字段

**4. OperationDispatchTask 更新**
- log() 方法构造 OperationRecord 并发出 operationLogInserted 信号

**5. DBCon 信号层改造**
- AlarmLogDBCon::recordInserted 信号改为携带 AlarmRecord
- OperationLogDBCon::recordInserted 信号改为携带 OperationRecord
- CommunicateLogDBCon::recordInserted 信号改为携带 CommunicateRecord
- DeviceParamLogDBCon 新增 recordInserted 信号携带 DeviceParamRecord
- onWriteTaskCompleted() 实现改为构造 Record 并发出

**6. UIDemo6 更新**
- connectTipLabelTask() 中信号处理改为从 Record 提取字段

**7. NetworkStatusTask 更新**
- 使用 info.record.xxx 访问 AlarmInfo 字段

**8. metatypes 注册**
- 注册 AlarmRecord / OperationRecord / CommunicateRecord / DeviceParamRecord
- 注册 QList<AlarmRecord> / QList<OperationRecord> / QList<CommunicateRecord> / QList<DeviceParamRecord>
- 注册 AlarmInfo

**9. classes.pri 更新**
- 添加 4 个 Record 头文件

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/classes/alarmrecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/operationrecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/communicaterecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/deviceparamrecord.h`（新增）
  - `OHB80PortMonitor_V_1_0_0/classes/alarminfo.h`
  - `OHB80PortMonitor_V_1_0_0/classes/alarminfo.cpp`
  - `OHB80PortMonitor_V_1_0_0/classes/classes.pri`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/operationlogdb/operationlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/communicatelogdb/communicatelogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/deviceparamlogdb/deviceparamlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/uidemo6.cpp`
  - `OHB80PortMonitor_V_1_0_0/app/metatypes.cpp`

#### 待完成
- 查询任务（AlarmLogQueryTask/OperationLogQueryTask/CommunicateLogQueryTask）返回 QList<Record> 而非 QList<QVariantMap）✓
- DBCon 查询接口返回 QList<Record> 而非 QList<QVariantMap> ✓
- UI widgets（AlarmLogWidget/OperationLogWidget/ComunicateLogWidget）使用 Record 类型 ✓

---

### 2026-05-08 - Simon
**命名统一：OperationLogDispatchTask 重命名为 OperationDispatchTask**

#### 背景
为与 `AlarmDispatchTask` 命名方式保持一致，去掉 `OperationLogDispatchTask` 中的 `log` 后缀，统一为 `OperationDispatchTask`。

#### 修改内容

**1. 文件重命名**
- `scheduler/tasks/operation_log_dispatch_task.h` → `scheduler/tasks/operation_dispatch_task.h`
- `scheduler/tasks/operation_log_dispatch_task.cpp` → `scheduler/tasks/operation_dispatch_task.cpp`

**2. 类名更新**
- 类名：`OperationLogDispatchTask` → `OperationDispatchTask`
- 宏定义：`OPERATION_LOG_DISPATCH_TASK_H` → `OPERATION_DISPATCH_TASK_H`
- taskType() 返回值：`"OperationLogDispatchTask"` → `"OperationDispatchTask"`
- 调试输出：`[OperationLogDispatchTask]` → `[OperationDispatchTask]`

**3. SharedData 接口更新**
- 函数名：`getOperationLogDispatchTask()` → `getOperationDispatchTask()`
- 静态成员：`s_operationLogDispatchTask` → `s_operationDispatchTask`
- 注释：`操作日志调度任务` → `操作调度任务`

**4. 构建文件更新**
- `scheduler/scheduler.pri`：更新头文件和源文件引用路径

**5. 全局引用更新**
- 所有 include 语句：`#include "scheduler/tasks/operation_log_dispatch_task.h"` → `#include "scheduler/tasks/operation_dispatch_task.h"`
- 所有函数调用：`SharedData::getOperationLogDispatchTask()` → `SharedData::getOperationDispatchTask()`
- 所有类型声明：`OperationLogDispatchTask*` → `OperationDispatchTask*`

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/scheduler.pri`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.h`
  - `OHB80PortMonitor_V_1_0_0/app/shareddata.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/monitor_data_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/network_status_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/logindialog.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/changepassworddialog.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/useraccountlabel/useraccountlabel.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/humidityoffsetsettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/idlepurgesettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/purgeflowsettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/pneumaticvalvepressuresettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/configsettingwidget/sh85selfchecksettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/uirefreshtimesettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/vefcflowunitmediumstatuswidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/debugsettingwidget/vefcgastypesettingwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/customwidget.pri`
  - `README.md`

---

### 2026-05-07 - Simon
**移除 alarm_log.customer_visible 字段（全链路清理）**

#### 背景
本次移除 `customer_visible` 字段，保留 2026-05-06 日志系统重构时新增的 `user_permission` 字段（INTEGER NOT NULL DEFAULT 0）。

#### 修改内容

**1. SQL 文件（x32 + x64 同步）**
- `create_operation_log.sql`：从 `CREATE TABLE alarm_log` 移除 `customer_visible` 列；移除 `idx_alarm_log_customer_visible` 索引；末尾追加 `ALTER TABLE alarm_log DROP COLUMN customer_visible;` 老库降级迁移
- `alarm_log_queries.sql`：`query_page_with_conditions` / `query_total_count_with_conditions` 删除 `(? IS NULL OR customer_visible = ?)` 条件行（参数 15→13、13→11）；`insert_record` 删除列名与一个 `?`（9→8）

**2. C++ 数据访问层**
- `data/logdatabases/alarmlogdb/alarmlogdbcon.{h,cpp}`：`insertRecord` / `queryPageWithConditions` / `queryTotalCountWithConditions` 移除 `int customerVisible` 参数；`onWriteTaskCompleted` 中 `row` 不再写 `customer_visible`，params 长度校验 9→8、`user_permission` 索引 8→7
- `data/logdatabases/alarmlogdb/alarmlogsqllogic.{h,cpp}`：同步删除参数、`addBindValue` 两次 / `params << customerVisible`

**3. 业务逻辑层**
- `classes/alarminfo.h`：删除 `bool customerVisible` 字段、构造初始化、`reset()`
- `app/alarmtype.h`：删除 `alarmTypeToCustomerVisible()` 函数
- `scheduler/tasks/alarm_dispatch_task.{h,cpp}`：删除推导赋值、`loadActiveFromDb` 读取、`persistInsert` 透传、头文件文档说明
- `scheduler/tasks/alarmlogquerytask.{h,cpp}`：删除 `setCustomerVisible` / `m_customerVisible`、查询调用入参

**4. UI 层**
- `ui/customwidget/alarmlogwidget/alarmlogwidget.{h,cpp}`：删除 `setCustomerVisibleFilter` / `customerVisibleFilter` / `m_customerVisibleFilter`、表头 "Customer Visible" 列、live log 与 history log 渲染、查询入参
- `ui/customwidget/alarmloggerwidget/alarmloggerwidget.cpp`：`insertRecord` 调用移除 `customer_visible` 实参

**5. 文档**
- `docs/api/log_databases_user_permission.md`：`AlarmLogDBCon::insertRecord` 示例同步
- `docs/realize/log_databases_user_permission.md`：`alarm_log` ER 图同步

#### 影响范围
- 修改文件：
  - `OHB80PortMonitor_V_1_0_0/bin/x32/databases/create_operation_log.sql`
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/create_operation_log.sql`
  - `OHB80PortMonitor_V_1_0_0/bin/x32/databases/alarm_log_queries.sql`
  - `OHB80PortMonitor_V_1_0_0/bin/x64/databases/alarm_log_queries.sql`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogdbcon.cpp`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogsqllogic.h`
  - `OHB80PortMonitor_V_1_0_0/data/logdatabases/alarmlogdb/alarmlogsqllogic.cpp`
  - `OHB80PortMonitor_V_1_0_0/classes/alarminfo.h`
  - `OHB80PortMonitor_V_1_0_0/app/alarmtype.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_dispatch_task.cpp`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarmlogquerytask.h`
  - `OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarmlogquerytask.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.h`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmlogwidget/alarmlogwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/ui/customwidget/alarmloggerwidget/alarmloggerwidget.cpp`
  - `OHB80PortMonitor_V_1_0_0/docs/api/log_databases_user_permission.md`
  - `OHB80PortMonitor_V_1_0_0/docs/realize/log_databases_user_permission.md`

---

### 2026-05-07 - Simon
**新增 ScrollingTipLabel 控件与 TipLabelTask 调度任务**

#### 修改内容

**1. 新增 ScrollingTipLabel 控件**
- 位置：`ui/customwidget/scrollingtiplabel/`
- 功能：实时显示系统操作日志和警报日志
- 继承自 QLabel，支持滚动显示消息

**2. 核心功能**
- 警报优先显示：警报队列不空时优先显示最后一个警报
- 消息消除机制：通过警报记录 ID 或操作日志替换实现消息更新
- 滚动显示：文本长度超过控件宽度时自动滚动显示
- 自动隐藏：无消息时自动隐藏控件，有消息时自动显示

**3. 样式切换**
- 警报模式：红色背景（#fff5f5）、红色边框（#ff6b6b）、红色文字（#c92a2a）、加粗字体
- 普通模式：浅灰色背景（#f8f9fa）、灰色边框（#dee2e6）、深灰色文字（#495057）
- 样式根据当前显示的消息类型自动切换

**4. 公开接口**
- `submitAlarmLog(const QStringList& operationLog, int alarmRecordId)` - 提交警报日志
- `submitAlarmResolved(int alarmRecordId)` - 提交警报已解决
- `submitOperationLog(const QStringList& operationLog)` - 提交操作日志

**5. 新增 TipLabelTask 调度任务**
- 位置：`scheduler/tasks/tip_label_task.h`、`tip_label_task.cpp`
- 功能：为 ScrollingTipLabel 提供生产者-消费者模式的日志采集任务
- 继承自 SchedulerTask，运行在调度器线程中
- 提供与 ScrollingTipLabel 相同的公开接口（submitAlarmLog / submitAlarmResolved / submitOperationLog）
- 内部队列线程安全，支持任意线程调用
- 使用无锁 swap 技巧减少锁持有时间：O(1) 交换 + 无锁处理
- 通过信号将记录转发给 ScrollingTipLabel，调用方无需知道 UI 控件的存在

**6. TipLabelTask 公开接口**
- `submitAlarmLog(const QStringList& operationLog, int alarmRecordId)` - 提交警报日志（线程安全）
- `submitAlarmResolved(int alarmRecordId)` - 提交警报已解决（线程安全）
- `submitOperationLog(const QStringList& operationLog)` - 提交操作日志（线程安全）

**7. TipLabelTask 信号**
- `alarmLogReady(QStringList, int)` - 警报日志就绪
- `alarmResolvedReady(int)` - 警报已解决
- `operationLogReady(QStringList)` - 操作日志就绪

**8. 使用方式**
- 业务侧调用：`SharedData::getTipLabelTask()->submitXxx(...)`
- UI 端连接：`UIDemo6::connectTipLabelTask()` 将信号连接到 `scrollingTipLabel`

#### 影响范围
- 新增文件：
  - `ui/customwidget/scrollingtiplabel/scrollingtiplabel.h`
  - `ui/customwidget/scrollingtiplabel/scrollingtiplabel.cpp`
  - `ui/customwidget/scrollingtiplabel/scrollingtiplabel.pri`
  - `docs/api/scrollingtiplabel.md`
  - `docs/realize/scrollingtiplabel.md`
  - `scheduler/tasks/tip_label_task.h`
  - `scheduler/tasks/tip_label_task.cpp`
  - `docs/api/tip_label_task.md`
  - `docs/realize/tip_label_task.md`
- 修改文件：
  - `ui/customwidget/customwidget.pri`（添加 scrollingtiplabel 模块引用）
  - `scheduler/scheduler.pri`（添加 tip_label_task 文件）
  - `app/shareddata.h`、`app/shareddata.cpp`（注册 TipLabelTask 到调度器）
  - `ui/uidemo6.h`、`ui/uidemo6.cpp`（添加 connectTipLabelTask 方法）

---

### 2026-05-06 - Simon
**日志系统重构：以 SQLite 数据库替换三套旧 CSV 日志模块**

#### 背景与动机

旧三套日志模块（`AlarmLoggerWidget`、`CommunicateLoggerWidget`、`RunningLoggerWidget`/`LoggerWidget`）均基于 CSV 文件存储，存在以下问题：

| 问题 | 说明 |
|---|---|
| 查询性能差 | 每次查询全量扫描 CSV，无索引支持，数据量大时翻页极慢 |
| 历史范围查询难 | 旧模块按天分文件，跨天查询需读取并合并多个文件 |
| 数据安全性低 | CSV 无事务保护，多线程并发写入易导致文件损坏 |
| 结构分散难维护 | 三套 CSV 格式各异，代码重复，后续扩展字段成本高 |

#### 修改内容

**1. 新增 `data/logdatabases/` 数据层子系统**

共享基础组件（均在 `LogDB` 命名空间下）：

- **`DatabaseManager`**（单例）：统一管理全部数据库连接的初始化与清理，提供 4 个 DBCon 的访问入口
- **`WriteSqlDBCon`**：专用写入线程 + 阻塞任务队列，所有日志模块共享同一写连接，保证写操作串行无竞争
- **`DBConnectionHelper`**：SQLite 连接工具，默认启用 WAL 模式 + `synchronous=NORMAL` + `busyTimeout=5000ms`，读写性能与数据安全兼顾
- **`SqlMapper`**：从外部 `.sql` 文件按 `-- ID=xxx` 注释解析 SQL 语句，通过 ID 字符串映射，彻底解耦硬编码 SQL
- **`LogCleanupScheduler`**：通用定期清理调度器，每分钟检查一次，按可配置月份阈值（`retainMonths`）触发删除旧数据（`cleanupMonths`）
- **`dbtypes.h`**：跨模块公共类型（`SortOrder`、`OperationLogType`、`WriteOp`、`WriteResult`）

**2. 四个日志 DB 模块（每模块各含 SqlLogic + DBCon 两层）**

| 新模块 | 替换旧模块 | 数据库表 | 关键字段 |
|---|---|---|---|
| `AlarmLogDBCon` | `AlarmLoggerWidget`（CSV） | `alarm_log` | alarm_level / occur_time / qr_code / alarm_type / is_resolved / resolve_time / customer_visible / description |
| `CommunicateLogDBCon` | `CommunicateLoggerWidget`（CSV） | `communicate_log` | send_time / response_time / command_id / qr_code / exec_status / retry_count / send_frame / response_frame / description |
| `OperationLogDBCon` | `RunningLoggerWidget`/`LoggerWidget`（CSV） | `operation_log` | occur_time / log_type / description |
| `DeviceParamLogDBCon` | 无旧模块（新增） | `device_param_log` | qr_code / record_time / inlet_pressure / outlet_pressure / inlet_flow / humidity / temperature / foup_status |

所有表均配套 `log_record_count` 计数缓存表，`queryTotalCount()` 直接读缓存，避免对大表做 `COUNT(*)`。

`AlarmLogDBCon` 额外支持 `updateResolve()` 接口及对应 `recordResolved` 信号，实现警报原位已解决标记。

**3. Scheduler 层 — 四个新任务**

- **`AlarmLogQueryTask`**：警报日志多条件（级别 / qrCode / 类型 / 是否解决 / 客户可见 / 时间区间）分页查询任务
- **`CommunicateLogQueryTask`**：通讯日志多条件（commandId / qrCode / 执行状态 / 重试次数 / 时间区间 / 排序方向）分页查询任务
- **`OperationLogQueryTask`**：运行日志范围 + 条件分页查询任务，支持关键词高亮、全局顺序号显示、跨页 Pre/Next 跳转
- **`OperationDispatchTask`**：操作调度常驻任务，取代旧 `RunningLoggerCollector`；业务侧任意线程调用 `logMessage / logWarn / logError` 直接落库，无需独立缓冲队列

**4. UI 层 — 三个新控件替换旧控件**

- **`AlarmLogWidget`**：警报日志控件，含实时表（订阅 `recordInserted`/`recordResolved`）+ 历史条件查询 + 分页；支持启动时加载上次未解决警报
- **`ComunicateLogWidget`**：通讯日志控件，含实时表（固定行，按 qrcode 更新）+ 历史条件查询 + 分页
- **`OperationLogWidget`**：运行日志控件，含实时表 + 历史高级查询（时间范围 / 日志类型 / 关键词 / 命中行高亮 / 上一条下一条跨页跳转）

> 旧模块（`AlarmLoggerWidget`、`CommunicateLoggerWidget`、`RunningLoggerWidget`/`LoggerWidget`）代码仍保留在工程中，但已不再集成使用。

**5. SQL 文件（`bin/x32/databases/` 与 `bin/x64/databases/`）**

- `create_operation_log.sql`：建表语句（4 张日志表 + `log_record_count` 计数缓存表 + 触发器）
- `alarm_log_queries.sql`：警报日志查询语句集（分页条件查询 / 总数 / 插入 / 删除 / update_resolve / 时间边界）
- `communicate_log_queries.sql`：通讯日志查询语句集
- `operation_log_queries.sql`：运行日志查询语句集（含 prev/next 匹配 ID、首条定位等高级查询）
- `device_param_log_queries.sql`：设备参数日志查询语句集

#### 技术细节
- **WAL 模式**：读写并发不互锁，读操作不阻塞写，写操作不阻塞读
- **写入线程隔离**：`WriteSqlDBCon` 占一条写专用连接，各 SqlLogic 各持一条只读连接，完全隔离
- **SQL 外置**：所有查询语句外置到 `.sql` 文件，通过 `SqlMapper` 按 ID 索引加载，便于独立调试和修改
- **计数缓存**：通过数据库触发器维护 `log_record_count`，`queryTotalCount()` O(1) 查表无需全表扫描
- **定期清理**：每个 SqlLogic 内置 `LogCleanupScheduler`，默认保留 12 个月，超限时一次删除最早 3 个月

#### 新增文件
- `data/logdatabases/databasemanager.h`、`.cpp`、`dbtypes.h`、`dbconnectionhelper.h`、`.cpp`、`sqlmapper.h`、`.cpp`、`logcleanupscheduler.h`、`.cpp`、`logdatabases.pri`
- `data/logdatabases/alarmlogdb/alarmlogsqllogic.h`、`.cpp`、`alarmlogdbcon.h`、`.cpp`、`alarmlogdb.pri`
- `data/logdatabases/communicatelogdb/communicatelogsqllogic.h`、`.cpp`、`communicatelogdbcon.h`、`.cpp`、`communicatelogdb.pri`
- `data/logdatabases/operationlogdb/operationlogsqllogic.h`、`.cpp`、`operationlogdbcon.h`、`.cpp`、`operationlogdb.pri`
- `data/logdatabases/deviceparamlogdb/deviceparamlogsqllogic.h`、`.cpp`、`deviceparamlogdbcon.h`、`.cpp`、`deviceparamlogdb.pri`
- `data/logdatabases/writesqldb/writesqldbcon.h`、`.cpp`、`writesqldb.pri`
- `scheduler/tasks/alarmlogquerytask.h`、`.cpp`
- `scheduler/tasks/communicatelogquerytask.h`、`.cpp`
- `scheduler/tasks/operationlogquerytask.h`、`.cpp`
- `scheduler/tasks/running_logger_task.h`、`.cpp`
- `ui/customwidget/alarmlogwidget/alarmlogwidget.h`、`.cpp`、`.ui`、`alarmlogwidget.pri`
- `ui/customwidget/comunicatelogwidget/comunicatelogwidget.h`、`.cpp`、`.ui`、`comunicatelogwidget.pri`
- `ui/customwidget/operationlogwidget/operationlogwidget.h`、`.cpp`、`.ui`、`operationlogwidget.pri`
- `bin/x32/databases/create_operation_log.sql`、`alarm_log_queries.sql`、`communicate_log_queries.sql`、`operation_log_queries.sql`、`device_param_log_queries.sql`

---

### 2026-04-30 15:17 - Simon
**功能实现：Purge 流量/VEFC 气体类型/UI 刷新时间/VEFC 状态读取**

#### 修改内容

**1. 设置充气流量功能**
- 新增 `PurgeFlowSettingWidget` UI 控件（ConfigPage）
  - Target Device QRCode SpinBox（设备选择）
  - Purge Flow SpinBox（流量值，整数）+ Set / Set All 按钮
  - Set：仅对选中设备生效
  - Set All：对所有设备生效
- 新增 `SetPurgeFlowTask` 调度任务
  - 底层指令：`WritePurgeFlow`（FC 0x06, addr 0x0000）
  - 寄存器值 = flow × 100（例：35 → 3500）
  - 仅 FOUP IN 时有效，FOUP OUT 状态固定为 0
  - 信号：`allFinished(bool allSuccess, int successCount, QStringList failedQrCodes, int flowValue)`

**2. 设置 VEFC 气体介质类型功能**
- 新增 `VEFCGasTypeSettingWidget` UI 控件（DebugPage）
  - Target Device QRCode SpinBox（设备选择）
  - Gas Type ComboBox（CDA/N2/Ar/CO2/O2）+ Set / Set All 按钮
- 新增 `SetVEFCGasTypeTask` 调度任务
  - 底层指令：`WriteVEFCGasType`（FC 0x06, addr 0x0001，掉电保持）
  - 气体类型枚举：CDA=0x0000, N2=0x0001, Ar=0x0002, CO2=0x0003, O2=0x0004
  - 信号：`allFinished(bool allSuccess, int successCount, QStringList failedQrCodes, int gasType)`
- 初始下发器配置：在 `InitialCommands` 中添加 `WriteVEFCGasType` 指令

**3. 设置下位机 UI 屏幕刷新时长功能**
- 新增 `UIRefreshTimeSettingWidget` UI 控件（DebugPage）
  - Target Device QRCode SpinBox（设备选择）
  - Log Screen Duration SpinBox（log 界面时长，秒）
  - Property Screen Duration SpinBox（属性界面时长，秒）
  - + Set / Set All 按钮
- 新增 `SetUIRefreshTimeTask` 调度任务
  - 底层指令：`WriteUIRefreshTime`（FC 0x10, addr 0x0004，2 寄存器，4 字节）
  - 数据布局（大端）：[0..1] logScreenSec, [2..3] propertyScreenSec
  - 信号：`allFinished(bool allSuccess, int successCount, QStringList failedQrCodes, int logScreenSec, int propertyScreenSec)`

**4. 读取 VEFC 流量单位以及介质配置状态功能**
- 新增 `VEFCFlowUnitMediumStatusWidget` UI 控件（DebugPage）
  - Target Device QRCode SpinBox（设备选择）
  - Read / Read All 按钮
  - 读取结果弹窗提示：全部成功或逐行列出失败设备
- 新增 `ReadVEFCFlowUnitAndMediumStatusTask` 调度任务
  - 底层指令：`ReadVEFCFlowUnitAndMediumStatus`（FC 0x04, addr 0x0011）
  - 响应 2 字节寄存器：
    - hi_byte: 0=单位配置成功 / 1=单位配置失败（默认 L/Min）
    - lo_byte: 0=介质配置成功 / 1=介质配置失败（默认 CDA）
  - 信号：`allFinished(bool allSuccess, int successCount, QList<DeviceStatus> results)`
  - DeviceStatus 结构：qrcode, commFailed, unitOk, mediumOk, unitRaw, mediumRaw

#### 技术细节
- **任务模式**：所有设置任务支持单设备（Set）和全设备（Set All）两种模式
- **倍率转换**：Purge 流量值需乘以 100 写入寄存器
- **掉电保持**：VEFC 气体介质类型配置掉电保持，放入初始下发器
- **状态反馈**：VEFC 状态读取任务返回每台设备的详细状态（通信/单位/介质）

#### 影响范围
- 新增文件：`ui/customwidget/configsettingwidget/purgeflowsettingwidget.h`、`.cpp`
- 新增文件：`scheduler/tasks/set_purge_flow_task.h`、`.cpp`
- 新增文件：`ui/customwidget/debugsettingwidget/vefcgastypesettingwidget.h`、`.cpp`
- 新增文件：`scheduler/tasks/set_vefc_gas_type_task.h`、`.cpp`
- 新增文件：`ui/customwidget/debugsettingwidget/uirefreshtimesettingwidget.h`、`.cpp`
- 新增文件：`scheduler/tasks/set_ui_refresh_time_task.h`、`.cpp`
- 新增文件：`ui/customwidget/debugsettingwidget/vefcflowunitmediumstatuswidget.h`、`.cpp`
- 新增文件：`scheduler/tasks/read_vefc_flow_unit_medium_status_task.h`、`.cpp`
- 修改文件：`bin/config/ModbusTcpMasterConfig.xml`（新增 ReadVEFCFlowUnitAndMediumStatus 指令，InitialCommands 添加 WriteVEFCGasType）
- 修改文件：`ui/configpage.cpp`（集成 PurgeFlowSettingWidget）
- 修改文件：`ui/debugpage.cpp`（集成 VEFCGasTypeSettingWidget / UIRefreshTimeSettingWidget / VEFCFlowUnitMediumStatusWidget）
- 修改文件：`scheduler/scheduler.pri`（添加新任务文件）
- 修改文件：`ui/customwidget/configsettingwidget/configsettingwidget.pri`（添加 PurgeFlowSettingWidget）
- 修改文件：`ui/customwidget/debugsettingwidget/debugsettingwidget.pri`（添加三个 DebugPage 控件）

---

### 2026-04-30 14:18 - Simon
**Modbus 指令扩展：QRCode 地址修正与 Purge/VEFC/UI 刷新配置**

#### 修改内容

**1. ModbusTcpMasterConfig.xml WriteQRCode 地址修正**
- 修改 `WriteQRCode` 指令寄存器地址：0x0004 → 0x0002
- 请求帧：`01 10 00 02 00 02 04 XX XX XX XX CRC`
- 响应帧：`01 10 00 02 00 02 CRC`
- 功能码 10（写多个保持寄存器），写入 QRCode/设备ID（4字节）

**2. 新增 WritePurgeFlow 指令**
- 功能码 06，寄存器地址 0x0000
- 用于设置 Purge 流量大小（VEFC 流量）
- 请求帧：`01 06 00 00 XX XX CRC`
- 响应帧：`01 06 00 00 XX XX CRC`（镜像）
- 值计算：`Value = (256 * hi_byte + lo_byte)`
- 备注：流量值需乘以 100 写入（例：35 slm/sccm → 3500），仅 FOUP IN 时有效

**3. 新增 WriteVEFCGasType 指令**
- 功能码 06，寄存器地址 0x0001
- 用于写入 VEFC 气体介质类型（掉电保持）
- 请求帧：`01 06 00 01 XX XX CRC`
- 响应帧：`01 06 00 01 XX XX CRC`（镜像）
- 介质类型枚举：
  - 0x0000 = CDA（上电初始默认）
  - 0x0001 = N2
  - 0x0002 = Ar
  - 0x0003 = CO2
  - 0x0004 = O2
- 备注：放到初始下发器中

**4. 新增 WriteUIRefreshTime 指令**
- 功能码 10，寄存器地址 0x0004
- 用于设置 UI 页面刷新时间
- 请求帧：`01 10 00 04 00 02 04 XX XX XX XX CRC`
- 响应帧：`01 10 00 04 00 02 CRC`
- 数据长度：4 字节（2 个寄存器）

#### 影响范围
- 修改文件：`bin/config/ModbusTcpMasterConfig.xml`（WriteQRCode 地址修正，新增 WritePurgeFlow/WriteVEFCGasType/WriteUIRefreshTime 指令）

---

### 2026-04-27 16:50 - Simon
**配置文件更新：设备列表扩展与气控阀压力指令**

#### 修改内容

**1. ModbusTcpMasterConfig.xml 新增指令**
- 添加 `WritePneumaticValvePressure` 指令（功能码 06）
- 寄存器地址：0x0002
- 用于设置气控阀压力值
- 请求帧：`01 06 00 02 XX XX CRC`
- 响应帧：`01 06 00 02 XX XX CRC`（镜像返回）
- 值计算：`Value = (256 * hi_byte + lo_byte)`

**2. qrcode.ini 设备列表扩展**
- MasterDevices list 新增设备 ID：1220、1240、1270
- 原设备列表：12001, 12010
- 扩展后设备列表：12001, 12010, 1220, 1240, 1270

#### 影响范围
- 修改文件：`bin/config/ModbusTcpMasterConfig.xml`（添加 WritePneumaticValvePressure 指令）
- 修改文件：`bin/config/qrcode.ini`（扩展 MasterDevices list）

---

### 2026-04-27 11:30 - Simon
**固件升级：Modbus RTU CRC 字节序修复与设备重启等待时间配置**

#### 修改内容

**1. Modbus RTU CRC 字节序修复**
- 修复 `handleVersionResponse` 中 CRC 校验字节序错误
- Modbus RTU 协议标准：CRC 以低字节在前、高字节在后传输
- 原代码 `receivedCrc = (resp[5] << 8) | resp[6]` 字节序反了
- 修正为 `receivedCrc = (resp[6] << 8) | resp[5]`
- 响应帧无论 CRC 校验成功或失败都打印完整帧内容

**2. 数据传输后等待设备重启配置**
- 将 `handleTransferResponse` 中的 `postTransferWaitMs` 局部变量改为成员变量 `m_postTransferWaitMs`
- 添加 `setPostTransferWaitTime(int ms)` 方法，允许外部设置等待时间
- 默认值：2000ms
- 数据传输完成后等待设备重启/加载新固件，避免版本查询超时

**3. 调度层接口扩展**
- `SetFirmwareConfigTask` 添加 `setPostTransferWaitTime(int ms)` 方法
- 添加 `postTransferWaitTimeApplied()` 信号
- 添加成员变量 `m_postTransferWaitTime`
- 在 `start()` 中将配置应用到所有设备的 FirmwareUpgrader

**4. UI 层设置项添加**
- `FirmwareUpdateConfigSettingWidget` 添加 "Post-Transfer Wait Time" 设置项
- 新增控件 `m_postTransferWaitTimeSpinBox` 和 `m_postTransferWaitTimeItem`
- 添加 `initPostTransferWaitTimeItem()` 初始化方法
- 添加 `onPostTransferWaitTimeSetBtnClicked()` 槽函数
- 范围：0-60000ms

**5. FirmwareConfig 配置支持**
- 添加 `postTransferWaitMs()` getter
- 添加 `setPostTransferWaitMs()` setter
- 配置键：`PostTransferWaitTimeMs`
- 默认值：2000ms
- 支持从 `bin/config/firmware.ini` 加载和保存配置

#### 技术细节
- **Modbus RTU CRC 标准**：CRC16 校验码在传输时低字节在前、高字节在后
- **设备重启等待**：固件数据传输完成后，设备需要时间重启并加载新固件，此时发送版本查询会超时
- **配置层级**：UI → Scheduler → Data 层，三层配置传递链路

#### 影响范围
- 修改文件：`data/modbustcpmastermanager/modbustcpmaster/firmwareupgrader.h`（添加成员变量和 setter）
- 修改文件：`data/modbustcpmastermanager/modbustcpmaster/firmwareupgrader.cpp`（修复 CRC 字节序，使用成员变量）
- 修改文件：`scheduler/tasks/set_firmware_config_task.h`（添加方法和信号）
- 修改文件：`scheduler/tasks/set_firmware_config_task.cpp`（实现方法和应用配置）
- 修改文件：`ui/customwidget/debugsettingwidget/firmwareupdateconfigsettingwidget.h`（添加控件和方法声明）
- 修改文件：`ui/customwidget/debugsettingwidget/firmwareupdateconfigsettingwidget.cpp`（实现初始化和槽函数）
- 修改文件：`config/firmwareconfig.h`（添加 getter/setter）
- 修改文件：`config/firmwareconfig.cpp`（实现 getter/setter，加载/保存配置）

---

### 2026-04-27 09:00 - Simon
**内存泄漏修复与 UI 线程阻塞优化**

#### 问题背景
软件运行后内存持续增长至 5468MB，同时频繁写入运行日志时 UI 线程出现明显卡顿。

#### 根因分析

**内存泄漏 1（严重）— LogicalFileSystem 防抖永不触发**
- `LogicalFileSystem::writeLog()` 每次调用都重置 50ms single-shot 定时器
- 80 设备持续写入（间隔 < 50ms）→ 定时器永远不超时 → `m_pendingLogs`（`QVector<QJsonObject>`）无限增长
- 该 bug 同时影响 `RunningLoggerWidget` 和 `LoggerWidget`

**内存泄漏 2（严重）— CommLogicalFileSystem 事件队列无界膨胀**
- `CommLogicalFileSystem::writeLog()` 每次直接 emit 跨线程信号 `_requestAppendLog`
- Qt 为每次 emit 创建 `QMetaCallEvent`（携带 6 个 QString 拷贝）投递到 worker 线程
- worker 每次处理需 CSV 追加写 + 页表重建 + 页面读取，80 设备 × ~5 命令/秒 ≈ 400 事件/秒
- worker I/O 跟不上 → Qt 事件队列无限膨胀（每小时增长数百 MB）

**内存泄漏 3（中等）— 历史查询缓存容量不符**
- `CommLogFileSystem::m_queryCache{3}` 注释明确写"容量必须为 1"，实际为 3
- 每条缓存存整天 CSV 所有行（`QVector<QStringList>`），单条可达数百 MB

**UI 阻塞 1 — refreshDisplayText 每条记录重绘**
- `RunningLoggerWidget::writeRecord()` 每条都调用 `refreshDisplayText()` → `m_btn->setText()` → 触发 repaint
- 每 50ms 处理 20 条 = 400 次/秒按钮重绘

**UI 阻塞 2 — 归还剩余条目的 O(n²) 循环**
- `onFlushTick()` 先 swap 整个队列（可能 10000+ 条），只消费 20 条后归还剩余
- `QQueue::prepend()` 底层 `QList::prepend()` = O(n) memmove，9980 次循环 = O(n²)

**UI 阻塞 3 — Collector 队列无上限**
- `RunningLoggerCollector::log()` 无上限 enqueue → 连接风暴时队列无限膨胀

#### 修改内容

**1. LogicalFileSystem 防抖修复**
- `writeLog()` 添加 `kMaxPendingBatch = 200` 上限保护
- 达到上限时立即 `flushPendingLogs()`，不再等定时器
- 低频写入仍走 50ms 防抖合并

**2. CommLogicalFileSystem 批量缓冲改造**
- `writeLog()` 改为主线程侧缓冲到 `QVector<QStringList>`
- 50ms 防抖 + 200 条上限保护，批量 flush 到 worker
- `CommLogFileSystem` 新增 `requestAppendBatch(const QVector<QStringList> &)` 槽
- 一次批量 append 只做一次页表重建和页面读取（原先每条一次）
- 析构函数 `BlockingQueuedConnection` flush 残留缓冲

**3. 历史查询缓存容量修正**
- `m_queryCache{3}` → `m_queryCache{1}`

**4. RunningLoggerWidget 批量写入模式**
- 新增 `beginBatchWrite()` / `endBatchWrite()` 接口
- 批量模式下 `writeRecord()` 跳过 `refreshDisplayText()`
- 仅在 `endBatchWrite()` 时统一刷新一次按钮文本
- 400 次/秒 repaint → 20 次/秒

**5. RunningLoggerCollector 重写**
- `onFlushTick()` 只 dequeue 需要的条数，消除 swap + prepend-back 的 O(n²) 开销
- 使用 `beginBatchWrite()` / `endBatchWrite()` 包裹批量写入
- `log()` 添加 `kMaxQueueSize = 2000` 上限，超出丢弃最旧条目

#### 性能对比
| 指标 | 优化前 | 优化后 |
|---|---|---|
| 运行日志内存增长 | 无限（防抖永不触发） | 有界（≤200 条 flush） |
| 通讯日志事件队列 | 无限（逐条 emit） | 有界（≤200 条批量） |
| 历史查询缓存 | ≤3 天数据常驻 | ≤1 天数据常驻 |
| 按钮重绘频率 | 400 次/秒 | 20 次/秒 |
| Collector flush 复杂度 | O(n²) | O(k)，k=每帧消费数 |

#### 影响范围
- 修改文件：`ui/customwidget/loggerwidget/logicalfilesystem.h`（添加 kMaxPendingBatch）
- 修改文件：`ui/customwidget/loggerwidget/logicalfilesystem.cpp`（writeLog 添加上限保护）
- 修改文件：`ui/customwidget/communicateloggerwidget/commlogicalfilesystem.h`（添加 batch 信号/slot/定时器/缓冲区）
- 修改文件：`ui/customwidget/communicateloggerwidget/commlogicalfilesystem.cpp`（writeLog 改为缓冲 + 防抖；析构 flush）
- 修改文件：`ui/customwidget/communicateloggerwidget/commlogfilesystem.h`（添加 requestAppendBatch；m_queryCache 容量 3→1）
- 修改文件：`ui/customwidget/communicateloggerwidget/commlogfilesystem.cpp`（实现 requestAppendBatch）
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggerwidget.h`（添加 beginBatchWrite/endBatchWrite）
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggerwidget.cpp`（实现批量写入模式）
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggercollector.h`（添加 kMaxQueueSize）
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggercollector.cpp`（重写 onFlushTick；log 添加队列上限）

---

### 2026-04-24 10:50 - Simon
**Modbus 扩展：FOUP 属性与 Idle Purge 状态查询**

#### 主要完成的任务

**1. XML 配置扩展**
- 添加 7 条新 Modbus 指令定义到 `ModbusTcpMasterConfig.xml`
- ReadFoupStatus：FOUP 属性查询（进气压力、负压、流量、湿度、温度、FOUP 在位状态、充气时间）
- ReadIdlePurgeEnable：Idle Purge 使能查询
- ReadIdlePurgeStatus：Idle Purge 状态查询
- ReadIdlePurgeWorkingTime：Idle Purge 工作计时时间查询
- WriteIdlePurgeEnable：Idle Purge 使能写入
- WriteIdlePurgeTime：Idle Purge 时间写入
- WriteIdlePurgeInterval：Idle Purge 间隔写入

**2. 定时查询指令配置**
- 设置 `PeriodicCommandSender` 间隔为 300ms
- 添加 ReadFoupStatus、ReadIdlePurgeEnable、ReadIdlePurgeStatus、ReadIdlePurgeWorkingTime 到定时查询列表
- 定时查询逻辑：仅在 FOUP in 状态时下发 idle purge 相关查询指令

**3. Data 层：指令解析与响应处理**
- 创建 `CommandResponseParser` 类，实现模块化的 Modbus 响应解析
- 为 ReadFoupStatus、ReadIdlePurgeEnable、ReadIdlePurgeStatus、ReadIdlePurgeWorkingTime 注册解析函数
- 修复非标准设备响应处理：
  - ReadFoupStatus 响应函数码为 0x12（非标准），XML 配置已更新
  - 接收器帧长计算：当响应 FC ≠ 请求 FC 时，使用 XML 配置的 byteCount 而非帧内 BC 字段
  - fillResponseFrame 补充 case 0x12 分支，按读寄存器格式提取 registerValue

**4. Scheduler 层：数据更新与状态管理**
- MonitorDataTask 解析所有 4 条查询指令的响应
- 将解析结果写入全局共享变量 FoupOfOHBInfo
- 实现 FOUP 状态变化逻辑：
  - **FOUP out → in**：设置 startTime 为当前时间，idleState 置为 Idle，idleWorkingTimeSec 归 0
  - **FOUP in → out**：purgeTimeSec 和 startTime 归 0
  - **Idle Purge 禁用**：idleState 置为 Idle，idleWorkingTimeSec 归 0

**5. 全局共享变量扩展（FoupOfOHBInfo）**
- 新增字段：
  - `negativePressure`（double）：负压值
  - `idlePurgeEnabled`（bool）：Idle Purge 使能状态
  - `idleState`（enum）：Idle Purge 状态（Stopped、Preparing、Purging、Idle）
  - `idleWorkingTimeSec`（quint16）：Idle Purge 工作计时时间（秒）
- 重命名字段：
  - `purgeTimeMs` → `purgeTimeSec`（单位改为秒）
  - `idleTimeMs` → `idleWorkingTimeSec`（单位改为秒）

**6. UI 层：监控表格扩展**
- FoupMonitor 表（第一个表格）：
  - 添加 "Vacuum"（负压）列，显示负压值并加负号前缀（例如：-0.05 Bar）
  - 列头 "Humidity" 改为 "Relative Humidity"
- FoupPurgeTimeMonitor 表（第二个表格）：
  - 添加 "Idle Enable"、"Idle State"、"IdleTime" 列
  - 列顺序：Start Time / Duration / Purge Time / Idle Enable / Idle State / IdleTime
  - Duration：purgeTimeSec 转分钟显示（例如：5Min）
  - Idle State：枚举值转中文显示
  - IdleTime：idleWorkingTimeSec 转 hh:mm:ss 格式
- SetMonitor 表（第三个表格）：
  - 同步添加 "Vacuum" 列和 "Relative Humidity" 列头

**7. 代码国际化**
- RunningLoggerWidget 中文字符串改为英文：
  - "暂无日志" → "No logs"
  - "日志查看器" → "Log Viewer"

#### 技术细节
- **非标准设备处理**：本设备对 FC=0x04 请求返回 FC=0x12 响应，需要特殊处理帧长计算和数据提取
- **状态机管理**：FOUP 在位状态变化时自动重置相关计时字段，避免数据不一致
- **定时查询优化**：Idle purge 查询仅在 FOUP in 时执行，减少无效通信

#### 影响范围
- 修改文件：`bin/config/ModbusTcpMasterConfig.xml`
- 新增文件：`data/modbustcpmastermanager/modbuscommand/commandresponseparser.h`、`.cpp`
- 修改文件：`data/modbustcpmastermanager/modbuscommand/modbuscommand.pri`
- 修改文件：`data/modbustcpmastermanager/modbustcpmaster/modbuscommandreceiver.cpp`
- 修改文件：`classes/foupofohbinfo.h`、`.cpp`
- 修改文件：`scheduler/tasks/monitor_data_task.h`、`.cpp`
- 修改文件：`ui/customwidget/overheadcranetrack/devicemonitorwidget.cpp`
- 修改文件：`ui/customwidget/overheadcranetrack/framedevice.cpp`
- 修改文件：`scheduler/tasks/network_status_task.cpp`
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggerwidget.cpp`

---

### 2026-04-24 09:00 - Simon
**关机崩溃修复：ModbusTcpMasterPool 与 Scheduler 线程清理**

#### 问题描述
应用关机时在 `ModbusTcpMasterPool` 析构阶段发生崩溃，日志显示"清理线程"后立即崩溃。根本原因是 **竞态条件**：
- `clear()` 用 `QueuedConnection` 异步投递 `stop() + deleteLater()` 到 22 个工作线程
- 线程清理是串行的：当 thread 0 被 quit/wait/delete 时，其他线程仍在处理 `clear()` 投递的事件
- 二次 `stop()` 触发信号级联（`disconnectDevice` → `setStatus` → `statusChanged` → `onConnectionStatusChanged`），导致跨线程访问已销毁对象

#### 修改内容

**1. ModbusTcpMasterPool 析构函数重写**
- **第一步**：`stopAllMasters()` — 用 `BlockingQueuedConnection` 同步停止所有 Master（保留不变）
- **第二步**：直接 `deleteLater()` 所有 Master — 去掉冗余的二次 `stop()` 和 `QueuedConnection` lambda
- **第三步**：并行 `quit()` 所有线程 — 避免串行 quit 时部分线程仍在处理事件的竞态
- **第四步**：逐一 `wait()` 线程 — 确保干净退出
- **移除**：500ms sleep（同步调用无需额外等待）

**关键改进**：
```
修改前：stopAllMasters() → sleep(500) → clear() → 串行 quit/wait/delete 线程
修改后：stopAllMasters() → deleteLater() → 并行 quit() → 逐一 wait()
```

**2. Scheduler::stop() 跨线程安全修复**
- 问题：`qDeleteAll(m_tasks)` 在主线程直接 `delete` 住在调度器线程上的 Task 对象，违反 Qt 线程安全规则
- 修复：改为 `deleteLater()` + `quit()` + `wait()`，让调度器线程自行销毁 Task 对象
- 注释说明：`deleteLater` 事件会在 `quit` 退出事件循环前被处理

#### 修复后的完整关机流程
```
aboutToQuit → Scheduler::stop()
  ├─ 同步停止所有任务（BlockingQueuedConnection）
  ├─ deleteLater() 所有 Task
  ├─ quit() 调度器线程
  └─ wait() 等待线程退出

a.exec() 返回 → UI 析构（信号已自动断开）

App::cleanup() → ModbusTcpMasterManager::shutdown()
  ├─ stopAllMasters()（BlockingQueuedConnection 同步停止）
  ├─ deleteLater() 所有 Master
  ├─ 并行 quit() 所有工作线程
  └─ 逐一 wait() 确保干净退出
```

#### 技术细节
- **事件循环顺序**：Qt 事件循环按 FIFO 处理，`deleteLater()` 投递的 DeferredDelete 事件在 `quit()` 之前处理
- **线程安全**：`deleteLater()` 自动投递到对象所属线程，无需手动 `invokeMethod`
- **并行 quit 的好处**：避免串行 quit 时某些线程仍在处理事件，导致竞态条件

#### 影响范围
- 修改文件：`data/modbustcpmastermanager/modbustcpmasterpool.cpp`（析构函数）
- 修改文件：`scheduler/scheduler.cpp`（stop 方法）

---

### 2026-04-23 19:30 - Simon
**报警直连运行日志与界面卡顿优化**

#### 修改内容
- **AlarmLoggerWidget → RunningLoggerWidget 直连**
  - `RunningLoggerWidget` 新增 `onAlarmPublished(const AlarmInfo&)` 与 `onAlarmResolved(const AlarmInfo&)` 槽
  - 在 `UIDemo6::initForm()` 中将 `ui->alarmpage->alarmLogger()` 的 `alarmPublished/alarmResolved` 信号直接连接到运行日志控件
  - `onAlarmResolved` 固定按 `Message` 级别写入运行日志

- **AlarmPage/AlarmLoggerWidget 文案与消息增强**
  - 断连告警文案改为动态设备号：`Device {masterId} connection lost`
  - `SoftwareConnectionLost` 解决时文案统一为：`Device {qrCode} connection resolved`
  - `AlarmInfo` 新增 `setMessage(const QString&)`，支持在转发前按场景改写 message
  - 修复默认构造 `AlarmLoggerWidget(QWidget*)` 下未应用上述解决文案的问题

- **性能优化（缓解界面卡顿）**
  - `RunningLoggerWidget` 跑马灯改为缓存拼接字符串（`m_scrollDoubled/m_scrollTotalLen`），避免每 150ms 重复字符串分配
  - `AlarmLoggerWidget::purgeResolvedRows()` 批量删行时禁用表格更新，减少重复布局重算
  - `RunningLoggerCollector::onFlushTick()` 增加空队列快速返回，减少无效锁与 swap 开销

#### 影响范围
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggerwidget.h`
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggerwidget.cpp`
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggercollector.cpp`
- 修改文件：`ui/customwidget/alarmloggerwidget/alarminfo.h`
- 修改文件：`ui/customwidget/alarmloggerwidget/alarmloggerwidget.cpp`
- 修改文件：`ui/alarmpage.h`
- 修改文件：`ui/alarmpage.cpp`
- 修改文件：`ui/uidemo6.cpp`

---

### 2026-04-23 18:30 - Simon
**新增 RunningLoggerCollector 运行日志采集器**

#### 修改内容
- **RunningLoggerCollector**：新增跨线程全局日志采集单例
  - 基于 Producer-Consumer 模式，任意线程可调用 `logMessage()` 提交日志
  - 内部使用 `QQueue<LogEntry>` + `QMutex` 缓冲，主线程定时器（50ms）异步提交
  - 每帧最多提交 20 条（`kMaxFlushPerTick`），防止队列积压时 UI 卡顿
  - 使用 `Q_GLOBAL_STATIC` 管理单例生命周期，`QPointer<RunningLoggerWidget>` 自动处理 Widget 销毁

- **RunningLoggerWidget**：优化日志控件初始化
  - 构造函数自动设置日志根目录为 `CustomLogger::RunningLoggerPath()`
  - `initialize()` 时自动注册到 `RunningLoggerCollector::setTarget(this)`
  - 滚动定时器移至构造函数启动，不依赖 `initialize()`
  - `refreshDisplayText()` 中立即更新按钮文字，无需等待定时器 tick

- **CustomLogger**：新增运行日志路径
  - 新增静态方法 `RunningLoggerPath()`，返回 `bin/log/runningLog`

- **UIDemo6**：初始化运行日志控件
  - `initForm()` 中调用 `ui->runningLoggerWidget->initialize()`

- **日志调用点**（均为 Message 级别）
  - `logindialog.cpp`：记录登录成功/失败
  - `changepassworddialog.cpp`：记录密码修改成功/失败
  - `useraccountlabel.cpp`：记录用户退出登录、"Login New Account" 时先退出再登录

#### 影响范围
- 新增文件：`ui/customwidget/runningloggerwidget/runningloggercollector.h`、`ui/customwidget/runningloggerwidget/runningloggercollector.cpp`
- 修改文件：`ui/customwidget/runningloggerwidget/runningloggerwidget.h`、`ui/customwidget/runningloggerwidget/runningloggerwidget.cpp`、`ui/customwidget/runningloggerwidget/runningloggerwidget.pri`、`app/customlogger.h`、`app/customlogger.cpp`、`ui/uidemo6.cpp`、`ui/logindialog.cpp`、`ui/changepassworddialog.cpp`、`ui/customwidget/useraccountlabel/useraccountlabel.cpp`

---

### 2026-04-23 17:00 - Simon
**UserAccountLabel 菜单功能增强**

#### 修改内容
- **UserAccountLabel**：菜单功能增强
  - 未登录状态：显示当前账号级别（Level）、登录账号（Login）
  - 已登录状态：显示当前用户名（Current User）、账号级别（Level）、修改密码（Change Password）、登录新账号（Login New Account）、退出登录（Logout）
  - 菜单项高度增加至 40px，便于触屏点击
  - 菜单位置调整至 label 正下方，中心对齐
  - 所有菜单字符串改为英文

- **ChangePasswordDialog**：新增修改密码对话框
  - 需要输入旧密码验证身份
  - 输入新密码并确认
  - 密码无长度限制，只需非空
  - 所有界面字符串改为英文

- **UserManager**：权限字符串转换方法
  - 新增静态方法 `permissionToString()` 将权限枚举转换为英文字符串
  - 从 UserAccountLabel 移至 UserManager，避免代码重复

#### 影响范围
- 新增文件：`ui/changepassworddialog.h`、`ui/changepassworddialog.cpp`
- 修改文件：`ui/customwidget/useraccountlabel/useraccountlabel.h`、`ui/customwidget/useraccountlabel/useraccountlabel.cpp`、`tool/usermanager/usermanager.h`、`tool/usermanager/usermanager.cpp`、`ui/logindialog.cpp`、`ui/ui.pri`

---

### 2026-04-23 16:20 - Simon
**UI 权限注册集成**

#### 修改内容
- **UIDemo6**：添加 `registerWidgetPermissions()` 方法
  - 通过 `UserManager::registerWidget()` 注册 UI 控件权限
  - `btnDebug` → `UserPermission::Root` (4)
  - `btnCommunicate` → `UserPermission::Debug` (2)
  - `btnSetting` → `UserPermission::Normal` (1)（普通用户可查看）
  - 在 `initForm()` 中调用权限注册方法

#### 影响范围
- 修改文件：`ui/uidemo6.h`、`ui/uidemo6.cpp`

---

### 2026-04-23 16:00 - Simon
**新增运行日志模块和用户管理模块**

#### 运行日志模块
- **RunningLoggerWidget**：实时运行日志显示组件
  - 支持三种消息类型：Message、Warn、Error
  - 待处理警报轮播显示
  - 基于 LoggerWidget 实现，使用 QJson 格式存储
  - 支持警报解决记录

#### 用户管理模块
- **UserManager**：用户权限管理系统
  - 基于单例模式，统一管理用户登录状态
  - 支持五级权限：Guest、Normal、Debug、Engineer、Root
  - 内置 root 账号（密码：cytc666666），硬编码验证不写入配置文件
  - 自动控制 UI 控件显示/隐藏（基于权限级别）
  - 支持用户增删改查（需要 Debug 及以上权限）

- **UserAccountLabel**：用户账号显示组件
  - 未登录：显示默认图标，左键点击弹出登录菜单
  - 已登录：显示蓝色圆形头像 + 用户名首字母
  - 支持登录新账号和登出功能
  - 自动响应登录/登出状态变化

- **LoginDialog**：登录对话框
  - 用户名和密码输入
  - 通过 UserManager 进行登录验证
  - 错误提示显示

---

### 2026-04-22 19:16 - Simon
**通讯日志历史查询性能优化（翻页 1021ms → 4ms）**

#### 问题背景
用户反馈通讯日志（`CommunicateLoggerWidget`）历史 Tab 点击分页按钮非常慢（约 1 秒），即使数据已缓存。

#### 根因分析
通过插入 `QElapsedTimer` 计时日志定位瓶颈：
- 每次翻页 **cache MISS**，worker 线程每次重读 74 万行 CSV（约 1015ms）
- 原缓存键包含 `fileSig`（文件路径 + 大小），当天 CSV 文件在持续 `append` 实时日志，大小不断变化，导致缓存**永远失效**

#### 修改内容
1. **查询缓存从"单项"升级为 LRU（容量 5）**
   - 原 `m_qc*` 系列字段（单项缓存）替换为 `CommLRUCache<CommQueryKey, CommQueryValue> m_queryCache{5}`
   - 切换查询条件（日期/时间段/qrcode/commandId）也能命中历史缓存

2. **缓存键移除文件大小依赖**
   - `CommQueryKey` 删除 `files` 字段（原为 `QVector<QPair<QString,qint64>>`）
   - 今天 CSV 持续 append 不再让翻页缓存失效

3. **显式刷新机制**
   - `CommHistoryQuery` 新增 `bool forceRefresh = false`
   - Search 按钮 → `forceRefresh=true`，worker 主动 `m_queryCache.remove(key)` 重算最新数据
   - 翻页按钮 → `forceRefresh=false`，O(1) 命中缓存做切片

4. **`matchedGlobalIndices` O(N) → O(1)**
   - 原每次翻页都 `resize(viewSize)` 并填充 `[0..viewSize-1]`（8179 条时跨线程传输量大）
   - 改为只在有命中时填 1 个哨兵元素（widget 只用 `isEmpty()` 判断）

5. **Miss 时才读盘**
   - 缓存命中时完全跳过 `filePathsForDate` 和 `QtConcurrent::blockingMapped`

#### 性能对比
| 指标 | 优化前 | 优化后 |
|---|---|---|
| 翻页总耗时 | 1021 ms | **4 ms** |
| 缓存命中 | MISS（每次） | HIT |


---

### 2026-04-22 18:00 - Simon
**AlarmLoggerWidget 警报日志模块完善**

#### 修改内容
1. **UI 字符串本地化**
   - `alarmpage.cpp`、`alarmloggerwidget.ui`、`alarmloggerwidget.cpp` 中所有中文 UI 字符串统一为英文
   - 移除不需要的按钮（findPrevBtn、findNextBtn）及相关逻辑
   - `labelLike`、`likeEdit` 替换为 `QRCode SpinBox` 和 `CommandId ComboBox`
   - 查找成功/失败均通过 `QMessageBox` 提示

2. **已解决告警行清理机制**
   - `AlarmLoggerWidget` 新增 `QList<int> m_resolvedRows` 记录已解决行索引
   - 新增 `QTimer m_purgeTimer`，每 3 秒触发 `purgeResolvedRows()` 按 FIFO 清理
   - 新增 `setResolvedPurgeBatchSize(int)` 接口，默认批次 40 条
   - 清理后自动重建内部行索引映射
   - 日期翻页（`onDayRollover`）时清空 `m_resolvedRows`

3. **表格列宽比例配置**
   - 实时表格与历史表格统一使用 `5:8:5:5:5:8:5` 列宽比例
   - `Message` 列左对齐，其余列居中（`Request`/`Response` 除外）
   - 分页按钮增大尺寸（宽度、字体）方便点击

4. **警报日志根目录配置**
   - `AlarmPage` 初始化时通过 `setRootDir(CustomLogger::AlarmLoggerPath())` 设置根目录为 `bin/log/alarms/`
   - 警报日志按 `年月/日_HHmmss.csv` 结构存储，保留 6 个月

5. **网络状态联动**
   - `AlarmPage::onNetworkStatusChanged` 在断连时自动上报 `SoftwareConnectionLost` 告警
   - 重连成功时自动提交告警解决

6. **注释统一**
   - `alarmloggerwidget.h` / `.cpp` 中的英文注释统一改回中文，保持项目注释风格一致

#### 影响范围
- 修改文件：`ui/alarmpage.cpp`
- 修改文件：`ui/customwidget/alarmloggerwidget/alarmloggerwidget.ui`
- 修改文件：`ui/customwidget/alarmloggerwidget/alarmloggerwidget.h`
- 修改文件：`ui/customwidget/alarmloggerwidget/alarmloggerwidget.cpp`

---

### 2026-04-22 11:43 - Simon
**CommunicatePage 模块完成**

#### 修改内容
1. **UI字符串本地化**
   - 将 `communicateloggerwidget.ui` 中所有UI标签、按钮文本翻译为英文
   - 将 `communicateloggerwidget.cpp` 中列头（kLiveHeaders）和用户可见消息翻译为英文
   - 更新 `communicatepage.cpp` 中 JSON 键以匹配新的英文列头
   - 翻译 `commloadmoreitemwidget.cpp` 中加载更多小部件的文本
   - 翻译 `commlogfilesystem.cpp` 中错误消息
   - 修复 `commlogpagemodel.cpp` 中用户可见格式中的冒号

2. **时间格式优化**
   - 将日志时间格式从 `yyyy-MM-dd HH:mm:ss.zzz` 改为 `yyyy-MM-dd HH:mm:ss`
   - 不再显示毫秒部分

3. **表格列宽比例设置**
   - 实时表格和历史表格列宽比例设置为 1:2:2.5:1:3.5:6
   - qrcode: 80px, Time: 160px, CommandId: 200px, DurationMs: 80px, Request: 280px, Response: 480px

#### 影响范围
- 修改文件：`ui/customwidget/communicateloggerwidget/communicateloggerwidget.ui`
- 修改文件：`ui/customwidget/communicateloggerwidget/communicateloggerwidget.cpp`
- 修改文件：`ui/communicatepage.cpp`
- 修改文件：`ui/customwidget/communicateloggerwidget/commloadmoreitemwidget.cpp`
- 修改文件：`ui/customwidget/communicateloggerwidget/commlogfilesystem.cpp`
- 修改文件：`ui/customwidget/communicateloggerwidget/commlogpagemodel.cpp`

---

### 2026-04-20 14:28 - Simon
**MonitorDataTask 数据解析修正**

#### 修改内容
- 修正 `parseReadFoupStatusResponse` 中时间字段的单位转换
- 充气时间和 idle 时间从秒转换为毫秒（乘以 1000）
- 更新 `getU16BE` lambda 注释，明确单位为秒

#### 技术细节
- Modbus 寄存器返回的时间单位为秒，需转换为毫秒存储
- 字节偏移 10-11：充气时间（秒）→ 毫秒
- 字节偏移 12-13：idle 时间（秒）→ 毫秒

#### 影响范围
- 修改文件：`scheduler/tasks/monitor_data_task.cpp`

---

### 2026-04-20 14:06 - Simon
**图配置解析器增强与天车轨道布局优化**

#### 修改内容
1. **GraphConfigParser 自动设置 SET 节点 uiId**
   - 在 `GraphConfigParser` 中添加 `m_setNodeIndex` 计数器成员变量
   - 在构造函数中初始化计数器为 0
   - 在 `processNode()` 方法中，解析到 SET 节点时按文档顺序自动设置 `SharedData::setOfOHBInfoList` 中对应项的 `uiId`
   - 添加详细日志记录：`"SET节点文档顺序#{}: 设置 setOfOHBInfoList[{}].uiId = {}"`
   - 移除 `SharedData` 构造函数中硬编码的 `uiIds` 数组，改为由 XML 解析器动态设置

2. **graph_config.xml 布局调整**
   - 调整边的偏移值以优化天车轨道布局显示效果

#### 技术细节
- **SET 节点顺序映射**：按 XML 文档中 SET 节点的出现顺序（从上到下），依次映射到 `setOfOHBInfoList[0]`、`[1]`、`[2]`...
- **解析时机**：`GraphConfigParser::parse()` 在 `QtConcurrent::run()` 中异步执行，确保在 `SharedData` 初始化之后
- **线程安全**：`SharedData::setOfOHBInfoList` 在主线程初始化，解析器在工作线程中修改 `uiId`，无并发冲突
- **日志路径**：使用 `AppLogger::CraneMapLoggerPath()` 记录到 `ui/cranemap` 日志文件

#### 数据流向
```
GraphConfigParser::parse()
  ↓ parseEdges()
  ↓ processNode(SET节点)
  ↓ 创建 SetOfOHBNode
  ↓ SharedData::setOfOHBInfoList[m_setNodeIndex].setUiId(nodeId)
  ↓ m_setNodeIndex++
```

#### 影响范围
- 修改文件：`ui/customwidget/overheadcranetrack/graphconfigparser.h`（添加 `m_setNodeIndex` 成员）
- 修改文件：`ui/customwidget/overheadcranetrack/graphconfigparser.cpp`（引入 `shareddata.h`，构造函数初始化计数器，`processNode` 中设置 uiId）
- 修改文件：`bin/config/graph_config.xml`（调整边的偏移值以优化布局显示）

---

### 2026-04-20 11:05 - Simon
**日志系统优化与固件升级功能增强**

#### 修改内容
1. **日志级别统一**
   - 移除 "trace" 日志级别，统一使用 info/warn/error 三种级别
   - 数据帧日志从 trace 改为 info 级别
   - 移除日志样式中的 trace 分支，仅保留 error（红色）和 warn（橙色）文字颜色

2. **日志批量写入优化**
   - 在 LogicalFileSystem 中添加日志缓冲机制
   - 使用 100ms 定时器批量写入日志，减少信号发射频率
   - LogFileSystem 新增 requestAppendBatch 槽，支持批量写入
   - 解决高频日志写入导致的 UI 刷新闪烁问题

3. **固件升级截图功能**
   - 实现 captureTableWidgetScreenshot() 方法，捕获升级结果表格
   - 截图保存路径：`bin/log/firmware_upgrade/capture/`
   - 文件命名格式：`firmware_update_yyyyMMdd_hhmmss_zzz.png`
   - 在所有设备升级完成时自动触发截图，确保最终状态完整

4. **固件升级界面重构**
   - 创建 FirmwareUpdateSettingWidget 容器，用 SettingWidget 封装升级界面
   - 配置界面（FirmwareUpdateConfigSettingWidget）和升级界面分离
   - 通过 Qt 信号槽（binFilePathChanged）解耦通信
   - 标题改为英文：Firmware Config、Firmware Update

5. **目录结构调整**
   - 重命名 `customwidget/debugpage/` → `customwidget/debugsettingwidget/`
   - 更新 customwidget.pri 中的 include 路径
   - 更新 debugpage.cpp 中的头文件引用路径
   - 添加 .gitignore，忽略构建产物和日志文件

#### 技术细节
- **日志缓冲**：QVector<QJsonObject> 缓冲 + QTimer::singleShot(100) 触发批量写入
- **截图时机**：onTaskAllProgress 中 completed >= total 时触发，先 repaint() 再截图
- **信号解耦**：配置界面发射 binFilePathChanged 信号，DebugPage 连接到升级界面

#### 影响范围
- 新增文件：`ui/customwidget/debugsettingwidget/firmwareupdatesettingwidget.h`、`.cpp`
- 新增文件：`.gitignore`
- 修改文件：`ui/customwidget/debugsettingwidget/firmwareupdateconfigsettingwidget.h`、`.cpp`
- 修改文件：`ui/customwidget/debugsettingwidget/firmwareupdatewidget.cpp`（日志级别、截图）
- 修改文件：`ui/customwidget/loggerwidget/logfilesystem.h`、`.cpp`（批量写入）
- 修改文件：`ui/customwidget/loggerwidget/logicalfilesystem.h`、`.cpp`（批量缓冲）
- 修改文件：`ui/customwidget/loggerwidget/logitemdelegate.cpp`（样式调整）
- 修改文件：`ui/debugpage.h`、`ui/debugpage.cpp`（集成两个 SettingWidget）
- 修改文件：`ui/customwidget/customwidget.pri`（路径更新）
- 修改文件：`ui/customwidget/debugsettingwidget/debugsettingwidget.pri`（文件重命名）

---

### 2026-04-18 11:30 - Simon
**固件升级任务迁移**

#### 修改内容
1. **FirmwareUpgradeTask 固件升级任务**
   - 从旧代码 `old/firmware_upgrade_task.{h,cpp}` 迁移到新架构
   - 创建 `scheduler/tasks/firmware_upgrade_task.h` 和 `.cpp`
   - 适配新版 `FirmwareUpgrader` 类（位于 data 层）
   - 支持多设备并发固件升级
   - 使用共享 BinFileReader 实例，避免重复读取文件
   - 遵循项目日志格式规范：`[Scheduler][FirmwareUpgradeTask][方法名]`

2. **主要变更**
   - 设备标识从 `qrcode` 改为 `deviceId`（与 ModbusTcpMaster 的 ID 字段一致）
   - FirmwareUpgrader 信号增加 `masterId` 参数，便于多设备场景识别
   - ModbusTcpMasterManager 访问方式从指针改为引用
   - 添加详细的迁移说明文档：`scheduler/tasks/FIRMWARE_UPGRADE_MIGRATION.md`

3. **构建配置**
   - 将新任务添加到 `scheduler/scheduler.pri`
   - 保留旧代码在 `old/` 目录作为参考

---

### 2026-04-17 19:30 - Simon
**调度系统完善与日志格式优化**

#### 修改内容
1. **InitCheckTask 初始化检查任务**
   - 创建 `scheduler/tasks/init_check_task.h` 和 `.cpp`
   - 普通任务，监听所有 ModbusTcpMaster 的 InitialCommandIssuer 的 `finished` 信号
   - 汇总所有设备的初始化结果，区分成功和失败设备
   - 发射 `allFinished` 信号，携带成功数、失败数和失败设备ID列表
   - 任务完成后自动释放，不占用调度器资源

2. **NetworkStatusTask 网络状态监控任务**
   - 创建 `scheduler/tasks/network_status_task.h` 和 `.cpp`
   - 长驻任务，监听所有 ModbusTcpMaster 的 ModbusConnecter 的 `statusChanged` 信号
   - 在 `start()` 方法中创建并启动 InitCheckTask，管理其生命周期
   - 启动所有 ModbusTcpMaster 的自动重连功能
   - 根据连接状态实时更新 FoupOfOHBInfo 的告警信息（alarmId=111）
   - 连接成功时清除告警，断开或出错时设置告警
   - 接收 InitCheckTask 的 `allFinished` 信号并转发为 `allInitFinished` 信号

3. **调度器初始化流程优化**
   - `App::initScheduler()` 在 `getSharedData()` 之后调用，确保 ModbusTcpMaster 实例已创建
   - 在 NetworkStatusTask 启动前提交任务，保证能够监控到底层 Master 类的初始化过程
   - 只提交 NetworkStatusTask，InitCheckTask 由 NetworkStatusTask 内部管理
   - 移除 SharedData 中的 ModbusTcpMaster 自动启动调用
   - 将 ModbusTcpMaster 启动控制权交给 NetworkStatusTask

4. **MonitorDataTask 完善**
   - 添加构造函数和析构函数日志，格式与 InitCheckTask 一致
   - 移除冗余的数据更新日志，减少日志输出量
   - 添加 PeriodicCommandSender 为空的详细日志提示

5. **日志格式统一优化**
   - **Scheduler 模块**：统一为 `[Scheduler][类名][方法名]` 格式
     - Scheduler 类：`[Scheduler][方法名]`（避免重复类名）
     - InitCheckTask：`[Scheduler][InitCheckTask][方法名]`
     - NetworkStatusTask：`[Scheduler][NetworkStatusTask][方法名]`
     - MonitorDataTask：`[Scheduler][MonitorDataTask][方法名]`
     - SendCommandTask：`[Scheduler][SendCommandTask][方法名]`
   
   - **UI 模块**：统一为 `[ui][类名][方法名]` 格式
     - GraphConfigParser：`[ui][GraphConfigParser][方法名]`
     - SetLevelGraphBuilder：`[ui][SetLevelGraphBuilder][方法名]`
     - FoupLevelGraphBuilder：`[ui][FoupLevelGraphBuilder][方法名]`
     - GraphAdjacencyList：`[ui][Graph][方法名]`
     - GraphAdjacencyMultilist：`[ui][Graph][方法名]`
   
   - **Data 模块**：统一为 `[data][类名][方法名]` 格式
     - ModbusTcpMasterManager：`[data][ModbusTcpMasterManager][方法名]`
     - ModbusTcpMasterPool：`[data][ModbusTcpMasterPool][方法名]`
     - ModbusTcpMaster：`[data][ModbusTcpMaster][方法名]`
     - ModbusConnecter：`[data][ModbusConnecter][方法名]`
     - ModbusCommandReceiver：`[data][ModbusCommandReceiver][方法名]`
     - ModbusCommandSender：`[data][ModbusCommandSender][方法名]`
     - ModbusConfigParser：`[data][ModbusConfigParser][方法名]`
   
   - **日志路径优化**：
     - 添加 `AppLogger::CraneMapLoggerPath()` 返回 `"ui/cranemap"`
     - 修改 `AppLogger::ModbusMasterLoggerPath()` 返回 `"data/modbustcpmaster/deviceid_%1"`
     - 天车轨道布局相关类统一使用 CraneMapLoggerPath

6. **界面刷新问题修复**
   - 解决 FrameDevice 控件背景颜色状态不刷新的问题
   - 确保设备状态变化时界面能够实时更新显示

7. **QString::arg() 歧义修复**
   - 在 `send_command_task.cpp` 中使用 `QString::fromLatin1()` 显式转换 QByteArray
   - 避免 QString::arg() 重载解析冲突

#### 技术细节
- **任务生命周期管理**：InitCheckTask 由 NetworkStatusTask 创建、启动和释放
- **信号转发机制**：NetworkStatusTask 接收 InitCheckTask 的 allFinished 信号并转发为 allInitFinished
- **线程安全**：所有信号连接使用 Qt::QueuedConnection 确保跨线程安全
- **日志分层**：按模块（Scheduler/ui/data）和类名分层，便于日志过滤和问题定位
- **启动顺序**：Scheduler → SharedData → NetworkStatusTask → InitCheckTask → ModbusTcpMaster

#### 数据流向
```
App::initialize()
  ↓ initScheduler()
Scheduler::start()
  ↓ submitTask(NetworkStatusTask)
NetworkStatusTask::start()
  ↓ create InitCheckTask
  ↓ start all ModbusTcpMaster
  ↓ connect statusChanged signals
InitCheckTask::start()
  ↓ connect InitialCommandIssuer::finished signals
  ↓ wait for all devices init
  ↓ emit allFinished
NetworkStatusTask::onInitCheckFinished()
  ↓ release InitCheckTask
  ↓ emit allInitFinished
```

#### 影响范围
- 新增文件：`scheduler/tasks/init_check_task.h`、`scheduler/tasks/init_check_task.cpp`
- 新增文件：`scheduler/tasks/network_status_task.h`、`scheduler/tasks/network_status_task.cpp`
- 修改文件：`app/app.cpp`（调整 initScheduler 调用顺序）
- 修改文件：`app/shareddata.cpp`（移除 ModbusTcpMaster 自动启动）
- 修改文件：`app/applogger.h`、`app/applogger.cpp`（添加日志路径方法）
- 修改文件：`scheduler/scheduler.cpp`（日志格式优化）
- 修改文件：`scheduler/tasks/monitor_data_task.cpp`（添加构造/析构日志，移除冗余日志）
- 修改文件：`scheduler/tasks/send_command_task.cpp`（修复 QString::arg 歧义）
- 修改文件：`ui/customwidget/overheadcranetrack/graphconfigparser.cpp`（日志格式优化）
- 修改文件：`ui/customwidget/overheadcranetrack/setlevelgraphbuilder.cpp`（日志格式优化）
- 修改文件：`ui/customwidget/overheadcranetrack/fouplevelgraphbuilder.cpp`（日志格式优化）
- 修改文件：`ui/customwidget/overheadcranetrack/graphadjacencylist.h`（日志格式优化）

---

### 2026-04-17 15:48 - Simon
**重大功能：调度层实现与 UI-Scheduler-Data 三层通路打通**

#### 修改内容
1. **Scheduler 调度器核心实现**
   - 创建 `scheduler/scheduler.h` 和 `scheduler.cpp`，实现单例模式的任务调度器
   - 支持长驻任务（persistent）和普通任务两种类型
   - 长驻任务不占用并发槽位，立即启动并持续运行
   - 普通任务进入队列，按并发数限制调度执行
   - 调度器运行在独立线程 `SchedulerThread`，避免阻塞主线程
   - 使用 `QMetaObject::invokeMethod` 的 functor 形式跨线程调用任务方法，避免 moc 名字查找问题

2. **SchedulerTask 任务基类**
   - 创建 `scheduler/scheduler_task.h`，定义任务生命周期接口
   - 任务状态：Pending、Running、Paused、Finished、Failed、Cancelled
   - 任务优先级：Low、Normal、High、Urgent
   - 提供 `start()`、`stop()`、`pause()`、`resume()` 虚方法供子类实现
   - 支持 `isPersistent()` 和 `isRecurring()` 属性标识
   - 信号：`stateChanged`、`progress`、`finished`、`dataResult`

3. **MonitorDataTask 监控数据任务**
   - 创建 `scheduler/tasks/monitor_data_task.h` 和 `.cpp`
   - 长驻任务，监听所有 ModbusTcpMaster 的 PeriodicCommandSender 的 `commandCompleted` 信号
   - 解析 ReadFoupStatus 响应帧（18字节，9个寄存器，大端序）
   - 通过 `SharedData::getFoupByQRCode()` 更新 FoupOfOHBInfo 实时数据
   - 支持跨线程信号连接（Qt::QueuedConnection）

4. **SendCommandTask 业务指令任务**
   - 创建 `scheduler/tasks/send_command_task.h` 和 `.cpp`
   - 普通任务，用于发送业务指令（如充氮、抽真空等）
   - 通过 `ModbusTcpMasterManager` 获取 ModbusCommandSender
   - 克隆指令模板，构建参数覆盖，提交高优先级指令
   - 监听 `commandFinished` 信号，解析响应并发射 `finished` 信号
   - 支持超时、重试、设备繁忙等异常处理

5. **SharedData 增强**
   - 添加 `getFoupByQRCode()` 静态方法，根据 qrCode 快速查找 FoupOfOHBInfo 指针
   - 遍历所有 SetOfOHBInfo 和 FoupOfOHBInfo，匹配 qrCode 并返回

6. **PeriodicCommandSender 信号增强**
   - `commandCompleted` 信号添加 `masterId` 参数，方便调度层直接使用
   - 构造函数中通过 lambda 转发 `commandSucceeded` 信号并附加 masterId

7. **ModbusCommandSender 信号增强**
   - `commandFinished` 信号添加 `masterId` 参数
   - 所有 `emit commandFinished` 调用均添加 `m_masterId` 参数

8. **App 初始化流程完善**
   - 在 `App::initialize()` 中添加 `initScheduler()` 调用
   - `App::initScheduler()` 启动调度器线程并打印日志
   - 确保调度器线程在任务提交前已启动

9. **HomePage 集成调度器**
   - 添加 `initScheduler()` 方法，创建并提交 MonitorDataTask
   - 在构造函数中调用 `initScheduler()`，启动实时数据监控

10. **日志系统统一**
    - `modbuscommandreceiver.cpp` 中所有日志路径获取从 `LoggerFilePathManager::` 改为 `AppLogger::`
    - 统一使用 `AppLogger::getModbusMasterLogPath()` 和 `AppLogger::getModbusFrameMessageLogPath()`

#### 技术细节
- **线程模型**：Scheduler 运行在独立线程，所有 Task 通过 `moveToThread` 移动到 Scheduler 线程执行
- **信号槽连接**：使用 `Qt::QueuedConnection` 确保跨线程安全
- **invokeMethod 优化**：使用 functor 形式 `[task]() { task->start(); }` 替代字符串 `"start"`，避免 moc 元信息查找失败
- **大端序解析**：MonitorDataTask 中使用 `(data[i] << 8) | data[i+1]` 解析 Modbus 大端序数据
- **UUID 追踪**：所有指令使用 `cmd.uuid` 在异步通信中追踪指令生命周期

#### 数据流向
```
UI (HomePage)
  ↓ submitTask(MonitorDataTask)
Scheduler
  ↓ invokeMethod(task->start)
MonitorDataTask
  ↓ connect(PeriodicCommandSender::commandCompleted)
Data Layer (ModbusTcpMaster)
  ↓ emit commandCompleted(cmd, masterId)
MonitorDataTask::onCommandCompleted
  ↓ parseReadFoupStatusResponse
  ↓ SharedData::getFoupByQRCode
  ↓ updateFoupInfo
SharedData (FoupOfOHBInfo)
```

#### 影响范围
- 新增文件：`scheduler/scheduler.h`、`scheduler/scheduler.cpp`
- 新增文件：`scheduler/scheduler_task.h`
- 新增文件：`scheduler/tasks/monitor_data_task.h`、`scheduler/tasks/monitor_data_task.cpp`
- 新增文件：`scheduler/tasks/send_command_task.h`、`scheduler/tasks/send_command_task.cpp`
- 修改文件：`app/app.h`、`app/app.cpp`（添加 initScheduler）
- 修改文件：`app/shareddata.h`、`app/shareddata.cpp`（添加 getFoupByQRCode）
- 修改文件：`ui/homepage.h`、`ui/homepage.cpp`（集成调度器）
- 修改文件：`data/modbustcpmastermanager/modbustcpmaster/periodiccommandsender.h`、`.cpp`（信号增强）
- 修改文件：`data/modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h`、`.cpp`（信号增强）
- 修改文件：`data/modbustcpmastermanager/modbustcpmaster/modbuscommandreceiver.cpp`（日志统一）

---

### 2026-04-17 09:46 - Simon
**重大重构：配置系统优化与 Modbus 初始化改进**

#### 修改内容
1. **配置类重构**
   - 创建 `NetworkConfig` 类，管理 `network.ini` 配置文件（80个OHB的IP和端口）
   - 创建 `QRCodeConfig` 类，管理 `qrcode.ini` 配置文件（80个OHB的二维码映射）
   - 创建 `UserInfoConfig` 类，管理 `userinfo.ini` 配置文件
   - 从 `OHBNetworkInfo` 和 `OHBQRCodeInfo` 结构体中移除 `ohbIndex` 字段
   - 删除 `OHBQRCodeInfo` 结构体，改用 `QString` 直接存储二维码

2. **AppConfig 类增强**
   - 添加 `getNetworkConfigPath()`、`getQRCodeConfigPath()`、`getUserInfoConfigPath()` 方法
   - 添加 `getModbusConfigPath()` 方法，获取 Modbus 配置文件路径
   - 添加 `getNetworkConfig()`、`getQRCodeConfig()`、`getUserInfoConfig()` 方法获取配置类实例
   - 修复循环依赖问题

3. **SharedData 初始化优化**
   - 从配置文件读取 OHB 网络信息和二维码映射
   - 按数字顺序（OHB1-OHB80）读取配置，确保顺序正确
   - 为每个 Foup 创建独立的 ModbusTcpMaster 实例（共320个）
   - 添加 `foupIn` 和 `hasAlarm` 字段初始化
   - 移除 SharedData 中的 ModbusTcpMasterManager 初始化，改为在 App::initialize() 中加载

4. **配置文件更新**
   - `network.ini`：所有 OHB 的 IP 统一为 169.254.173.206，端口从 500-579
   - 配置读取添加详细的调试日志，打印每一条配置项

5. **App 初始化流程优化**
   - 在 `App::initialize()` 中添加 ModbusTcpMasterManager 配置文件加载
   - 使用 `AppConfig::getInstance().getModbusConfigPath()` 获取配置路径
   - 添加 `modbustcpmastermanager.h` 头文件引用

6. **CraneMapWidget 自动刷新**
   - 添加 QTimer 定时器，每隔1秒自动刷新设备数据
   - 在构造函数中初始化并启动定时器

7. **Modbus 日志系统整合**
   - 将 `modbustcpmastermanager` 模块中的所有日志引用从 `loggermanager.h` 改为 `applogger.h`
   - 修改文件包括：`modbustcpmastermanager.cpp`、`modbustcpmasterpool.cpp`、`modbuscommandreceiver.cpp`、`modbusconnecter.cpp`、`periodiccommandsender.cpp`、`initialcommandissuer.cpp`、`modbustcpmaster.cpp`、`modbuscommandsender.cpp`、`modbusconfigparser.cpp`
   - 统一使用 AppLogger 进行日志管理，确保日志路径一致性

#### 技术细节
- 配置读取顺序：使用 `for (int i = 1; i <= 80; ++i)` 确保按 OHB1-OHB80 顺序读取
- Modbus Master 创建：每个 Foup 使用独立的 IP、端口和二维码作为 Master ID
- 日志增强：在 `readNetworkConfig()`、`readQRCodeMapping()`、`readMasterDevices()` 中添加详细日志

#### 影响范围
- 新增文件：`app/networkconfig.h`、`app/networkconfig.cpp`
- 新增文件：`app/qrcodeconfig.h`、`app/qrcodeconfig.cpp`
- 新增文件：`app/userinfoconfig.h`、`app/userinfoconfig.cpp`
- 修改文件：`app/appconfig.h`、`app/appconfig.cpp`
- 修改文件：`app/shareddata.h`、`app/shareddata.cpp`
- 修改文件：`app/app.cpp`
- 修改文件：`ui/customwidget/overheadcranetrack/cranemapwidget.h`、`cranemapwidget.cpp`
- 修改文件：`bin/config/network.ini`
- 修改文件：`data/modbustcpmastermanager/` 目录下的 9 个 .cpp 文件（日志系统整合）

---

### 2026-04-16 19:20 - Simon
**功能增强：添加全局共享数据类**

#### 修改内容
1. **添加 SharedData 类**
   - 创建 `shareddata.h` 和 `shareddata.cpp`，实现全局共享内存管理
   - 包含静态成员变量 `setOfOHBInfoList`，存储 20 个 SetOfOHBInfo 对象
   - 提供 `getSetOfOHBInfoByUiId()` 方法，根据 uiId 获取对应的 SetOfOHBInfo 对象
   - 每个 SetOfOHBInfo 包含 4 个 FoupOfOHBInfo 对象

2. **App 类集成 SharedData**
   - 添加 `getSharedData()` 方法，在应用程序启动时初始化 SharedData
   - 在 `App::initialize()` 中调用 `getSharedData()` 确保共享数据初始化

3. **AppConfig 类增强**
   - 添加 `getGraphConfigPath()` 方法，获取天车地图配置文件路径
   - 配置文件路径：`bin/config/graph_config.xml`

4. **HomePage 类完善**
   - 添加四个初始化方法：`initUI()`、`initDeviceMonitorWidget()`、`initCraneMapWidget()`、`initOverheadCranesWidget()`
   - 在构造函数中调用 `initUI()` 初始化界面
   - 使用 `App::getConfig()->getGraphConfigPath()` 获取配置文件路径
   - 添加 `m_deviceMonitor` 成员变量

5. **修复编译错误**
   - 将 `setlevelgraphbuilder.cpp` 中的 `App::getSetOfOHBInfoByUiId` 改为 `SharedData::getSetOfOHBInfoByUiId`
   - 将 `fouplevelgraphbuilder.cpp` 中的 `App::getSetOfOHBInfoByUiId` 改为 `SharedData::getSetOfOHBInfoByUiId`
   - 将 `devicemonitorwidget.cpp` 中的 `App::setOfOHBInfoList` 改为 `SharedData::setOfOHBInfoList`
   - 所有相关文件添加 `#include "app/shareddata.h"` 头文件

#### 影响范围
- 新增文件：`app/shareddata.h`、`app/shareddata.cpp`
- 修改文件：`app/app.h`、`app/app.cpp`、`app/appconfig.h`、`app/appconfig.cpp`
- 修改文件：`ui/homepage.h`、`ui/homepage.cpp`
- 修改文件：`ui/customwidget/overheadcranetrack/setlevelgraphbuilder.cpp`
- 修改文件：`ui/customwidget/overheadcranetrack/fouplevelgraphbuilder.cpp`
- 修改文件：`ui/customwidget/overheadcranetrack/devicemonitorwidget.cpp`

---

### 2026-04-16 18:41 - Simon
**项目结构优化：添加 classes 目录**
- 新增 `classes/` 目录，用于存放数据信息相关的类文件

---

### 2026-04-16 18:00 - Simon
**编写项目框架文档**
- 创建 `PROJECT_STRUCTURE.md` 文档，详细介绍项目目录结构、bin 目录结构、app 模块实现及使用方法

---

### 2026-04-16 17:00 - Simon
**功能优化：日志系统重构**
- 日志目录拆分为调试日志和用户日志
- AppConfig 类增强：新增 `getRootDir()`、`getOSType()`、`getDebugLogDir()`、`getUserLogDir()` 方法
- 配置文件新增 `OSType` 配置项
- App 类更新：静态变量 `logDir` 拆分为 `debugLogDir` 和 `userLogDir`
- 完善代码注释

详细修改内容请参阅项目文档。
