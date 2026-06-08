# 日志数据库 user_permission 字段 — API 文档

## 模块概述

为四张日志表新增 `user_permission` 字段，用于记录"触发该日志的用户当时所处权限级别"。该字段类型为 `INTEGER NOT NULL DEFAULT 0`，对应枚举 `UserPermission`（见 `data/usermanager/usermanager.h`）：

| 枚举值          | 数值 | 说明     |
| --------------- | ---- | -------- |
| `Guest`         | 0    | 游客（默认） |
| `Normal`        | 1    | 普通用户 |
| `Debug`         | 2    | 调试用户 |
| `Engineer`      | 3    | 工程师   |
| `Root`          | 4    | root 用户 |

涉及表：

- `operation_log`（运行/操作日志）
- `communicate_log`（通讯日志）
- `alarm_log`（警报日志）
- `device_param_log`（设备参数日志）

## 公共 API

所有 4 个 `*DBCon` 类的 `insertRecord` 都新增**末位 `int userPermission = 0`** 参数（默认值 = `UserPermission::Guest`）。

> 兼容性：旧调用方不传该参数会自动按 `Guest` 入库；新调用方建议显式传 `static_cast<int>(UserManager::instance()->currentPermission())`。

### 1. `LogDB::OperationLogDBCon::insertRecord`

```cpp
void insertRecord(const QString& occurTime,
                  int            logType,
                  const QString& description,
                  int            userPermission = 0);   // 新增
```

### 2. `LogDB::AlarmLogDBCon::insertRecord`

```cpp
void insertRecord(int            alarmLevel,
                  const QString& occurTime,
                  const QString& qrCode,
                  const QString& alarmType,
                  int            isResolved,
                  const QString& resolveTime,
                  const QString& description,
                  int            userPermission = 0);   // 新增
```

### 3. `LogDB::CommunicateLogDBCon::insertRecord`

```cpp
void insertRecord(const QString&   sendTime,
                  const QString&   responseTime,
                  const QString&   commandId,
                  const QString&   qrCode,
                  int              execStatus,
                  int              retryCount,
                  const QByteArray& sendFrame,
                  const QByteArray& responseFrame,
                  const QString&   description,
                  int              userPermission = 0); // 新增
```

### 4. `LogDB::DeviceParamLogDBCon::insertRecord`

```cpp
void insertRecord(const QString& qrCode,
                  const QString& recordTime,
                  double         inletPressure,
                  double         outletPressure,
                  double         inletFlow,
                  double         humidity,
                  double         temperature,
                  int            foupStatus,
                  int            userPermission = 0);   // 新增
```

### 实时事件信号

`recordInserted(const QVariantMap& row)` 信号派发的 `row` 中新增 `"user_permission"` 字段（`AlarmLog / OperationLog / CommunicateLog`）。

### 查询接口

无变化。`queryPageWithConditions` 等返回的 `QList<QVariantMap>` 仍然是 `SELECT *`，因此结果 `row` 自动携带新增的 `user_permission` 列。

## 使用示例

### 示例 1：插入操作日志（旧代码无须改动）

```cpp
auto* db = LogDB::DatabaseManager::instance().operationLogCon();
QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
db->insertRecord(now, 1, "系统启动成功");      // 默认 Guest
```

### 示例 2：携带当前登录用户权限插入

```cpp
auto* db   = LogDB::DatabaseManager::instance().alarmLogCon();
int   perm = static_cast<int>(UserManager::instance()->currentPermission());

db->insertRecord(2, now, qrCode, "PressureAbnormal",
                 0, "", 1, "进气压力超限", perm);
```

### 示例 3：在 UI 端读取并显示权限

```cpp
QList<QVariantMap> rows = db->queryPageWithConditions(/* ... */);
for (const auto& row : rows) {
    int perm = row.value("user_permission").toInt();
    QString permStr = UserManager::permissionToString(
        static_cast<UserPermission>(perm));
    // 把 permStr 显示到表格
}
```

## 注意事项

- **类型**：参数是 `int` 而不是 `UserPermission`，便于跨线程 `QMetaObject::invokeMethod` 传递；调用方需要 `static_cast<int>(...)`。
- **取值范围**：建议传 0~4；超出范围不会报错（无 `CHECK` 约束），但会失去语义。
- **老库迁移**：启动时由 `create_operation_log.sql` 中的 `ALTER TABLE ... ADD COLUMN` 语句自动添加列，老数据 `user_permission` 自动填 0。新库执行同样 SQL 时该 ALTER 会失败（列已存在），属正常告警，不影响运行。
- **索引**：`user_permission` 当前未建索引，如果后续按权限筛选量较大，可在 `create_operation_log.sql` 内追加 `CREATE INDEX IF NOT EXISTS ...`。
