#ifndef SH85_SELF_CHECK_ROUND_CONTEXT_H
#define SH85_SELF_CHECK_ROUND_CONTEXT_H

#include "sh85_self_check_types.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>

// ====================================================================
// SH85SelfCheckRoundContext - SH85 自检轮次上下文
//
// 设计目标：
//   1. 统一维护单轮自检的 roundId、目标顺序、pending 集合和设备结果。
//   2. 对外提供语义化方法，避免 Task3 直接操作结果容器。
//   3. 保持汇总顺序稳定，方便 UI 和日志按设备顺序展示。
// ====================================================================
class SH85SelfCheckRoundContext
{
public:
    // 开始新一轮：重置上下文，并为每个目标设备创建默认“未参与”结果。
    void beginRound(const QString& roundId,
                    const QString& startTime,
                    const QStringList& orderedQrcodes);

    // 完成本轮：关闭 active 状态，但保留结果数据，直到下一次 beginRound() 覆盖。
    void completeRound();

    // 完全清空：用于需要回到空上下文的场景。
    void clear();

    bool isActive() const { return m_active; }
    bool hasPendingDevices() const { return !m_pendingQrcodes.isEmpty(); }
    bool isPending(const QString& qrcode) const { return m_pendingQrcodes.contains(qrcode); }
    int pendingCount() const { return m_pendingQrcodes.size(); }
    QStringList pendingQrcodes() const { return m_pendingQrcodes.values(); }

    const QString& roundId() const { return m_roundId; }
    const QString& startTime() const { return m_startTime; }
    const QStringList& orderedQrcodes() const { return m_orderedQrcodes; }
    int totalCount() const { return m_orderedQrcodes.size(); }

    bool containsResult(const QString& qrcode) const;
    SH85SelfCheck::DeviceResult result(const QString& qrcode) const;

    // 标记跳过：用于未启用、FOUP 在位等未提交 checker 的设备。
    void markSkipped(const QString& qrcode, const QString& description);

    // 标记参与：设备已进入本轮统计，即使后续启动 checker 失败也算参与失败。
    void markParticipating(const QString& qrcode);

    // 标记等待：checker 准备启动后加入 pending，后续等待 finished 信号收口。
    void markPending(const QString& qrcode);

    // 结束设备：写入最终结果，并从 pending 中移除。
    bool finishDevice(const QString& qrcode, bool success, const QString& description);

    SH85SelfCheck::RoundSummary summary() const;

private:
    QString m_roundId;
    QString m_startTime;
    bool m_active = false;
    QStringList m_orderedQrcodes;
    QSet<QString> m_pendingQrcodes;
    QHash<QString, SH85SelfCheck::DeviceResult> m_results;
};

#endif // SH85_SELF_CHECK_ROUND_CONTEXT_H
