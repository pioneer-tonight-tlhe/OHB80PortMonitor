#include "vefc_sensor_monitor_device_selector.h"

#include "app/shareddata.h"
#include "classes/foupofohbinfo.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

QStringList VEFCSensorMonitorDeviceSelector::targetQrcodes() const
{
    return SharedData::getAllQrcodes();
}

VEFCSensorMonitor::DeviceInspection VEFCSensorMonitorDeviceSelector::inspectDevice(const QString& qrCode) const
{
    VEFCSensorMonitor::DeviceInspection inspection;
    inspection.qrCode = qrCode;

    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(qrCode);
    if (!foup) {
        inspection.unavailableReason = QStringLiteral("FoupOfOHBInfo not found");
        return inspection;
    }

    inspection.foupAvailable = true;
    inspection.gasPressure = foup->inletPressure();
    inspection.actualFlow = foup->inletFlow();

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
    for (const QString& qrCode : qrcodes) {
        inspections.append(inspectDevice(qrCode));
    }
    return inspections;
}

QStringList VEFCSensorMonitorDeviceSelector::filterAvailableDevices() const
{
    QStringList available;
    const QList<VEFCSensorMonitor::DeviceInspection> inspections = inspectTargets();
    for (const VEFCSensorMonitor::DeviceInspection& inspection : inspections) {
        if (inspection.canSubmitCommands()) {
            available.append(inspection.qrCode);
        }
    }
    return available;
}
