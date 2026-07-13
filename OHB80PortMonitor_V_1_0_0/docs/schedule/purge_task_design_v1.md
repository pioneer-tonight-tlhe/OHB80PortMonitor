# 充气任务设计稿（第一版：必要数据类型）

## 1. 文档目标

本文档只讨论充气任务的必要数据类型设计，先把“任务定义长什么样、运行期最少保留什么信息、采样记录长什么样”定下来。

本稿暂不展开以下内容：

- 阶段切换时是否先停流量
- 任务失败后的硬件收尾策略
- 配置文件格式与 UI 录入方式

---

## 2. 设计原则

### 2.1 生命周期状态枚举只保留一份

`Task / Stage / Action` 三层都存在相同的生命周期概念：

- Pending
- Running
- Finished
- Failed
- Cancelled

因此不再分别定义 `PurgeTaskState`、`PurgeStageState`、`PurgeActionState`。

统一改为：

```cpp
enum class PurgeExecutionState {
    Pending,
    Running,
    Finished,
    Failed,
    Cancelled
};
```

这份枚举可同时用于：

- 任务整体状态
- 阶段状态
- 行为状态

### 2.2 动作只记录下发是否成功

原始想法中的 `Sending`、`Sent` 更像“指令下发过程状态”，而不是任务生命周期状态。

因此它们不应放进 `PurgeExecutionState` 中，否则会让 `Task / Stage / Action` 三层共用枚举变得语义混乱。

当前版本也不再额外定义“动作下发状态枚举”。

动作层只保留两个最直接的信息：

- 指令是否下发成功
- 如果失败，失败原因是什么

### 2.3 定义层、任务类成员、采样记录分离

本设计把数据分成三类：

- 定义层：描述“应该怎么执行”
- 任务类成员：描述“现在执行到哪里了”
- 记录层：描述“每秒采样一条什么数据”

由于充气任务是普通一次性任务，任务结束后对象即可销毁，因此不再额外设计 `TaskRuntime / StageRuntime / ActionRuntime` 三套运行期结构体。

这样后续做调度器、UI、CSV 落盘时职责仍然清晰，但模型更轻。

### 2.4 行为参数统一使用 `QJsonObject`

行为参数是面向具体指令的，字段会随指令变化，因此统一使用：

```cpp
QJsonObject params;
```

例如：

```json
{ "flow_l_min": 10 }
```

---

## 3. 共享枚举定义

```cpp
enum class PurgeExecutionState {
    Pending,
    Running,
    Finished,
    Failed,
    Cancelled
};
```

命名结论：

- `PurgeExecutionState`：共享生命周期状态，三层通用

---

## 4. 定义层数据类型

### 4.1 行为定义

```cpp
struct PurgeActionDefinition {
    QString actionId;         // 行为唯一标识，例如 "stage1_action1"
    QString commandId;        // 指令名，例如 "WritePurgeFlow"
    QJsonObject params;       // 指令参数，例如 { "flow_l_min": 10 }
    int timeoutMs = 3000;     // 单动作超时时间
    bool required = true;     // true=失败即中断任务，false=可容忍失败
};
```

说明：

- `actionId` 用于日志、UI、CSV 和错误定位
- `commandId` 对应底层命令池中的指令名
- `params` 用于表达与具体指令绑定的参数
- `required` 用于后续边界策略扩展

### 4.2 阶段定义

```cpp
struct PurgeStageDefinition {
    int stageNo = 0;                          // 阶段序号，建议从 1 开始
    QString stageName;                        // 阶段名称，例如 "阶段1"
    int durationSec = 0;                      // 阶段持续时间
    QVector<PurgeActionDefinition> actions;   // 阶段内行为列表
};
```

说明：

- `stageNo` 不依赖数组下标，便于日志、文件名、UI 展示
- `durationSec` 是该阶段的阶段总时长
- `actions` 表示进入该阶段后需要执行的动作集合

### 4.3 任务定义

```cpp
struct PurgeTaskDefinition {
    QString taskId;                           // 任务 ID，可运行时生成
    QString qrCode;                           // 目标设备二维码，例如 "12001"
    int totalDurationSec = 0;                 // 总时长，例如 600
    QVector<PurgeStageDefinition> stages;     // 阶段列表
};
```

说明：

- `qrCode` 明确本任务是单设备任务
- `totalDurationSec` 作为配置值和校验值存在
- 后续启动前应校验 `sum(stage.durationSec) == totalDurationSec`

---

## 5. 运行期信息

### 5.1 不再单独定义三套 Runtime 结构体

当前方案不再定义：

- `PurgeActionRuntime`
- `PurgeStageRuntime`
- `PurgeTaskRuntime`

原因很简单：

- 这是普通一次性任务
- 任务结束后对象就会被销毁
- 没必要再包一层 `context / runtime` 数据模型

### 5.2 运行期信息直接放到任务类成员

运行期只保留最少必要字段，直接作为任务类成员即可，例如：

```cpp
PurgeTaskDefinition m_definition;

PurgeExecutionState m_taskState = PurgeExecutionState::Pending;

int m_currentStageIndex = -1;
int m_currentActionIndex = -1;

bool m_lastCommandSentSuccess = false;
QString m_lastError;

QDateTime m_taskStartedAt;
QDateTime m_taskFinishedAt;
QDateTime m_currentStageStartedAt;

QString m_taskOutputDir;
```

说明：

- `m_definition` 保存本次任务的完整定义
- `m_currentStageIndex / m_currentActionIndex` 表示当前执行位置
- `m_lastCommandSentSuccess` 只记录最近一次动作下发是否成功
- `m_lastError` 用于记录失败原因
- 时间字段用于阶段计时、任务计时和结果总结
- `m_taskOutputDir` 用于保存本次 CSV 输出目录

