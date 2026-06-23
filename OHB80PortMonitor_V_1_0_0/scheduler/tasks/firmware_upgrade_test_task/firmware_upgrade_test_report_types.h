#ifndef FIRMWARE_UPGRADE_TEST_REPORT_TYPES_H
#define FIRMWARE_UPGRADE_TEST_REPORT_TYPES_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

struct FirmwareUpgradeTestSessionSummaryRecord
{
    QString   sessionId;
    QDateTime startTime;
    QDateTime endTime;
    QString   status;
    int       targetRounds = 0;
    int       completedRounds = 0;
    int       intervalMs = 0;
    int       deviceCount = 0;
    int       successRounds = 0;
    int       failedRounds = 0;
    QString   binFilePath;
};

struct FirmwareUpgradeTestRoundSummaryRecord
{
    QString   sessionId;
    int       roundIndex = 0;
    QDateTime startTime;
    QDateTime endTime;
    int       totalDevices = 0;
    int       successDevices = 0;
    int       failedDevices = 0;
    QString   result;
    QString   screenshotPath;
};

struct FirmwareUpgradeTestFailureDetailRecord
{
    QString   sessionId;
    int       roundIndex = 0;
    QString   qrcode;
    QString   phase;
    QString   errorCode;
    QString   errorMessage;
    QDateTime occurredTime;
    QString   screenshotPath;
};

struct FirmwareUpgradeTestReportData
{
    bool hasSessionSummary = false;
    FirmwareUpgradeTestSessionSummaryRecord sessionSummary;
    QVector<FirmwareUpgradeTestRoundSummaryRecord> roundSummaries;
    QVector<FirmwareUpgradeTestFailureDetailRecord> failureDetails;
};

Q_DECLARE_METATYPE(FirmwareUpgradeTestSessionSummaryRecord)
Q_DECLARE_METATYPE(FirmwareUpgradeTestRoundSummaryRecord)
Q_DECLARE_METATYPE(FirmwareUpgradeTestFailureDetailRecord)
Q_DECLARE_METATYPE(FirmwareUpgradeTestReportData)

#endif // FIRMWARE_UPGRADE_TEST_REPORT_TYPES_H
