#ifndef VEFC_SENSOR_MONITOR_TYPES_H
#define VEFC_SENSOR_MONITOR_TYPES_H

#include "classes/vefcsensormonitorrecord.h"

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace VEFCSensorMonitor {

enum class SensorCommandType {
    Pressure,
    Temperature
};

struct PendingCommand {
    QString qrCode;
    SensorCommandType type = SensorCommandType::Pressure;
};

struct DeviceInspection {
    QString qrCode;
    bool foupAvailable = false;
    bool masterAvailable = false;
    bool connected = false;
    bool senderAvailable = false;
    double gasPressure = 0.0;
    double actualFlow = 0.0;
    QString unavailableReason;

    bool canSubmitCommands() const
    {
        return foupAvailable && masterAvailable && connected && senderAvailable;
    }
};

struct DeviceRoundState {
    QString qrCode;
    VEFCSensorMonitorRecord record;
    bool pressureFinished = false;
    bool temperatureFinished = false;
    bool pressureOk = false;
    bool temperatureOk = false;
    bool persisted = false;
    bool skipped = false;
    QString failReason;

    bool isFinished() const
    {
        return pressureFinished && temperatureFinished;
    }

    bool isSuccessful() const
    {
        return pressureOk && temperatureOk && persisted;
    }
};

struct RoundSummary {
    QString roundId;
    QString startTime;
    QString endTime;
    int totalCount = 0;
    int persistedCount = 0;
    int failedCount = 0;
    int skippedCount = 0;
    QList<DeviceRoundState> details;
    QStringList failedDevices;
    QStringList skippedDevices;
};

} // namespace VEFCSensorMonitor

Q_DECLARE_METATYPE(VEFCSensorMonitor::SensorCommandType)
Q_DECLARE_METATYPE(VEFCSensorMonitor::DeviceRoundState)
Q_DECLARE_METATYPE(QList<VEFCSensorMonitor::DeviceRoundState>)
Q_DECLARE_METATYPE(VEFCSensorMonitor::RoundSummary)

#endif // VEFC_SENSOR_MONITOR_TYPES_H
