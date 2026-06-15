#include "firmware_upgrade_test_report_repository.h"

#include "customlogger.h"
#include "csvio.h"

#include <QDir>
#include <QFile>

#include <algorithm>

namespace {
const QStringList kSessionSummaryHeaders = {
    QStringLiteral("session_id"),
    QStringLiteral("start_time"),
    QStringLiteral("end_time"),
    QStringLiteral("status"),
    QStringLiteral("target_rounds"),
    QStringLiteral("completed_rounds"),
    QStringLiteral("interval_ms"),
    QStringLiteral("device_count"),
    QStringLiteral("success_rounds"),
    QStringLiteral("failed_rounds"),
    QStringLiteral("bin_file_path")
};

const QStringList kRoundSummaryHeaders = {
    QStringLiteral("session_id"),
    QStringLiteral("round_index"),
    QStringLiteral("start_time"),
    QStringLiteral("end_time"),
    QStringLiteral("total_devices"),
    QStringLiteral("success_devices"),
    QStringLiteral("failed_devices"),
    QStringLiteral("result"),
    QStringLiteral("screenshot_path")
};

const QStringList kFailureDetailHeaders = {
    QStringLiteral("session_id"),
    QStringLiteral("round_index"),
    QStringLiteral("qrcode"),
    QStringLiteral("phase"),
    QStringLiteral("error_code"),
    QStringLiteral("error_message"),
    QStringLiteral("occurred_time"),
    QStringLiteral("screenshot_path")
};

QString toCsvTime(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toString(Qt::ISODateWithMs) : QString();
}

QDateTime fromCsvTime(const QString &value)
{
    if (value.isEmpty()) {
        return QDateTime();
    }

    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    }
    return dateTime;
}

QString rowValue(const QStringList &headers, const QStringList &row, const QString &key)
{
    const int index = headers.indexOf(key);
    if (index < 0 || index >= row.size()) {
        return QString();
    }
    return row.at(index);
}

int rowIntValue(const QStringList &headers, const QStringList &row, const QString &key)
{
    bool ok = false;
    const int value = rowValue(headers, row, key).toInt(&ok);
    return ok ? value : 0;
}

QJsonObject toJsonObject(const FirmwareUpgradeTestSessionSummaryRecord &record)
{
    return QJsonObject{
        {QStringLiteral("session_id"), record.sessionId},
        {QStringLiteral("start_time"), toCsvTime(record.startTime)},
        {QStringLiteral("end_time"), toCsvTime(record.endTime)},
        {QStringLiteral("status"), record.status},
        {QStringLiteral("target_rounds"), record.targetRounds},
        {QStringLiteral("completed_rounds"), record.completedRounds},
        {QStringLiteral("interval_ms"), record.intervalMs},
        {QStringLiteral("device_count"), record.deviceCount},
        {QStringLiteral("success_rounds"), record.successRounds},
        {QStringLiteral("failed_rounds"), record.failedRounds},
        {QStringLiteral("bin_file_path"), record.binFilePath}
    };
}

QJsonObject toJsonObject(const FirmwareUpgradeTestRoundSummaryRecord &record)
{
    return QJsonObject{
        {QStringLiteral("session_id"), record.sessionId},
        {QStringLiteral("round_index"), record.roundIndex},
        {QStringLiteral("start_time"), toCsvTime(record.startTime)},
        {QStringLiteral("end_time"), toCsvTime(record.endTime)},
        {QStringLiteral("total_devices"), record.totalDevices},
        {QStringLiteral("success_devices"), record.successDevices},
        {QStringLiteral("failed_devices"), record.failedDevices},
        {QStringLiteral("result"), record.result},
        {QStringLiteral("screenshot_path"), record.screenshotPath}
    };
}

