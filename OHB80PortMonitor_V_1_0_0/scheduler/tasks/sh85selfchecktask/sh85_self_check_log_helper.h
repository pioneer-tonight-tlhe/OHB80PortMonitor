#ifndef SH85_SELF_CHECK_LOG_HELPER_H
#define SH85_SELF_CHECK_LOG_HELPER_H

#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "tasks/record/sh85selfchecktaskrecord.h"
#include "ilogger.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// ====================================================================
// SH85SelfCheckLogHelper —— 手动/周期 SH85 自检任务共用日志工具
//
// 约束：
//   1. 倒计时 tick 不写日志。
//   2. 单设备过程先写入 SH85SelfCheckTaskRecord，设备结束时只写一条日志。
//   3. 周期任务在一轮结束时额外写一条汇总和分隔日志。
// ====================================================================
class SH85SelfCheckLogHelper
{
public:
    enum class Mode {
        Manual,
        Periodic
    };

    struct RoundSummary {
        QString roundId;
        QString startTime;
        QString endTime;
        int totalDevices = 0;
        int participatedCount = 0;
        int successCount = 0;
        int failureCount = 0;
        int skippedCount = 0;
        QStringList failedDevices;
        QStringList skippedDevices;
    };

    static QString createRoundId();
    static QString modeToText(Mode mode);
    static QString stateToChineseText(SH85SelfChecker::State state);
    static QString resultToCode(SH85SelfChecker::Result result);
    static QString resultToChineseText(SH85SelfChecker::Result result);
    static QString resultText(bool success, SH85SelfChecker::Result result);
    static QString descriptionToChinese(SH85SelfChecker::Result result,
                                        const QString& description);

    static void initRecord(SH85SelfCheckTaskRecord& record,
                           Mode mode,
                           const QString& qrCode,
                           const QString& roundId = QString());
    static void appendStart(SH85SelfCheckTaskRecord& record);
    static void appendSkip(SH85SelfCheckTaskRecord& record, const QString& reason);
    static void appendStage(SH85SelfCheckTaskRecord& record,
                            SH85SelfChecker::State state);
    static void appendCommand(SH85SelfCheckTaskRecord& record,
                              SH85SelfChecker::State state,
                              const ModbusCommand& cmd,
                              bool retrying = false);
    static void appendCommandNotSubmitted(SH85SelfCheckTaskRecord& record,
                                          SH85SelfChecker::State state,
                                          const ModbusCommand& cmd,
                                          const QString& reason);
    static void finishRecord(SH85SelfCheckTaskRecord& record,
                             bool success,
                             SH85SelfChecker::Result result,
                             const QString& description);
    static void finishSkippedRecord(SH85SelfCheckTaskRecord& record,
                                    const QString& reason);

    static void writeRecord(const SH85SelfCheckTaskRecord& record,
                            bool warn);
    static void writeRoundReport(const RoundSummary& summary,
                                 const QList<SH85SelfCheckTaskRecord>& records);
    static void writeRoundWarning(const RoundSummary& summary,
                                  const QList<SH85SelfCheckTaskRecord>& records);
    static void writeRoundSummary(const RoundSummary& summary);
    static void writeTaskSeparator(Mode mode,
                                   const QString& roundId,
                                   const QString& qrCode,
                                   const QString& resultText,
                                   const QString& startTime,
                                   const QString& endTime,
                                   int totalDevices = 0,
                                   int participatedCount = 0,
                                   int successCount = 0,
                                   int failureCount = 0,
                                   int skippedCount = 0);

    static QString commandDescription(const ModbusCommand& cmd);

private:
    static ILogger& logger();
    static QString normalizedDescription(SH85SelfChecker::Result result,
                                         const QString& description);
    static QString bytesToHexWithCrc(const QByteArray& bytes, const QByteArray& crc);
    static QString responseFrameString(const ModbusCommand& cmd);
    static QString parsedValueText(const ModbusCommand& cmd);
};

#endif // SH85_SELF_CHECK_LOG_HELPER_H
