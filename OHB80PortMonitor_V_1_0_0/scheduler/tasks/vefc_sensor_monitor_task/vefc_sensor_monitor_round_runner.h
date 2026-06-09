#ifndef VEFC_SENSOR_MONITOR_ROUND_RUNNER_H
#define VEFC_SENSOR_MONITOR_ROUND_RUNNER_H

#include "vefc_sensor_monitor_types.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>

class VEFCSensorMonitorRoundRunner : public QObject
{
    Q_OBJECT

public:
    explicit VEFCSensorMonitorRoundRunner(QObject* parent = nullptr);
    ~VEFCSensorMonitorRoundRunner() override;

    void connectAllSenders();
    void disconnectAllSenders();

    bool submitCommand(const QString& qrCode,
                       VEFCSensorMonitor::SensorCommandType type,
                       const char* commandId);

    void clearPendingCommands();
    bool hasPendingCommands() const { return !m_pendingCommands.isEmpty(); }
    int pendingCount() const { return m_pendingCommands.size(); }
    bool containsPendingCommand(qint64 uuid) const;
    VEFCSensorMonitor::PendingCommand pendingCommand(qint64 uuid) const;
    VEFCSensorMonitor::PendingCommand takePendingCommand(qint64 uuid);

signals:
    void commandFinished(ModbusCommand cmd, const QString& masterId);
    void commandRetrying(ModbusCommand cmd, const QString& masterId);

private:
    QList<QMetaObject::Connection> m_connections;
    QHash<qint64, VEFCSensorMonitor::PendingCommand> m_pendingCommands;
};

#endif // VEFC_SENSOR_MONITOR_ROUND_RUNNER_H