QJsonObject toJsonObject(const FirmwareUpgradeTestFailureDetailRecord &record)
{
    return QJsonObject{
        {QStringLiteral("session_id"), record.sessionId},
        {QStringLiteral("round_index"), record.roundIndex},
        {QStringLiteral("qrcode"), record.qrcode},
        {QStringLiteral("phase"), record.phase},
        {QStringLiteral("error_code"), record.errorCode},
        {QStringLiteral("error_message"), record.errorMessage},
        {QStringLiteral("occurred_time"), toCsvTime(record.occurredTime)},
        {QStringLiteral("screenshot_path"), record.screenshotPath}
    };
}

FirmwareUpgradeTestSessionSummaryRecord sessionSummaryFromRow(const QStringList &headers,
                                                             const QStringList &row)
{
    FirmwareUpgradeTestSessionSummaryRecord record;
    record.sessionId = rowValue(headers, row, QStringLiteral("session_id"));
    record.startTime = fromCsvTime(rowValue(headers, row, QStringLiteral("start_time")));
    record.endTime = fromCsvTime(rowValue(headers, row, QStringLiteral("end_time")));
    record.status = rowValue(headers, row, QStringLiteral("status"));
    record.targetRounds = rowIntValue(headers, row, QStringLiteral("target_rounds"));
    record.completedRounds = rowIntValue(headers, row, QStringLiteral("completed_rounds"));
    record.intervalMs = rowIntValue(headers, row, QStringLiteral("interval_ms"));
    record.deviceCount = rowIntValue(headers, row, QStringLiteral("device_count"));
    record.successRounds = rowIntValue(headers, row, QStringLiteral("success_rounds"));
    record.failedRounds = rowIntValue(headers, row, QStringLiteral("failed_rounds"));
    record.binFilePath = rowValue(headers, row, QStringLiteral("bin_file_path"));
    return record;
}

FirmwareUpgradeTestRoundSummaryRecord roundSummaryFromRow(const QStringList &headers,
                                                          const QStringList &row)
{
    FirmwareUpgradeTestRoundSummaryRecord record;
    record.sessionId = rowValue(headers, row, QStringLiteral("session_id"));
    record.roundIndex = rowIntValue(headers, row, QStringLiteral("round_index"));
    record.startTime = fromCsvTime(rowValue(headers, row, QStringLiteral("start_time")));
    record.endTime = fromCsvTime(rowValue(headers, row, QStringLiteral("end_time")));
    record.totalDevices = rowIntValue(headers, row, QStringLiteral("total_devices"));
    record.successDevices = rowIntValue(headers, row, QStringLiteral("success_devices"));
    record.failedDevices = rowIntValue(headers, row, QStringLiteral("failed_devices"));
    record.result = rowValue(headers, row, QStringLiteral("result"));
    record.screenshotPath = rowValue(headers, row, QStringLiteral("screenshot_path"));
    return record;
}

FirmwareUpgradeTestFailureDetailRecord failureDetailFromRow(const QStringList &headers,
                                                            const QStringList &row)
{
    FirmwareUpgradeTestFailureDetailRecord record;
    record.sessionId = rowValue(headers, row, QStringLiteral("session_id"));
    record.roundIndex = rowIntValue(headers, row, QStringLiteral("round_index"));
    record.qrcode = rowValue(headers, row, QStringLiteral("qrcode"));
    record.phase = rowValue(headers, row, QStringLiteral("phase"));
    record.errorCode = rowValue(headers, row, QStringLiteral("error_code"));
    record.errorMessage = rowValue(headers, row, QStringLiteral("error_message"));
    record.occurredTime = fromCsvTime(rowValue(headers, row, QStringLiteral("occurred_time")));
    record.screenshotPath = rowValue(headers, row, QStringLiteral("screenshot_path"));
    return record;
}

bool ensureDirectory(const QString &dirPath)
{
    QDir dir(dirPath);
    return dir.exists() || dir.mkpath(QStringLiteral("."));
}

bool ensureHeaderFile(const QString &filePath, const QStringList &headers)
{
    if (!QFile::exists(filePath)) {
        return CsvIO::writeHeader(filePath, headers);
    }

    const QStringList existingHeaders = CsvIO::readHeader(filePath);
    if (existingHeaders.isEmpty()) {
        return CsvIO::writeHeader(filePath, headers);
    }

    return existingHeaders == headers;
}
} // namespace