---

## 6. 记录层数据类型

### 6.1 采样记录

```cpp
struct PurgeSampleRecord {
    QDateTime timestamp;

    QString qrCode;
    int stageNo = 0;
    QString stageName;

    double inletPressure = 0.0;
    double negativePressure = 0.0;
    double inletFlow = 0.0;
    double humidity = 0.0;
    double temperature = 0.0;

    bool foupIn = false;
    bool idlePurgeEnabled = false;
    int idleState = 0;
    quint16 idleWorkingTimeSec = 0;
};
```

说明：

- 每秒采样一次，直接写入 `stage_x.csv` 和 `stage_all.csv`
- 采样来源默认来自共享内存中的 `FoupOfOHBInfo`
- `stageNo`、`stageName` 在写 CSV 时一并记录，方便后续绘图与排查

---

## 7. 层级关系

```text
PurgeTaskDefinition
  -> QVector<PurgeStageDefinition>
       -> QVector<PurgeActionDefinition>

PurgeTask 类成员
  -> 保存当前执行位置、最近一次下发结果、时间信息、输出目录

PurgeSampleRecord
  -> 独立按秒生成，用于 CSV 持久化
```

---

## 8. 第一版结论

第一版必要数据类型结论如下：

1. 任务、阶段、行为三层共用 `PurgeExecutionState`
2. 动作层不再引入额外的下发状态枚举，只记录“是否下发成功”
3. 核心数据分为定义层、任务类轻量运行字段、记录层
4. 行为参数统一使用 `QJsonObject`
5. 采样记录独立建模，直接服务于后续 CSV 落盘
6. 运行期信息直接放任务类成员中，不额外定义三套 runtime struct

---

## 9. 本轮新增设计与待讨论项

### 9.1 阶段开始计时的判定规则

先区分两个概念：

- 进入阶段：调度器已经切到这个阶段，开始准备执行该阶段动作
- 阶段计时开始：该阶段已经真正生效，开始消耗 `durationSec`

当前版本建议采用下面这套最简单、也最贴近现有工程的规则。

#### 规则 1：不要在“刚进入阶段”时立即开始计时

原因：

- 进入阶段后通常还要先下发动作指令
- 指令发送、重试、等待回包都需要时间
- 如果这段时间也算进阶段时长，会导致设备真正处于该阶段的有效时间变短

因此，阶段计时开始时间不能取“切换到该阶段的那一刻”。

#### 规则 2：单动作阶段，以“动作成功回包到达”作为计时起点

以阶段内只有一个动作的常见情况为例：

1. 切换到当前阶段
2. 下发该阶段动作
3. 等待 `commandFinished`
4. 如果回包成功，则此刻记为阶段开始时间，并启动阶段定时器
5. 如果回包失败，则阶段失败，任务失败，不启动阶段定时器

这个规则和现有任务模型是对得上的，因为当前工程里已经通过 `ModbusCommandSender::commandFinished` 判断一次指令是否成功，例如 [set_purge_flow_task.cpp](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_purge_flow_task/set_purge_flow_task.cpp:155)。

#### 规则 3：多动作阶段，以“最后一个必需动作成功回包到达”作为计时起点

如果一个阶段里有多个动作，建议顺序执行，不要并发执行。

推荐规则：

1. 依次下发当前阶段的动作列表
2. 每个动作都等待成功/失败结果
3. 只要某个必需动作失败，当前阶段立刻失败，任务结束
4. 当最后一个必需动作成功后，才正式启动该阶段定时器

这样定义的好处是：

- 阶段时长表示“设备已经完成本阶段准备动作后，稳定运行了多久”
- 不会把前面的参数切换时间混进阶段有效时间

#### 规则 4：阶段开始信号，也要跟着“计时启动时刻”发出

因此阶段开始相关信息应在同一个时刻更新：

- 记录 `m_currentStageStartedAt`
- 启动共享阶段定时器
- 发出“阶段开始”信号
- 将当前阶段对外视为正式开始

不要在“动作刚发出去”时就发阶段开始信号，否则 UI 和日志看到的开始时间会比真实生效时间更早。

#### 规则 5：几个边界情况的简单处理

- 阶段没有动作：进入阶段后立即开始计时
- 阶段 `durationSec == 0`：动作全部成功后，不启动定时器，直接结束该阶段并进入下一阶段
- 动作提交函数本身就返回失败：等价于动作失败，阶段失败，任务结束
- 正在等待动作成功时收到停止请求：不再启动阶段计时，直接按任务取消流程收尾

#### 规则 6：第一版先不引入“设备物理到位确认”作为计时条件

更严格的做法是：

- 下发设置流量指令成功后
- 再等待共享内存里的监控值或状态位达到目标
- 到达后才开始计时

但这会明显增加复杂度，例如要定义：

- 以哪个字段作为“阶段真正生效”的依据
- 允许的到位误差是多少
- 最长等待多久算失败

所以第一版不建议这样做。

第一版就采用：

- “动作成功回包” = “阶段可以开始计时”

这个规则简单、稳定，也最容易先落地。

#### 9.1 结论

当前推荐结论只有一句话：

`阶段计时从当前阶段最后一个必需动作成功回包的时刻开始，而不是从进入阶段或提交指令的时刻开始。`

---

### 9.2 动作失败后的终止边界

这一节只讨论“任务状态如何收口”，不讨论“失败后是否额外下发停流量指令”。

也就是说，这里先定义：

- 什么情况算动作失败
- 动作失败后，阶段和任务怎么结束
- `Failed` 和 `Cancelled` 怎么区分

