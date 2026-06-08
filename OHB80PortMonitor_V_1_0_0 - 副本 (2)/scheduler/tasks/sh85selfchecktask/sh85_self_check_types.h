#ifndef SH85_SELF_CHECK_TYPES_H
#define SH85_SELF_CHECK_TYPES_H

#include <QString>

namespace SH85SelfCheck {

// 单设备在一轮自检中的统计结果。
struct DeviceResult {
    QString qrcode;
    bool participated = false;
    bool success = false;
    QString description;
};

// 一轮自检结束后的轻量汇总；供 Task3 通知 UI 或后续写轮次日志使用。
struct RoundSummary {
    int totalCount = 0;
    int participatedCount = 0;
    int successCount = 0;
    int failureCount = 0;
    int skippedCount = 0;
    int notSubmittedCount = 0;
};

} // namespace SH85SelfCheck

#endif // SH85_SELF_CHECK_TYPES_H