QString FirmwareUpgradeTestReportRepository::createSessionId()
{
    const QString baseSessionId =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
    QString sessionId = baseSessionId;
    int suffix = 1;

    while (sessionExists(sessionId)) {
        sessionId = QStringLiteral("%1_%2").arg(baseSessionId).arg(suffix++);
    }

    return sessionId;
}

QString FirmwareUpgradeTestReportRepository::reportsRootPath()
{
    return QDir(CustomLogger::FirmwareUpgradeLoggerPath()).filePath(QStringLiteral("test_reports"));
}

QString FirmwareUpgradeTestReportRepository::sessionDirectoryPath(const QString &sessionId)
{
    return QDir(reportsRootPath()).filePath(sessionId);
}

QString FirmwareUpgradeTestReportRepository::captureDirectoryPath(const QString &sessionId)
{
    return QDir(sessionDirectoryPath(sessionId)).filePath(QStringLiteral("captures"));
}

bool FirmwareUpgradeTestReportRepository::initializeSession(const QString &sessionId)
{
    if (sessionId.trimmed().isEmpty()) {
        return false;
    }

    const QString sessionDir = sessionDirectoryPath(sessionId);
    if (!ensureDirectory(reportsRootPath())
        || !ensureDirectory(sessionDir)
        || !ensureDirectory(captureDirectoryPath(sessionId))) {
        return false;
    }

    return ensureHeaderFile(sessionSummaryCsvPath(sessionId), kSessionSummaryHeaders)
        && ensureHeaderFile(roundSummaryCsvPath(sessionId), kRoundSummaryHeaders)
        && ensureHeaderFile(failureDetailCsvPath(sessionId), kFailureDetailHeaders);
}

bool FirmwareUpgradeTestReportRepository::sessionExists(const QString &sessionId)
{
    return QDir(sessionDirectoryPath(sessionId)).exists();
}

bool FirmwareUpgradeTestReportRepository::writeSessionSummary(
    const FirmwareUpgradeTestSessionSummaryRecord &record)
{
    if (record.sessionId.trimmed().isEmpty() || !initializeSession(record.sessionId)) {
        return false;
    }

    const QString filePath = sessionSummaryCsvPath(record.sessionId);
    return CsvIO::writeHeader(filePath, kSessionSummaryHeaders)
        && CsvIO::appendRecord(filePath, kSessionSummaryHeaders, toJsonObject(record));
}

bool FirmwareUpgradeTestReportRepository::appendRoundSummary(
    const FirmwareUpgradeTestRoundSummaryRecord &record)
{
    if (record.sessionId.trimmed().isEmpty() || !initializeSession(record.sessionId)) {
        return false;
    }

    return CsvIO::appendRecord(roundSummaryCsvPath(record.sessionId),
                               kRoundSummaryHeaders,
                               toJsonObject(record));
}

bool FirmwareUpgradeTestReportRepository::appendFailureDetail(
    const FirmwareUpgradeTestFailureDetailRecord &record)
{
    if (record.sessionId.trimmed().isEmpty() || !initializeSession(record.sessionId)) {
        return false;
    }

    return CsvIO::appendRecord(failureDetailCsvPath(record.sessionId),
                               kFailureDetailHeaders,
                               toJsonObject(record));
}

bool FirmwareUpgradeTestReportRepository::loadSessionSummary(
    const QString &sessionId,
    FirmwareUpgradeTestSessionSummaryRecord *record)
{
    if (!record) {
        return false;
    }

    const QString filePath = sessionSummaryCsvPath(sessionId);
    const QStringList headers = CsvIO::readHeader(filePath);
    const QVector<QStringList> rows = CsvIO::readAllRecords(filePath);
    if (headers.isEmpty() || rows.isEmpty()) {
        return false;
    }

    *record = sessionSummaryFromRow(headers, rows.first());
    return !record->sessionId.isEmpty();
}

