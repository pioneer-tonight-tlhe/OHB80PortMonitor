#include "sh85_self_check_device_selector.h"

#include "app/shareddata.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

QStringList SH85SelfCheckDeviceSelector::targetQrcodes(bool singleDeviceMode,
                                                       const QString& singleDeviceQrcode) const
{
    // 保持 Task2 行为：单设备模式下二维码为空时，不产生任何目标设备。
    if (singleDeviceMode) {
        return singleDeviceQrcode.isEmpty() ? QStringList() : QStringList{singleDeviceQrcode};
    }

    return SharedData::getAllQrcodes();
}

SH85SelfCheckDeviceSelector::DeviceInspection
SH85SelfCheckDeviceSelector::inspectDevice(const QString& qrcode) const
{
    DeviceInspection inspection;
    inspection.qrcode = qrcode;

    // 设备启用状态和 FOUP 在位状态来自 SharedData，是业务层前置条件。
    FoupOfOHBInfo* foupInfo = SharedData::getFoupByQRCode(qrcode);
    inspection.enabled = (foupInfo && foupInfo->enable());
    if (!inspection.enabled) {
        inspection.unavailableReason = QStringLiteral("Device disabled");
        return inspection;
    }

    inspection.foupIn = foupInfo->foupIn();
    if (inspection.foupIn) {
        inspection.unavailableReason = QStringLiteral("FOUP in place");
        return inspection;
    }

    // 这里仅检查连接状态；checker 对象是否为空由 Task3 在启动前处理。
    inspection.master = ModbusTcpMasterManager::instance().getMaster(qrcode);
    inspection.connected = (inspection.master && inspection.master->isConnected());
    if (!inspection.connected) {
        inspection.unavailableReason = QStringLiteral("Device not connected");
        return inspection;
    }

    return inspection;
}

QList<SH85SelfCheckDeviceSelector::DeviceInspection>
SH85SelfCheckDeviceSelector::inspectTargets(bool singleDeviceMode,
                                            const QString& singleDeviceQrcode) const
{
    QList<DeviceInspection> inspections;
    const QStringList qrcodes = targetQrcodes(singleDeviceMode, singleDeviceQrcode);
    inspections.reserve(qrcodes.size());

    for (const QString& qrcode : qrcodes) {
        inspections.append(inspectDevice(qrcode));
    }

    return inspections;
}

QStringList SH85SelfCheckDeviceSelector::filterAvailableDevices(bool singleDeviceMode,
                                                                const QString& singleDeviceQrcode) const
{
    QStringList availableDevices;
    const QList<DeviceInspection> inspections = inspectTargets(singleDeviceMode, singleDeviceQrcode);

    for (const DeviceInspection& inspection : inspections) {
        if (inspection.canStartChecker()) {
            availableDevices.append(inspection.qrcode);
        }
    }

    return availableDevices;
}
