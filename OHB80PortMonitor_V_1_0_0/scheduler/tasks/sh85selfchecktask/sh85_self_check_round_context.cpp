#include "sh85_self_check_round_context.h"

void SH85SelfCheckRoundContext::beginRound(const QString& roundId,
                                           const QString& startTime,
                                           const QStringList& orderedQrcodes)
{
    m_roundId = roundId;
    m_startTime = startTime;
    m_active = true;
    m_orderedQrcodes = orderedQrcodes;
    m_pendingQrcodes.clear();
    m_results.clear();

    // 先为所有目标创建设备结果，确保跳过设备也能进入稳定顺序的汇总。
    for (const QString& qrcode : m_orderedQrcodes) {
        SH85SelfCheck::DeviceResult result;
        result.qrcode = qrcode;
        result.participated = false;
        result.success = false;
        result.description = QStringLiteral("Not participated");
        m_results.insert(qrcode, result);
    }
}

void SH85SelfCheckRoundContext::completeRound()
{
    // 完成后关闭 active，迟到的 checker 信号应被 Task3 忽略。
    m_active = false;
    m_pendingQrcodes.clear();
}

void SH85SelfCheckRoundContext::clear()
{
    m_roundId.clear();
    m_startTime.clear();
    m_active = false;
    m_orderedQrcodes.clear();
    m_pendingQrcodes.clear();
    m_results.clear();
}

bool SH85SelfCheckRoundContext::containsResult(const QString& qrcode) const
{
    return m_results.contains(qrcode);
}

SH85SelfCheck::DeviceResult SH85SelfCheckRoundContext::result(const QString& qrcode) const
{
    return m_results.value(qrcode);
}

void SH85SelfCheckRoundContext::markSkipped(const QString& qrcode, const QString& description)
{
    if (!m_results.contains(qrcode)) {
        return;
    }

    SH85SelfCheck::DeviceResult& result = m_results[qrcode];
    result.qrcode = qrcode;
    result.participated = false;
    result.success = false;
    result.description = description;
    m_pendingQrcodes.remove(qrcode);
}

void SH85SelfCheckRoundContext::markParticipating(const QString& qrcode)
{
    if (!m_results.contains(qrcode)) {
        return;
    }

    SH85SelfCheck::DeviceResult& result = m_results[qrcode];
    result.qrcode = qrcode;
    result.participated = true;
    result.success = false;
    result.description.clear();
}

void SH85SelfCheckRoundContext::markPending(const QString& qrcode)
{
    if (m_results.contains(qrcode)) {
        m_pendingQrcodes.insert(qrcode);
    }
}

bool SH85SelfCheckRoundContext::finishDevice(const QString& qrcode,
                                             bool success,
                                             const QString& description,
                                             double minimumHumidity)
{
    if (!m_results.contains(qrcode)) {
        return false;
    }

    SH85SelfCheck::DeviceResult& result = m_results[qrcode];
    result.qrcode = qrcode;
    result.participated = true;
    result.success = success;
    result.description = description;
    result.minimumHumidity = minimumHumidity;
    m_pendingQrcodes.remove(qrcode);
    return true;
}

SH85SelfCheck::RoundSummary SH85SelfCheckRoundContext::summary() const
{
    SH85SelfCheck::RoundSummary summary;
    summary.totalCount = m_orderedQrcodes.size();

    // 按目标列表顺序统计，不依赖 QHash 顺序，保证汇总行为稳定。
    for (const QString& qrcode : m_orderedQrcodes) {
        const SH85SelfCheck::DeviceResult result = m_results.value(qrcode);
        if (result.participated) {
            ++summary.participatedCount;
            if (result.success) {
                ++summary.successCount;
            } else {
                ++summary.failureCount;
            }
        } else {
            ++summary.skippedCount;
            ++summary.notSubmittedCount;
        }
    }

    return summary;
}