至于失败后的硬件收尾动作，放到下一轮单独讨论。

#### 规则 1：第一版默认把动作失败视为任务失败

第一版建议采用最简单规则：

- 任意必需动作失败
- 当前阶段立即失败
- 整个充气任务立即失败
- 不再继续后续动作
- 不再进入后续阶段

这样做的原因很直接：

- 这是单设备充气任务，不是批处理任务
- 阶段之间是强顺序依赖，不适合“跳过当前失败动作继续往后跑”
- 先把失败边界做硬，可以减少异常路径

当前文档里的 `required` 字段先保留，作为以后扩展用。

但第一版实际落地时，建议所有动作都按 `required=true` 处理。

#### 规则 2：下面这些情况都统一算“动作失败”

第一版不要把失败原因拆得太散，统一收口即可。

以下情况全部视为动作失败：

- 任务层拿不到目标设备、`sender`、`CommandPool` 或目标指令
- 指令克隆失败
- 指令提交函数本身返回失败，或根本没有成功提交
- 等待回包后判定失败，例如超时、未收到有效响应、校验错误、设备忙
- 后续如果动作需要结果校验，而校验不通过，也算动作失败

换句话说，只要这次动作没能得到“有效成功结果”，就算失败。

#### 规则 3：底层通信可以重试，但任务层不再额外重试

当前工程里底层通信本身已经有重试机制，例如现有任务会监听 `commandTimeoutRetry`，参考 [set_purge_flow_task.cpp](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_purge_flow_task/set_purge_flow_task.cpp:250)。

因此第一版建议：

- 底层通信重试照常进行
- 任务层只等最终的 `commandFinished`
- 如果最终结果仍失败，就按动作失败收口

不要在充气任务里再叠一层“业务级重试”，避免边界变复杂。

#### 规则 4：动作失败后立即进入终止流程

一旦确认动作失败，任务应立即执行下面的终止流程：

1. 记录失败动作信息：阶段号、动作号、`commandId`、失败原因
2. 将任务状态收口为失败
3. 停止阶段相关计时器和采样计时器
4. 断开当前任务相关的指令回调连接
5. 忽略后续迟到的成功/失败回包
6. 发出一次任务完成信号 `finished(false, msg)`

核心点只有一个：

`失败一旦成立，就立刻收口，不再尝试继续推进主流程。`

#### 规则 5：阶段可以在“尚未开始计时”时就失败

这条和上一节的计时规则要配套。

因为阶段计时是在“最后一个必需动作成功回包”之后才开始，所以阶段完全可能出现下面这种情况：

1. 切到阶段 2
2. 下发阶段 2 的准备动作
3. 动作失败
4. 阶段 2 直接失败
5. 整个任务失败

也就是说：

- 阶段失败，不要求它一定已经开始计时
- 阶段失败，也不要求一定已经发过“阶段开始”信号

这在语义上是成立的，因为它表示“该阶段的准备过程失败了”。

#### 规则 6：`Cancelled` 和 `Failed` 必须严格区分

下面两类结束原因不要混在一起：

- 用户主动停止，或上层明确要求停止：`Cancelled`
- 任务自己执行过程中出错：`Failed`

推荐判定规则：

- 如果 `stop()` 已经被调用，后续即使再收到动作失败回包，最终状态也保持 `Cancelled`
- 如果没有收到停止请求，而动作自然失败，则最终状态为 `Failed`

这个口径与现有任务风格也是一致的，例如 [alarm_reset_task.cpp](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/alarm_reset_task/alarm_reset_task.cpp:158) 就是根据 `m_cancelRequested` 决定最终落到 `Cancelled` 还是 `Failed`。

#### 规则 7：完成信号只能发一次

动作失败、用户取消、阶段超时、迟到回包，这些事件可能互相竞争。

因此任务类里必须保留一个“已经收口”的标记，例如：

```cpp
bool m_finished = false;
```

一旦任务已经收口：

- 不再重复改状态
- 不再重复发 `finished(...)`
- 不再重复切阶段

这样可以避免 UI、日志和调度器收到重复完成事件。

#### 规则 8：第一版先不做失败后自动恢复或自动降级

第一版不建议加入下面这些扩展策略：

- 某个动作失败后自动跳过继续执行
- 某个动作失败后自动回退到上一阶段
- 某个动作失败后自动下发备用动作
- 某个动作失败后自动重建整套阶段计划

这些都属于后续增强策略，不属于第一版必要边界。

第一版只做最稳的方案：

- 成功就继续
- 失败就终止
- 取消就取消

#### 9.2 结论

当前推荐结论只有一句话：

`第一版充气任务按“任一必需动作失败，则当前阶段失败、整个任务失败并立即收口；只有外部 stop 才算 Cancelled”处理。`

---

### 9.3 阶段切换时是否自动下发停流量

当前结论很明确：

`阶段切换时，不自动下发停流量指令。`

这一条只针对“阶段与阶段之间的切换”，不讨论“任务最终结束时是否需要停流量收尾”。

#### 规则 1：阶段结束后直接进入下一阶段，不插入停流量动作

也就是说，阶段切换流程保持最简单：

1. 当前阶段结束
2. 进入下一阶段
3. 执行下一阶段动作列表
4. 等待下一阶段动作成功后，再按上一节规则开始计时

中间不额外插入一条“流量先置 0”的过渡动作。

#### 规则 2：如果下一阶段需要新流量，直接下发下一阶段自己的流量指令

例如：

- 阶段 1 流量是 `10 L/Min`
- 阶段 2 流量是 `30 L/Min`

那么切换时不做：

