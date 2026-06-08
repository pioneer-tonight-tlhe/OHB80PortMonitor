# ScrollingTipLabel API 文档

## 模块概述

ScrollingTipLabel 是一个滚动公告栏控件，用于实时显示系统操作日志和警报日志。该控件继承自 QLabel，具有警报优先显示、消息滚动、消息消除等功能，并支持根据消息类型自动切换样式。

**特性：**
- 警报优先显示，警报模式下使用红色背景样式
- 普通日志使用浅灰色背景样式
- 文本长度超过控件宽度时自动滚动显示
- 无消息时自动隐藏控件
- 必须在 UI 线程中使用

---

## 类定义

```cpp
class ScrollingTipLabel : public QLabel
{
    Q_OBJECT
};
```

**继承关系：**
- `QLabel` → `ScrollingTipLabel`

---

## 公共 API

### submitAlarmLog

提交一个警报日志。

```cpp
void submitAlarmLog(const AlarmRecord& record);
```

**参数：**
- `record` (const AlarmRecord&): AlarmLogDBCon 记录对象

**说明：**
- 使用复合 key "qrCode|alarmType" 作为唯一标识
- 将警报日志存储到内部映射表 `m_alarmLogs`
- 将警报 key 加入队列 `m_alarmQueue`
- 触发显示更新

---

### submitAlarmResolved

提交一个警报已解决日志。

```cpp
void submitAlarmResolved(const AlarmRecord& record);
```

**参数：**
- `record` (const AlarmRecord&): AlarmLogDBCon 记录对象

**说明：**
- 使用复合 key "qrCode|alarmType" 从警报队列 `m_alarmQueue` 中移除对应的警报记录
- 从映射表 `m_alarmLogs` 中删除对应的警报记录
- 触发显示更新

---

### submitOperationLog

提交一个 OperationLogDBCon 记录。

```cpp
void submitOperationLog(const OperationRecord& record);
```

**参数：**
- `record` (const OperationRecord&): OperationLogDBCon 记录对象

**说明：**
- 替换当前存储的 OperationLog 记录 `m_operationLog`
- 触发显示更新

---

## 使用示例

```cpp
#include "scrollingtiplabel.h"
#include "classes/alarmrecord.h"
#include "classes/operationrecord.h"

// 创建控件
auto *tipLabel = new ScrollingTipLabel(this);
tipLabel->setFixedHeight(30);
tipLabel->setFixedWidth(400);

// 提交操作日志
OperationRecord opRecord;
opRecord.occurTime = "2026-05-07 16:00:00";
opRecord.logType = static_cast<int>(LogDB::OperationLogType::Message);
opRecord.description = "系统启动成功";
tipLabel->submitOperationLog(opRecord);

// 提交警报日志
AlarmRecord alarmRecord;
alarmRecord.qrCode = "12345";
alarmRecord.alarmType = static_cast<int>(AlarmType::DeviceOffline);
alarmRecord.occurTime = "2026-05-07 16:05:00";
alarmRecord.description = "设备连接断开";
tipLabel->submitAlarmLog(alarmRecord);

// 提交警报已解决
tipLabel->submitAlarmResolved(alarmRecord);

// 再次提交操作日志（替换之前的操作日志）
opRecord.description = "设备重新连接";
tipLabel->submitOperationLog(opRecord);
```

---

## 注意事项

1. **UI 线程使用**：该控件为 UI 控件，必须在 UI 线程中使用。调度层通过信号传递日志，调度层不需要知道该模块的存在
2. **内存管理**：警报队列和映射表会持续增长，建议定期清理已解决的警报
3. **滚动效果**：使用 QPainter 实现像素级精确滚动，支持无缝循环
4. **显示优先级**：警报日志优先级最高，只要警报队列不空，公告栏优先显示警报
5. **自动隐藏**：当没有任何可显示的消息时，控件会自动隐藏；有新消息时自动显示
6. **样式切换**：控件会根据当前显示的消息类型自动切换样式（警报模式/普通模式）
7. **警报标识**：使用 "qrCode|alarmType" 复合 key 作为警报唯一标识，避免依赖异步 DB 主键
