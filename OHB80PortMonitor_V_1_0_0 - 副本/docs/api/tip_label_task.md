# TipLabelTask API 文档

## 模块概述

TipLabelTask 是一个常驻调度任务，专门用于为 ScrollingTipLabel UI 控件采集日志数据。它继承自 SchedulerTask，运行在调度器线程中，通过生产者-消费者模式安全地将来自任意线程的日志提交转发给 UI 控件。

**特性：**
- 常驻任务（isPersistent = true），随调度器启动，全生命周期存在
- 提供与 ScrollingTipLabel 相同的公开接口（submitAlarmLog / submitAlarmResolved / submitOperationLog）
- 内部队列线程安全，支持任意线程调用
- 通过信号将记录转发给 ScrollingTipLabel，调用方无需知道 UI 控件的存在

---

## 类定义

```cpp
class TipLabelTask : public SchedulerTask
{
    Q_OBJECT
};
```

**继承关系：**
- `QObject` → `SchedulerTask` → `TipLabelTask`

---

## 公共 API

### submitAlarmLog

提交一个警报日志（线程安全）。

```cpp
void submitAlarmLog(const QStringList& operationLog, int alarmRecordId);
```

**参数：**
- `operationLog` (const QStringList&): OperationLogDBCon 记录
- `alarmRecordId` (int): AlarmLogDBCon 的警报记录的主键 ID

**说明：**
- 将记录加入内部队列
- 通过 `QMetaObject::invokeMethod(Qt::QueuedConnection)` 触发消费
- 最终 emit `alarmLogReady` 信号

---

### submitAlarmResolved

提交一个警报已解决（线程安全）。

```cpp
void submitAlarmResolved(int alarmRecordId);
```

**参数：**
- `alarmRecordId` (int): AlarmLogDBCon 的警报记录的主键 ID

---

### submitOperationLog

提交一个操作日志（线程安全）。

```cpp
void submitOperationLog(const QStringList& operationLog);
```

**参数：**
- `operationLog` (const QStringList&): OperationLogDBCon 记录

---

## 信号

| 信号 | 说明 | 连接目标 |
|---|---|---|
| `alarmLogReady(QStringList, int)` | 警报日志就绪 | `ScrollingTipLabel::submitAlarmLog` |
| `alarmResolvedReady(int)` | 警报已解决 | `ScrollingTipLabel::submitAlarmResolved` |
| `operationLogReady(QStringList)` | 操作日志就绪 | `ScrollingTipLabel::submitOperationLog` |

---

## 使用示例

### 业务侧提交（任意线程）

```cpp
// 提交操作日志
SharedData::getTipLabelTask()->submitOperationLog({"2026-05-07 16:00:00", "INFO", "系统启动成功"});

// 提交警报日志
SharedData::getTipLabelTask()->submitAlarmLog({"2026-05-07 16:05:00", "ALARM", "设备连接断开"}, 1001);

// 提交警报已解决
SharedData::getTipLabelTask()->submitAlarmResolved(1001);
```

### UI 端连接（UIDemo6::connectTipLabelTask）

```cpp
void UIDemo6::connectTipLabelTask()
{
    TipLabelTask* task = SharedData::getTipLabelTask();
    connect(task, &TipLabelTask::alarmLogReady,
            ui->scrollingTipLabel, &ScrollingTipLabel::submitAlarmLog,
            Qt::QueuedConnection);
    connect(task, &TipLabelTask::alarmResolvedReady,
            ui->scrollingTipLabel, &ScrollingTipLabel::submitAlarmResolved,
            Qt::QueuedConnection);
    connect(task, &TipLabelTask::operationLogReady,
            ui->scrollingTipLabel, &ScrollingTipLabel::submitOperationLog,
            Qt::QueuedConnection);
}
```

---

## 注意事项

1. **初始化顺序**：`connectTipLabelTask()` 必须在 `SharedData::initScheduler()` 之后调用
2. **调用方无感**：业务层调用 `SharedData::getTipLabelTask()->submitXxx()` 即可，无需关心 UI 控件
3. **线程安全**：所有 submit* 方法均通过 QMutex 保护，可安全跨线程调用
4. **自动流转**：记录由调度器线程消费后，信号经 Qt::QueuedConnection 投递到 UI 线程