- `10 -> 0 -> 30`

而是直接做：

- `10 -> 30`

这样更符合“阶段动作自描述”的设计。

也就是说，每个阶段只负责描述“自己要设备变成什么状态”，不负责额外清理上一阶段。

#### 规则 3：如果下一阶段没有改流量动作，就默认沿用上一阶段结果

既然阶段切换时不自动停流量，那么就必须接受一个明确语义：

- 下一阶段如果没有定义新的流量动作
- 设备就继续保持上一阶段已经生效的流量状态

这个语义是合理的，因为它和“不自动插入隐式动作”的原则一致。

换句话说：

- 阶段行为以显式动作定义为准
- 文档里没写的动作，任务就不替它偷偷补

#### 规则 4：这样做的主要好处是模型简单、日志清楚

不自动停流量的好处有三个：

- 阶段切换路径更短，不多一条隐藏指令
- 日志和 CSV 更容易解释，不会出现“设计稿没写，但系统自己插了一条 0 流量”
- 失败边界更清楚，阶段切换时不会多出一类“停流量失败”的额外异常

#### 规则 5：第一版把“停流量”视为显式业务动作，不做框架隐式动作

如果后续业务确实需要某两个阶段之间先停一下，再进入下一阶段，也建议这样表达：

- 把“设置 0 流量”写成一个明确动作
- 放进某个阶段定义里

而不是让调度框架偷偷在阶段切换时自动补发。

这样后续看配置、日志、CSV、任务回放时，语义都会更一致。

#### 9.3 结论

当前推荐结论只有一句话：

`阶段切换时不自动下发停流量；如果下一阶段需要新流量，就直接执行下一阶段自己的动作；如果下一阶段没有相关动作，就沿用上一阶段已生效状态。`

---

### 9.4 CSV 文件目录与文件头定义

这一节先解决两件事：

- 项目里是否已经有可直接复用的 CSV 落盘模块
- 充气任务 CSV 的目录结构、文件名和表头怎么定

#### 规则 1：项目里有一份 CSV I/O，但不建议直接拿来作为充气任务模块

当前仓库里确实已经有一份 CSV I/O：

- [ui/customwidget/loggerwidget/csvio.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/ui/customwidget/loggerwidget/csvio.h:1)
- [ui/customwidget/loggerwidget/csvio.cpp](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/ui/customwidget/loggerwidget/csvio.cpp:1)

它已经具备这些能力：

- 写表头
- 追加一条记录
- 读取记录
- 修改记录
- 构建页表

但它目前放在 `ui/customwidget/loggerwidget/` 下面，而且头文件还依赖 logger widget 自己的分页类型。

所以对于“充气任务 CSV 落盘”这个场景，当前结论是：

- 仓库里“有 CSV 能力”
- 但“没有合适的 tool 公共 CSV 模块”

因此仍然建议补一个新的、最小化的 `tool` 模块，而不是把任务层直接依赖到 UI 目录。

#### 规则 2：新模块放到 `tool` 目录，先做最小能力

当前建议新增一个公共工具模块，例如：

```text
tool/csvio/
  csvfilewriter.h
  csvfilewriter.cpp
  csvio.pri
```

第一版只做最少必要能力：

- 确保目标目录存在
- 文件不存在时创建文件
- 首次创建时写入表头
- 以追加方式写入一条记录
- 统一处理 CSV 转义

第一版先不做这些扩展能力：

- 分页读取
- 修改中间记录
- 删除记录
- 缓存页表
- 常驻文件句柄池

这样模块保持够小，也最适合先给充气任务使用。

#### 规则 3：模块接口保持简单，优先按行追加

建议模块接口先做成这种粒度：

```cpp
class CsvFileWriter
{
public:
    static bool ensureFileWithHeader(const QString& filePath,
                                     const QStringList& headers,
                                     QString* errorMessage = nullptr);

    static bool appendRow(const QString& filePath,
                          const QStringList& headers,
                          const QStringList& row,
                          QString* errorMessage = nullptr);

    static QString joinCsvLine(const QStringList& fields);
};
```

说明：

- `ensureFileWithHeader` 负责“目录存在 + 文件存在 + 表头存在”
- `appendRow` 负责按追加方式写一行
- `joinCsvLine` 负责逗号、引号、换行的转义

第一版不强制模块支持 `QJsonObject`。

充气任务自己把采样对象转成 `QStringList` 再写入即可，这样工具模块更通用、更轻。

#### 规则 4：第一版可以每次写入时打开文件、写完即关闭

虽然任务在运行中会每秒写一次 CSV，但第一版仍建议采用最简单策略：

- 需要写时再打开文件
- 追加写入
- 立刻关闭文件

理由：

- 任务总时长只有 10 分钟量级
- 采样频率只有 1 Hz
- 这种写法最稳定，异常时也不容易丢句柄
- 也不需要在任务类里长期维护 `QFile` / `QTextStream`

如果后续确认性能不够，再升级成持久句柄方案。

#### 规则 5：输出目录结构按“任务实例”隔离

当前沿用前面讨论过的目录层级，建议结构如下：

```text
OHB_12001_Monitor_Graph/
  20260713/
    12001/
      153025/
        stage_1.csv
        stage_2.csv
        ...
        stage_all.csv
```

字段含义：

- `OHB_12001_Monitor_Graph`：任务根目录，包含二维码
- `20260713`：任务启动日期
- `12001`：目标设备二维码
- `153025`：任务启动时分秒

这样做的好处：

- 每次任务都有独立目录
- 同一设备多次执行不会互相覆盖
- 排查时可以直接按日期、二维码、任务时间定位

#### 规则 6：任务类只保存根目录和几个关键文件路径

