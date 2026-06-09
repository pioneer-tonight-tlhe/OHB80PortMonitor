#ifndef VEFC_SENSOR_MONITOR_ROUND_CONTEXT_H
#define VEFC_SENSOR_MONITOR_ROUND_CONTEXT_H

#include "vefc_sensor_monitor_types.h"

#include <QHash>
#include <QString>
#include <QStringList>

class VEFCSensorMonitorRoundContext
{
public:
    void beginRound(const QString& roundId,
                    qint64 recordTimestamp,
                    const QString& startTime,
                    const QStringList& orderedQrcodes);
    void completeRound();
    void clear();

    bool isActive() const { return m_active; }
    const QString& roundId() const { return m_roundId; }
    qint64 recordTimestamp() const { return m_recordTimestamp; }
    const QString& startTime() const { return m_startTime; }
    const QStringList& orderedQrcodes() const { return m_orderedQrcodes; }
    int totalCount() const { return m_orderedQrcodes.size(); }

    bool containsState(const QString& qrCode) const;
    VEFCSensorMonitor::DeviceRoundState state(const QString& qrCode) const;
    VEFCSensorMonitor::DeviceRoundState* mutableState(const QString& qrCode);
    void upsertState(const VEFCSensorMonitor::DeviceRoundState& state);
    QList<VEFCSensorMonitor::DeviceRoundState> orderedStates() const;
    VEFCSensorMonitor::RoundSummary buildSummary(const QString& endTime) const;

private:
    bool m_active = false;
    QString m_roundId;
    qint64 m_recordTimestamp = 0;
    QString m_startTime;
    QStringList m_orderedQrcodes;
    QHash<QString, VEFCSensorMonitor::DeviceRoundState> m_states;
};

#endif // VEFC_SENSOR_MONITOR_ROUND_CONTEXT_H
