#ifndef VEFC_SENSOR_MONITOR_DEVICE_SELECTOR_H
#define VEFC_SENSOR_MONITOR_DEVICE_SELECTOR_H

#include "vefc_sensor_monitor_types.h"

#include <QList>
#include <QStringList>

// ====================================================================
// VEFCSensorMonitorDeviceSelector - VEFC 监控设备筛选器
//
// 设计目标：
//   1. 统一决定本轮需要检查的目标设备列表，并保持 SharedData 中的设备顺序。
//   2. 统一检查 Foup / Master / 连接状态 / Sender 是否满足提交业务指令的前置条件。
//   3. 只返回筛选结果，不修改轮次上下文，也不直接提交业务指令。
// ====================================================================
class VEFCSensorMonitorDeviceSelector
{
public:
    // 返回当前 VEFC 监控目标二维码列表；全量模式下保持 SharedData 顺序。
    QStringList targetQrcodes() const;

    // 检查单设备可执行条件，并返回完整快照与首个不可执行原因。
    VEFCSensorMonitor::DeviceInspection inspectDevice(const QString& qrCode) const;

    // 按目标顺序检查全部设备，供 Task 逐个处理。
    QList<VEFCSensorMonitor::DeviceInspection> inspectTargets() const;

    // 供 UI 查询当前可执行设备；只做只读筛选。
    QStringList filterAvailableDevices() const;
};

#endif // VEFC_SENSOR_MONITOR_DEVICE_SELECTOR_H