结合当前轻量运行态设计，任务类里只需要保存：

```cpp
QString m_taskOutputDir;
QString m_stageAllCsvPath;
QString m_currentStageCsvPath;
```

当切换阶段时：

- 更新 `m_currentStageCsvPath`
- 新阶段第一次写入前，确保对应 `stage_N.csv` 已创建并带表头

#### 规则 7：文件名规则保持朴素，不做额外修饰

当前建议文件名规则如下：

- `stage_1.csv`
- `stage_2.csv`
- ...
- `stage_all.csv`

不必先引入：

- 前导零编号
- 中文文件名
- 额外时间戳后缀

目录层已经足够区分一次任务实例，文件名越简单越好。

#### 规则 8：所有 stage 文件与 `stage_all.csv` 使用同一套表头

当前建议所有 CSV 统一表头，避免后续绘图、导出、比对时再做字段适配。

推荐第一版表头如下：

```cpp
QStringList headers = {
    "timestamp",
    "qr_code",
    "stage_no",
    "stage_name",
    "inlet_pressure",
    "negative_pressure",
    "inlet_flow",
    "humidity",
    "temperature",
    "foup_in",
    "idle_purge_enabled",
    "idle_state",
    "idle_working_time_sec"
};
```

说明：

- `timestamp`：采样时间
- `qr_code`：目标设备二维码
- `stage_no / stage_name`：方便 `stage_all.csv` 按阶段过滤
- 后面字段来自共享内存中的设备快照

如果后续需要补更多监控字段，直接在这套表头末尾追加即可。

#### 规则 9：写入策略为“每秒同时写两份”

每次采样成功后：

- 追加到当前阶段文件 `stage_N.csv`
- 同时追加到总文件 `stage_all.csv`

这样后续查询时会同时得到两种视角：

- 单阶段内曲线
- 整个任务全过程曲线

#### 规则 10：第一版失败时不回滚已写 CSV

如果任务执行到一半失败：

- 已经写入的 CSV 保留
- 不删除已有目录
- 不清空已采样内容

因为这些内容本身就是问题排查数据，失败后保留更有价值。

#### 9.4 结论

当前推荐结论只有一句话：

`项目里有 UI 内部 CsvIO，但没有合适的 tool 公共 CSV 模块；充气任务应新增一个 tool 级最小 CSV 写入模块，只负责建文件、写表头、追加记录，并按“任务实例目录 + stage_N.csv / stage_all.csv + 统一表头”落盘。`

---

### 9.5 任务类成员字段与类间协作关系

这一节的目标只有两个：

- 把充气任务类自身最少需要哪些成员定下来
- 把它和现有工程里哪些类发生协作定下来

设计原则继续保持不变：

- 不额外包 `TaskRuntime / StageRuntime / ActionRuntime`
- 运行期信息直接放任务类成员
- 能复用现有工程能力的地方就复用

#### 规则 1：任务类本身就是一个普通 `SchedulerTask`

当前建议新增一个一次性任务类，例如：

```cpp
class PurgeTask : public SchedulerTask
```

它的定位很简单：

- 继承 [scheduler/scheduler_task.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/scheduler_task.h:1)
- 由调度器提交和回收
- 不是长驻任务
- 任务结束后对象即可销毁

因此它不需要再套第二层“任务执行器”对象。

#### 规则 2：成员字段只保留 6 组信息

第一版建议任务类成员只保留下面 6 组：

##### 2.1 任务定义输入

```cpp
PurgeTaskDefinition m_definition;
```

用途：

- 保存本次任务的完整阶段定义
- 任务运行过程中不再回头找 UI 控件或配置页拿数据

##### 2.2 生命周期与收口标记

```cpp
PurgeExecutionState m_taskState = PurgeExecutionState::Pending;
bool m_cancelRequested = false;
bool m_finished = false;
```

用途：

- `m_taskState`：当前任务生命周期状态
- `m_cancelRequested`：区分 `Cancelled` 和 `Failed`
- `m_finished`：保证最终完成信号只发一次

##### 2.3 当前执行位置

```cpp
int m_currentStageIndex = -1;
int m_currentActionIndex = -1;
```

用途：

- 指向当前执行到哪个阶段
- 指向当前阶段执行到哪个动作

第一版一台设备、一个动作一个动作串行执行，这两个下标就够了。

##### 2.4 当前待确认指令信息

```cpp
qint64 m_pendingCommandUuid = 0;
QString m_pendingCommandId;
bool m_lastCommandSentSuccess = false;
QString m_lastError;
```

用途：

- `m_pendingCommandUuid`：收到 `commandFinished` 时做匹配
- `m_pendingCommandId`：日志和错误定位更直接
- `m_lastCommandSentSuccess`：保留最近一次动作结果
- `m_lastError`：统一收口失败原因

第一版不再单独设计动作级运行结构体。

##### 2.5 定时器与时间戳

```cpp
QTimer m_stageTimer;
QTimer m_sampleTimer;

QDateTime m_taskStartedAt;
QDateTime m_taskFinishedAt;
QDateTime m_currentStageStartedAt;
```

用途：

- `m_stageTimer`：当前阶段共用计时器
- `m_sampleTimer`：每秒采样一次
- 时间戳用于阶段开始、阶段结束、任务总结和目录命名

这里把 `QTimer` 放到任务类成员里是合理的。

我们前面否掉的是“把 `QTimer` 放进额外 runtime struct 里”，不是否掉任务类自己持有计时器。

##### 2.6 CSV 输出信息

```cpp
QString m_taskOutputDir;
QString m_stageAllCsvPath;
QString m_currentStageCsvPath;
QStringList m_csvHeaders;
```

