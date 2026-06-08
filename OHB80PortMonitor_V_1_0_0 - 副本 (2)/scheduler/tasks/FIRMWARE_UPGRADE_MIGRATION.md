# 固件升级任务迁移说明

## 概述

旧代码 `old/firmware_upgrade_task.{h,cpp}` 已成功迁移到新的项目架构中，位于 `scheduler/tasks/firmware_upgrade_task.{h,cpp}`。

## 主要变更

### 1. 类名和命名空间调整

- **旧版**: `MtmFirmwareUpgrader` (位于 `old/mtm_firmware_upgrader.{h,cpp}`)
- **新版**: `FirmwareUpgrader` (位于 `data/modbustcpmastermanager/modbustcpmaster/firmwareupgrader.{h,cpp}`)

### 2. 信号接口变更

#### 旧版信号
```cpp
void stateChanged(MtmFirmwareUpgrader::UpgradeState state,
                  const QString &logMessage,
                  const QByteArray &frame);

void finished(bool success,
              MtmFirmwareUpgrader::UpgradeState state,
              const QString &errorMessage);

void progress(int percent);
```

#### 新版信号
```cpp
void stateChanged(const QString &masterId,
                  FirmwareUpgrader::UpgradeState state,
                  const QString &logMessage,
                  const QByteArray &frame);

void finished(const QString &masterId,
              bool success,
              FirmwareUpgrader::UpgradeState state,
              const QString &errorMessage);

void progress(const QString &masterId, int percent);
```

**变更说明**: 新版信号增加了 `masterId` 参数，便于在多设备升级场景中识别设备。

### 3. 参数名称变更

- **旧版**: 使用 `qrcode` 作为设备标识
- **新版**: 使用 `deviceId` 作为设备标识（与 ModbusTcpMaster 的 ID 字段保持一致）

### 4. 日志格式统一

新版代码遵循项目日志格式规范：`[Scheduler][FirmwareUpgradeTask][方法名]`

示例：
```cpp
qDebug() << "[Scheduler][FirmwareUpgradeTask][start] 固件升级任务启动";
LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, 
    "[Scheduler][FirmwareUpgradeTask][start] 固件升级任务启动");
```

### 5. Manager 访问方式

- **旧版**: `ModbusTcpMasterManager::instance()` (返回指针)
- **新版**: `ModbusTcpMasterManager::instance()` (返回引用)

```cpp
// 旧版
ModbusTcpMasterManager *manager = ModbusTcpMasterManager::instance();
ModbusTcpMaster *master = manager->getMaster(qrcode);

// 新版
ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
ModbusTcpMaster *master = manager.getMaster(deviceId);
```

## 使用示例

```cpp
#include "scheduler/tasks/firmware_upgrade_task.h"

// 创建固件升级任务
QStringList deviceIds = {"device_001", "device_002", "device_003"};
QString binFilePath = "/path/to/firmware.bin";

FirmwareUpgradeTask *task = new FirmwareUpgradeTask(deviceIds, binFilePath);

// 连接信号
connect(task, &FirmwareUpgradeTask::deviceProgress, 
        this, &MyClass::onDeviceProgress);
connect(task, &FirmwareUpgradeTask::deviceStateLog, 
        this, &MyClass::onDeviceStateLog);
connect(task, &FirmwareUpgradeTask::deviceFinished, 
        this, &MyClass::onDeviceFinished);
connect(task, &FirmwareUpgradeTask::allProgress, 
        this, &MyClass::onAllProgress);
connect(task, &FirmwareUpgradeTask::finished, 
        this, &MyClass::onTaskFinished);

// 提交任务到调度器
Scheduler::instance().submitTask(task);
```

## 兼容性说明

### 完全兼容
- BinFileReader 的使用方式保持不变
- 分包规则 {248, 256} 保持不变
- 升级流程和状态机逻辑保持不变

### 需要适配
- 信号槽连接需要处理新增的 `masterId` 参数
- 设备标识从 `qrcode` 改为 `deviceId`
- Manager 实例获取方式从指针改为引用

## 文件位置

- **新版任务**: `scheduler/tasks/firmware_upgrade_task.{h,cpp}`
- **新版升级器**: `data/modbustcpmastermanager/modbustcpmaster/firmwareupgrader.{h,cpp}`
- **旧版代码**: `old/firmware_upgrade_task.{h,cpp}` (保留作为参考)
- **旧版升级器**: `old/mtm_firmware_upgrader.{h,cpp}` (保留作为参考)

## 构建配置

新任务已添加到 `scheduler/scheduler.pri`：
```qmake
HEADERS += \
    ...
    $$PWD/tasks/firmware_upgrade_task.h

SOURCES += \
    ...
    $$PWD/tasks/firmware_upgrade_task.cpp
```

## 注意事项

1. **线程安全**: FirmwareUpgrader 运行在 ModbusTcpMaster 所在的工作线程中，必须使用 `Qt::QueuedConnection` 调用其方法
2. **BinFileReader 共享**: 多设备升级时应共享同一个 BinFileReader 实例，避免重复读取文件
3. **信号断开**: 设备升级完成后会自动断开信号连接，避免重复处理
4. **停止机制**: 调用 `stop()` 会向所有正在升级的设备发送停止请求

## 测试建议

1. 单设备升级测试
2. 多设备并发升级测试
3. 升级过程中断测试（调用 stop()）
4. 文件读取失败测试
5. 设备不存在测试
6. 网络断开测试
