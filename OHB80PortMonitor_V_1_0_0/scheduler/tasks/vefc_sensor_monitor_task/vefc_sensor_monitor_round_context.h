#ifndef VEFC_SENSOR_MONITOR_ROUND_CONTEXT_H
#define VEFC_SENSOR_MONITOR_ROUND_CONTEXT_H

#include "vefc_sensor_monitor_types.h"

#include <QHash>
#include <QString>
#include <QStringList>

// ====================================================================
// VEFCSensorMonitorRoundContext - VEFC 监控轮次上下文
//
// 设计目标：
//   1. 统一维护一轮 VEFC 监控的 roundId、记录时间戳、目标顺序和设备状态集合。
//   2. 对外提供语义化读写接口，避免 Task 直接操作原始 QHash。
//   3. 保持设备顺序稳定，便于日志和 UI 按固定顺序展示本轮结果。
// ====================================================================
class VEFCSensorMonitorRoundContext
{
public:
    // 开始新一轮，按目标顺序为每台设备创建默认状态。
    void beginRound(const QString& roundId,
                    qint64 recordTimestamp,
                    const QString& startTime,
                    const QStringList& orderedQrcodes);

    // 完成当前轮次，但保留结果，直到下一次 beginRound() 覆盖。
    void completeRound();

    // 完全清空轮次上下文，供 stop() 或重新初始化时使用。
    void clear();

    bool isActive() const { return m_active; }
    const QString& roundId() const { return m_roundId; }
    qint64 recordTimestamp() const { return m_recordTimestamp; }
    const QString& startTime() const { return m_startTime; }
    const QStringList& orderedQrcodes() const { return m_orderedQrcodes; }
    int totalCount() const { return m_orderedQrcodes.size(); }

    // 查询或修改单设备状态；Task 通过这些接口维护设备本轮结果。
    bool containsState(const QString& qrCode) const;
    VEFCSensorMonitor::DeviceRoundState state(const QString& qrCode) const;
    VEFCSensorMonitor::DeviceRoundState* mutableState(const QString& qrCode);
    void upsertState(const VEFCSensorMonitor::DeviceRoundState& state);

    // 按设备目标顺序导出状态或轮次汇总，供日志与信号使用。
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