用途：

- 保存本次任务输出目录
- 保存当前阶段文件路径和总文件路径
- 统一保存 CSV 表头，避免到处重复构造

#### 规则 3：连接管理也放任务类自己维护，不再单独包装

当前任务一定会监听底层发送器信号，因此建议保留：

```cpp
QList<QMetaObject::Connection> m_connections;
```

用途：

- 统一断开 `commandFinished`
- 统一断开 `commandTimeoutRetry`
- 任务收口时快速清理连接

这和现有单设备/多设备命令任务风格一致，例如 [scheduler/tasks/send_command_task/send_command_task.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/send_command_task/send_command_task.h:1) 和 [scheduler/tasks/set_purge_flow_task/set_purge_flow_task.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/set_purge_flow_task/set_purge_flow_task.h:1) 都是这样做的。

#### 规则 4：第一版不强制任务类持有独立 `ILogger`

当前项目已经有统一业务日志入口：

- [app/shareddata.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/app/shareddata.h:55) `getOperationDispatchTask()`
- [scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h:1)

所以第一版建议：

- 任务运行日志优先走 `OperationDispatchTask`
- 不强制再给 `PurgeTask` 加一套独立 `ILogger` 成员

如果后续发现需要专门的设备明细文本日志，再单独补。

#### 规则 5：任务类方法保持“小而固定”

当前建议最少方法如下：

```cpp
void start() override;
void stop() override;

void startNextStage();
void submitCurrentAction();
void startStageTiming();

void onCommandFinished(ModbusCommand cmd, const QString& masterId);
void onCommandTimeoutRetry(ModbusCommand cmd, const QString& masterId);
void onStageTimeout();
void onSampleTimeout();

void appendCurrentSample();
void finishTask(bool success, const QString& message);
void disconnectAll();
```

说明：

- `start / stop`：调度器生命周期入口
- `startNextStage`：推进阶段
- `submitCurrentAction`：下发当前动作
- `startStageTiming`：在最后一个必需动作成功后启动阶段计时
- `onCommandFinished`：接最终动作结果
- `onStageTimeout`：阶段结束后推进下一阶段
- `onSampleTimeout`：每秒采样一次
- `appendCurrentSample`：把共享内存快照写到 CSV
- `finishTask`：统一收口，避免多个出口各写一遍

第一版不建议再拆成更多 helper class。

#### 规则 6：任务类不负责“生成任务定义”，只负责“执行任务定义”

边界要切清楚：

- `PurgeTask` 负责执行
- `PurgeTaskDefinition` 负责描述任务
- 谁来构造 `PurgeTaskDefinition`，由上层决定

也就是说，`PurgeTask` 不直接依赖：

- 配置页控件
- ini 文件解析逻辑
- 某个固定 UI 页面对象

推荐做法是：

- UI 或上层调用方先准备好 `PurgeTaskDefinition`
- 再把它传给 `PurgeTask`

这样任务类才会保持干净。

#### 规则 7：类间协作关系只保留 5 条主链路

第一版建议把协作关系收成下面 5 条：

##### 7.1 调度器链路

```text
UI / 上层调用方
  -> Scheduler::submitTask(...)
      -> PurgeTask::start()
```

职责：

- 上层创建任务并提交
- `Scheduler` 负责生命周期和回收
- `PurgeTask` 只关心自己的执行

##### 7.2 指令下发链路

```text
PurgeTask
  -> ModbusTcpMasterManager
      -> CommandPool
      -> ModbusTcpMaster
      -> ModbusCommandSender
          -> commandFinished / commandTimeoutRetry
              -> PurgeTask
```

对应现有接口位置：

- [data/modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/data/modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h:1)
- [scheduler/tasks/send_command_task/send_command_task.cpp](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/scheduler/tasks/send_command_task/send_command_task.cpp:48)

职责：

- `PurgeTask` 只负责组织动作和判断结果
- 底层发送、超时重试、回包匹配仍由现有 Modbus 链路负责

##### 7.3 设备快照链路

```text
PurgeTask
  -> SharedData::getFoupByQRCode(qrCode)
      -> FoupOfOHBInfo
```

职责：

- 采样时直接按二维码拿共享内存里的设备快照
- 不重复维护第二份监控数据缓存

对应接口位置：

- [app/shareddata.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/app/shareddata.h:25)

##### 7.4 运行日志链路

```text
PurgeTask
  -> SharedData::getOperationDispatchTask()
      -> OperationDispatchTask::log(...)
```

职责：

- 记录任务开始、阶段开始、动作失败、任务完成等业务日志
- 不让 `PurgeTask` 直接操作运行日志数据库

##### 7.5 CSV 落盘链路

```text
PurgeTask
  -> tool/csvio/CsvFileWriter
      -> stage_N.csv / stage_all.csv
```

职责：

- `PurgeTask` 负责决定什么时候写、写什么内容
- `CsvFileWriter` 只负责建文件、写表头、追加一行

#### 规则 8：第一版不新增“任务管理器”或“阶段执行器”中间类

当前不建议立刻再引入：

- `PurgeTaskManager`
- `PurgeStageRunner`
- `PurgeActionExecutor`
- `PurgeCsvService`

原因很简单：

- 现在只有一个普通单设备任务
- 阶段执行是串行的
- 协作链路并不复杂

第一版先让 `PurgeTask` 自己把主流程跑通更合适。

如果以后真的出现：

- 多种 purge 任务并存
- 多设备并行任务
- 多处复用同一套阶段执行逻辑

再拆中间层会更自然。

#### 9.5 结论

当前推荐结论只有一句话：

