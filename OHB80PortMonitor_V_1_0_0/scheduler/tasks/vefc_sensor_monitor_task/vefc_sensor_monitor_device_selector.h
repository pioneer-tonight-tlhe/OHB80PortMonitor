#ifndef VEFC_SENSOR_MONITOR_DEVICE_SELECTOR_H
#define VEFC_SENSOR_MONITOR_DEVICE_SELECTOR_H

#include "vefc_sensor_monitor_types.h"

#include <QList>
#include <QStringList>

class VEFCSensorMonitorDeviceSelector
{
public:
    QStringList targetQrcodes() const;
    VEFCSensorMonitor::DeviceInspection inspectDevice(const QString& qrCode) const;
    QList<VEFCSensorMonitor::DeviceInspection> inspectTargets() const;
    QStringList filterAvailableDevices() const;
};

#endif // VEFC_SENSOR_MONITOR_DEVICE_SELECTOR_H
