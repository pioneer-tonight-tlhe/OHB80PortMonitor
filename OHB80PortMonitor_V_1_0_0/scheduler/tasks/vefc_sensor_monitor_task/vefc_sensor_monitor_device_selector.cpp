#include "vefc_sensor_monitor_device_selector.h"

#include "app/shareddata.h"
#include "classes/foupofohbinfo.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

// ====================================================================
// VEFCSensorMonitorDeviceSelector - 设备筛选实现
//
// 说明：
//   1. 这里统一封装“本轮是否可以给某设备下发 VEFC 监控指令”的前置判断。
//   2. Task 只关心筛选结果，不再直接到处读取 SharedData / MasterManager。
// ====================================================================
QStringList VEFCSensorMonitorDeviceSelector::targetQrcodes() const
{
    return SharedData::getAllQrcodes();
}

VEFCSensorMonitor::DeviceInspection VEFCSensorMonitorDeviceSelector::inspectDevice(const QString& qrCode) const
{
    VEFCSensorMonitor::DeviceInspection inspection;
    inspection.qrCode = qrCode;

    // 先检查 FOUP 信息是否存在；后续气压与流量也从这里读取。
    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(qrCode);
    if (!foup) {
        inspection.unavailableReason = QStringLiteral("FoupOfOHBInfo not found");
        return inspection;
    }

    inspection.foupAvailable = true;
    inspection.gasPressure = foup->inletPressure();
    inspection.actualFlow = foup->inletFlow();

    // 再检查 Master、连接状态和 Sender；保持失败原因稳定且易读。
    ModbusTcpMaster* master = ModbusTcpMasterManager::instance().getMaster(qrCode);
    if (!master) {
        inspection.unavailableReason = QStringLiteral("Master not found");
        return inspection;
    }

    inspection.masterAvailable = true;
    inspection.connected = master->isConnected();
    if (!inspection.connected) {
        inspection.unavailableReason = QStringLiteral("Master not connected");
        return inspection;
    }

    inspection.senderAvailable = (master->sender() != nullptr);
    if (!inspection.senderAvailable) {
        inspection.unavailableReason = QStringLiteral("Command sender is null");
        return inspection;
    }

    return inspection;
}

QList<VEFCSensorMonitor::DeviceInspection> VEFCSensorMonitorDeviceSelector::inspectTargets() const
{
    QList<VEFCSensorMonitor::DeviceInspection> inspections;
    const QStringList qrcodes = targetQrcodes();
    inspections.reserve(qrcodes.size());

    // 按 SharedData 的设备顺序依次生成检查结果，便于后续轮次汇总稳定展示。
    for (const QString& qrCode : qrcodes) {
        inspections.append(inspectDevice(qrCode));
    }
    return inspections;
}

QStringList VEFCSensorMonitorDeviceSelector::filterAvailableDevices() const
{
    QStringList available;
    const QList<VEFCSensorMonitor::DeviceInspection> inspections = inspectTargets();

    // 这里只提供纯查询结果，供 UI 或调试场景使用。
    for (const VEFCSensorMonitor::DeviceInspection& inspection : inspections) {
        if (inspection.canSubmitCommands()) {
            available.append(inspection.qrCode);
        }
    }
    return available;
}