`第一版充气任务类只保留“任务定义、生命周期标记、当前执行位置、待确认指令、两个计时器、CSV 输出路径、连接列表”这些最少成员；类间协作只经过 Scheduler、Modbus 下发链、SharedData 快照、OperationDispatchTask 日志入口和 tool/csvio 写盘模块，不再额外引入中间执行器。`

---

### 9.6 任务对外信号与 UI 联动边界

这一节先对现有 UI 图表模块做结论，然后定义 `PurgeTask` 和 `ChartPage` 的协作边界。

#### 规则 1：ChartPage 当前还是占位页，图表能力来自 chartmanager

当前 [ui/chartpage.ui](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/ui/chartpage.ui:1) 只有一个占位 `QLabel`。

可复用的曲线模块在：

- [ui/customwidget/chartmanager/datamonitorchartplotmanager.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/ui/customwidget/chartmanager/datamonitorchartplotmanager.h:1)
- [ui/customwidget/chartmanager/datamonitorchartplot.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/ui/customwidget/chartmanager/datamonitorchartplot.h:1)
- [ui/customwidget/chartmanager/chartgraph.h](D:/Project/CYTC_Project/OHB80PortMonitor/OHB80PortMonitor_V_1_0_0/ui/customwidget/chartmanager/chartgraph.h:1)

这个模块的使用方式很适合本需求：

1. `ChartPage` 放一个 `QCustomPlot`
2. 调用 `DataMonitorChartPlotManager::registerPlot(plotId, customPlot)`
3. 调用 `addGraph(...)` 添加多条曲线
4. 每次 UI 计时器超时，调用 `refreshGraphs(plotId, values)` 批量刷新

底层 `QCustomPlot` 也已经支持 `savePng(...)`，所以 finish 成功后可以直接保存曲线图截图。

#### 规则 2：ChartPage 第一行只负责实时曲线显示

第一行 UI 放一个曲线图，使用 `chartmanager` 模块管理。

曲线固定 5 条：

- 进气压力：`FoupOfOHBInfo::inletPressure()`
- 出气压力/负压：第一版先取 `FoupOfOHBInfo::negativePressure()`
- 进气流量：`FoupOfOHBInfo::inletFlow()`
- 相对湿度：`FoupOfOHBInfo::RH()`
- 温度：`FoupOfOHBInfo::temperature()`

说明：

- `FoupOfOHBInfo` 里有 `outletPressure()`，但当前监控任务实际更新的是 `negativePressure()`。
- 因此第一版 UI 文案可以叫“出气压力/负压”，实际取值先用 `negativePressure()`。
- 如果后续底层开始更新 `outletPressure()`，再把曲线来源切过去。

#### 规则 3：ChartPage 使用自己的 100ms 计时器刷新曲线

ChartPage 自己持有一个 UI 计时器：

```cpp
QTimer m_chartRefreshTimer;
```

计时器规则：

- 间隔：`100 ms`
- 页面初始化后即可启动
- 每次超时通过当前 `qrCode` 读取共享内存
- 拿到设备快照后刷新 5 条曲线

刷新链路：

```text
ChartPage::onChartRefreshTimeout()
  -> SharedData::getFoupByQRCode(m_currentQrCode)
      -> FoupOfOHBInfo
          -> DataMonitorChartPlotManager::refreshGraphs(...)
```

这里刻意让 UI 自己轮询共享内存，而不是让 `PurgeTask` 高频发曲线数据。

原因：

- 曲线刷新属于 UI 展示行为
- 100ms 高频信号不适合从任务层一路 emit 到 UI
- 任务层已经有自己的 1s CSV 采样，不需要和 UI 100ms 曲线刷新绑定

#### 规则 4：ChartPage 第二行放任务控制控件

第二行 UI 放任务控制控件，第一版只保留 3 个按钮：

- 开始任务
- 结束任务
- 打开记录

按钮初始状态：

- `开始任务`：可用
- `结束任务`：不可用
- `打开记录`：不可用

任务运行中：

- `开始任务`：不可用
- `结束任务`：可用
- `打开记录`：不可用

用户点击结束任务后：

- `开始任务`：不可用
- `结束任务`：不可用
- `打开记录`：不可用
- 页面进入“充气业务即将结束”状态
- 等待充气任务最终 `finish` 信号

任务结束后：

- `开始任务`：重新可用
- `结束任务`：不可用
- `打开记录`：只有在记录目录有效时才可用

#### 规则 5：ChartPage 的 qrcode 来自任务配置

当前用户设计里没有额外提出设备选择控件，因此第一版先不额外加 qrcode 下拉框。

第一版建议：

- `ChartPage` 初始化时从充气任务配置读取默认 `qrCode`
- 曲线刷新和任务启动都使用这个 `qrCode`

如果后续需要多设备切换，再补一个设备选择控件。

#### 规则 6：任务参数需要新增独立配置文件和 config 类

充气任务阶段列表不建议塞进 `ohb_device.ini`。

建议新增独立配置文件：

```text
bin/config/purge_task.ini
```

建议新增配置类：

```text
config/purgetaskconfig.h
config/purgetaskconfig.cpp
```

第一版只需要读取方法，不急着做 UI 写入：

```cpp
class PurgeTaskConfig
{
public:
    static PurgeTaskConfig& getInstance();

    QString readDefaultQRCode() const;
    PurgeTaskDefinition readTaskDefinition() const;
    PurgeTaskDefinition readTaskDefinition(const QString& qrCode) const;
    QString getConfigPath() const;
};
```

职责边界：

- `PurgeTaskConfig` 只负责从配置文件读取任务定义
- `ChartPage` 负责调用配置类创建 `PurgeTaskDefinition`
- `PurgeTask` 只负责执行传入的 `PurgeTaskDefinition`

