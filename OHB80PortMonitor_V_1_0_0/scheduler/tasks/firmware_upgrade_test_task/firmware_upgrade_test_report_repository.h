#ifndef FIRMWARE_UPGRADE_TEST_REPORT_REPOSITORY_H
#define FIRMWARE_UPGRADE_TEST_REPORT_REPOSITORY_H

#include "tasks/firmware_upgrade_test_task/firmware_upgrade_test_report_types.h"

#include <QStringList>

class FirmwareUpgradeTestReportRepository
{
public:
    static QString createSessionId();

    static QString reportsRootPath();
    static QString sessionDirectoryPath(const QString &sessionId);
    static QString captureDirectoryPath(const QString &sessionId);

    static bool initializeSession(const QString &sessionId);
    static bool sessionExists(const QString &sessionId);

    static bool writeSessionSummary(const FirmwareUpgradeTestSessionSummaryRecord &record);
    static bool appendRoundSummary(const FirmwareUpgradeTestRoundSummaryRecord &record);
    static bool appendFailureDetail(const FirmwareUpgradeTestFailureDetailRecord &record);

    static bool loadSessionSummary(const QString &sessionId,
                                   FirmwareUpgradeTestSessionSummaryRecord *record);
    static QVector<FirmwareUpgradeTestRoundSummaryRecord> loadRoundSummaries(const QString &sessionId);
    static QVector<FirmwareUpgradeTestFailureDetailRecord> loadFailureDetails(const QString &sessionId);
    static QVector<FirmwareUpgradeTestFailureDetailRecord> loadFailureDetails(const QString &sessionId,
                                                                              int roundIndex);
    static FirmwareUpgradeTestReportData loadReport(const QString &sessionId);

    static QStringList listSessionIds();

private:
    static QString sessionSummaryCsvPath(const QString &sessionId);
    static QString roundSummaryCsvPath(const QString &sessionId);
    static QString failureDetailCsvPath(const QString &sessionId);
};

#endif // FIRMWARE_UPGRADE_TEST_REPORT_REPOSITORY_H
