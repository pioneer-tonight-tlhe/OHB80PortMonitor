#include "vefc_sensor_monitor_round_context.h"

// ====================================================================
// VEFCSensorMonitorRoundContext - 轮次上下文实现
//
// 说明：
//   1. Task 只负责驱动轮次流程，具体状态读写统一收口到这里。
//   2. 汇总时始终按目标设备顺序输出，避免 QHash 无序带来的展示抖动。
// ====================================================================
void VEFCSensorMonitorRoundContext::beginRound(const QString& roundId,
                                               qint64 recordTimestamp,
                                               const QString& startTime,
                                               const QStringList& orderedQrcodes)
{
    clear();

    // 进入新轮次后，先缓存元数据，再为每个目标设备创建默认状态。
    m_active = true;
    m_roundId = roundId;
    m_recordTimestamp = recordTimestamp;
    m_startTime = startTime;
    m_orderedQrcodes = orderedQrcodes;

    for (const QString& qrCode : m_orderedQrcodes) {
        VEFCSensorMonitor::DeviceRoundState state;
        state.qrCode = qrCode;
        state.record.qrCode = qrCode;
        state.record.recordTimestamp = m_recordTimestamp;
        m_states.insert(qrCode, state);
    }
}

void VEFCSensorMonitorRoundContext::completeRound()
{
    // completeRound() 只结束 active 状态，不主动清除本轮结果。
    m_active = false;
}

void VEFCSensorMonitorRoundContext::clear()
{
    m_active = false;
    m_roundId.clear();
    m_recordTimestamp = 0;
    m_startTime.clear();
    m_orderedQrcodes.clear();
    m_states.clear();
}

bool VEFCSensorMonitorRoundContext::containsState(const QString& qrCode) const
{
    return m_states.contains(qrCode);
}

VEFCSensorMonitor::DeviceRoundState VEFCSensorMonitorRoundContext::state(const QString& qrCode) const
{
    return m_states.value(qrCode);
}

VEFCSensorMonitor::DeviceRoundState* VEFCSensorMonitorRoundContext::mutableState(const QString& qrCode)
{
    auto it = m_states.find(qrCode);
    if (it == m_states.end()) {
        return nullptr;
    }
    return &it.value();
}

void VEFCSensorMonitorRoundContext::upsertState(const VEFCSensorMonitor::DeviceRoundState& state)
{
    m_states.insert(state.qrCode, state);
}

QList<VEFCSensorMonitor::DeviceRoundState> VEFCSensorMonitorRoundContext::orderedStates() const
{
    QList<VEFCSensorMonitor::DeviceRoundState> states;
    states.reserve(m_orderedQrcodes.size());

    // 按 beginRound() 记录的目标顺序导出，方便日志和 UI 稳定展示。
    for (const QString& qrCode : m_orderedQrcodes) {
        states.append(m_states.value(qrCode));
    }
    return states;
}

VEFCSensorMonitor::RoundSummary VEFCSensorMonitorRoundContext::buildSummary(const QString& endTime) const
{
    VEFCSensorMonitor::RoundSummary summary;
    summary.roundId = m_roundId;
    summary.startTime = m_startTime;
    summary.endTime = endTime;
    summary.totalCount = m_orderedQrcodes.size();
    summary.details = orderedStates();

    // 汇总统计统一基于 orderedStates() 结果，保证设备顺序与目标顺序一致。
    for (const VEFCSensorMonitor::DeviceRoundState& state : summary.details) {
        if (state.skipped) {
            ++summary.skippedCount;
            summary.skippedDevices.append(QStringLiteral("%1(%2)").arg(state.qrCode, state.failReason));
            continue;
        }

        if (state.persisted) {
            ++summary.persistedCount;
            continue;
        }

        ++summary.failedCount;
        summary.failedDevices.append(QStringLiteral("%1(%2)").arg(state.qrCode, state.failReason));
    }

    return summary;
}