QVector<FirmwareUpgradeTestRoundSummaryRecord>
FirmwareUpgradeTestReportRepository::loadRoundSummaries(const QString &sessionId)
{
    QVector<FirmwareUpgradeTestRoundSummaryRecord> records;

    const QString filePath = roundSummaryCsvPath(sessionId);
    const QStringList headers = CsvIO::readHeader(filePath);
    const QVector<QStringList> rows = CsvIO::readAllRecords(filePath);
    records.reserve(rows.size());

    for (const QStringList &row : rows) {
        records.append(roundSummaryFromRow(headers, row));
    }

    std::sort(records.begin(), records.end(),
              [](const FirmwareUpgradeTestRoundSummaryRecord &lhs,
                 const FirmwareUpgradeTestRoundSummaryRecord &rhs) {
        return lhs.roundIndex < rhs.roundIndex;
    });

    return records;
}

QVector<FirmwareUpgradeTestFailureDetailRecord>
FirmwareUpgradeTestReportRepository::loadFailureDetails(const QString &sessionId)
{
    QVector<FirmwareUpgradeTestFailureDetailRecord> records;

    const QString filePath = failureDetailCsvPath(sessionId);
    const QStringList headers = CsvIO::readHeader(filePath);
    const QVector<QStringList> rows = CsvIO::readAllRecords(filePath);
    records.reserve(rows.size());

    for (const QStringList &row : rows) {
        records.append(failureDetailFromRow(headers, row));
    }

    std::sort(records.begin(), records.end(),
              [](const FirmwareUpgradeTestFailureDetailRecord &lhs,
                 const FirmwareUpgradeTestFailureDetailRecord &rhs) {
        if (lhs.roundIndex != rhs.roundIndex) {
            return lhs.roundIndex < rhs.roundIndex;
        }
        if (lhs.qrcode != rhs.qrcode) {
            return lhs.qrcode < rhs.qrcode;
        }
        return lhs.occurredTime < rhs.occurredTime;
    });

    return records;
}

QVector<FirmwareUpgradeTestFailureDetailRecord>
FirmwareUpgradeTestReportRepository::loadFailureDetails(const QString &sessionId, int roundIndex)
{
    QVector<FirmwareUpgradeTestFailureDetailRecord> allRecords = loadFailureDetails(sessionId);
    QVector<FirmwareUpgradeTestFailureDetailRecord> filteredRecords;
    filteredRecords.reserve(allRecords.size());

    for (const FirmwareUpgradeTestFailureDetailRecord &record : allRecords) {
        if (record.roundIndex == roundIndex) {
            filteredRecords.append(record);
        }
    }

    return filteredRecords;
}

FirmwareUpgradeTestReportData FirmwareUpgradeTestReportRepository::loadReport(const QString &sessionId)
{
    FirmwareUpgradeTestReportData report;
    report.hasSessionSummary = loadSessionSummary(sessionId, &report.sessionSummary);
    report.roundSummaries = loadRoundSummaries(sessionId);
    report.failureDetails = loadFailureDetails(sessionId);
    return report;
}

QStringList FirmwareUpgradeTestReportRepository::listSessionIds()
{
    QDir dir(reportsRootPath());
    QStringList sessionIds = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    std::sort(sessionIds.begin(), sessionIds.end(), std::greater<QString>());
    return sessionIds;
}

QString FirmwareUpgradeTestReportRepository::sessionSummaryCsvPath(const QString &sessionId)
{
    return QDir(sessionDirectoryPath(sessionId)).filePath(QStringLiteral("session_summary.csv"));
}

QString FirmwareUpgradeTestReportRepository::roundSummaryCsvPath(const QString &sessionId)
{
    return QDir(sessionDirectoryPath(sessionId)).filePath(QStringLiteral("round_summary.csv"));
}

QString FirmwareUpgradeTestReportRepository::failureDetailCsvPath(const QString &sessionId)
{
    return QDir(sessionDirectoryPath(sessionId)).filePath(QStringLiteral("failure_detail.csv"));
}