#### 规则 7：配置文件第一版采用单任务定义

第一版配置文件先支持一个默认任务即可，不做多套 recipe。

建议格式：

```ini
[Task]
DefaultQRCode=12001
TotalDuration_s=600
StageCount=2

[Stage1]
Name=Stage 1
Duration_s=30
ActionCount=1
Action1_CommandId=WritePurgeFlow
Action1_Params={"flow_l_min":10}

[Stage2]
Name=Stage 2
Duration_s=570
ActionCount=1
Action1_CommandId=WritePurgeFlow
Action1_Params={"flow_l_min":30}
```

读取规则：

- `DefaultQRCode` 用于初始化 ChartPage 当前设备
- `TotalDuration_s` 用于任务定义校验
- `StageCount` 决定读取多少个阶段
- 每个阶段读取 `Duration_s` 和动作列表
- `ActionN_Params` 使用 JSON 字符串解析成 `QJsonObject`

#### 规则 8：PurgeTask 对外只发低频生命周期信号

任务层不要发实时曲线数据。

第一版建议 `PurgeTask` 对 UI 暴露这些信号：

```cpp
signals:
    void outputDirectoryReady(const QString& outputDir);

    void purgeStarted(const QString& qrCode);
    void purgeStageStarted(int stageNo, const QString& stageName, int durationSec);
    void purgeStageFinished(int stageNo, const QString& stageName);

    void purgeFinished(bool success,
                       const QString& message,
                       const QString& outputDir);
```

说明：

- `outputDirectoryReady`：目录创建成功后发出，UI 先缓存，不立即启用打开记录
- `purgeStarted`：任务真正启动后发出
- `purgeStageStarted`：阶段计时正式开始时发出
- `purgeStageFinished`：阶段计时结束时发出
- `purgeFinished`：业务完成信号，给 UI 判断弹框、截图、打开记录按钮状态

同时仍然要发基类的：

```cpp
emit finished(success, message);
```

基类 `finished` 给 `Scheduler` 使用，自定义 `purgeFinished` 给 `ChartPage` 使用。

#### 规则 9：开始按钮只做“读配置、建任务、连信号、提交调度器”

开始按钮流程：

1. 从 `PurgeTaskConfig` 读取默认 `qrCode`
2. 从 `PurgeTaskConfig` 读取 `PurgeTaskDefinition`
3. 校验任务定义
4. 清空图表旧数据
5. 创建 `PurgeTask`
6. 连接任务信号
7. 提交到 `Scheduler`
8. 更新按钮状态为运行中

ChartPage 保留：

```cpp
QPointer<PurgeTask> m_runningTask;
QString m_currentQrCode;
QString m_currentOutputDir;
QString m_lastValidOutputDir;
bool m_taskRunning = false;
bool m_waitingForStopFinish = false;
```

#### 规则 10：结束按钮只请求停止，不直接当作任务已结束

结束按钮流程：

1. 判断 `m_runningTask` 是否存在
2. 设置 `m_waitingForStopFinish = true`
3. UI 显示“充气业务即将结束”
4. 调用 `m_runningTask->stop()` 或通过调度器取消任务
5. 等待 `purgeFinished / finished` 信号

注意：

- 点击结束按钮不代表任务已经结束
- UI 不在点击结束时弹最终结果
- 最终结果只以任务 finish 信号为准

#### 规则 11：打开记录必须检查目录有效性

打开记录按钮点击时：

1. 取 `m_lastValidOutputDir`
2. 检查目录非空
3. 检查目录存在
4. 检查目录下至少有 `stage_all.csv` 或某个 `stage_N.csv`
5. 检查通过后再调用 `QDesktopServices::openUrl(...)`

如果检查失败：

- 不打开目录
- 弹框提示“记录目录无效或尚未生成有效记录”

#### 规则 12：finish 到来后的 UI 处理

收到 `purgeFinished(success, message, outputDir)` 后：

1. 停止等待结束状态
2. 清空 `m_runningTask`
3. 检查 `outputDir` 是否有效
4. 更新按钮状态
5. 根据成功/失败弹框

失败时：

- 弹框提示任务失败和失败原因
- `打开记录` 可能不可用
- 如果目录有效，可以允许打开记录；如果目录无效，不启用打开记录

成功时：

- 检查记录目录有效性
- 有效则保存曲线图截图
- 有效则启用 `打开记录`
- 弹框提示任务完成

截图规则：

```text
<outputDir>/chart_snapshot.png
```

第一版直接使用 `QCustomPlot::savePng(...)`。

#### 规则 13：ChartPage 不负责写 CSV，PurgeTask 不负责画图

边界最终定为：

- `PurgeTask`：执行阶段、写 CSV、发任务生命周期信号
- `ChartPage`：展示曲线、控制按钮、弹框、打开目录、保存图表截图
- `PurgeTaskConfig`：读取任务参数
- `chartmanager`：管理 QCustomPlot 曲线
- `SharedData`：提供当前设备快照

这样不会出现任务层和 UI 层互相抢职责。

#### 9.6 结论

当前推荐结论只有一句话：

`ChartPage 用 chartmanager + 100ms UI 计时器从 SharedData 刷新 5 条实时曲线；PurgeTask 只向 UI 发低频生命周期和输出目录信号；开始按钮读取 PurgeTaskConfig 并提交任务，结束按钮只请求停止并等待 finish，记录目录和截图只在 finish 后检查有效性再处理。`

---

### 9.7 仍待讨论项

下一轮设计建议继续讨论以下内容：

- 代码落地顺序
