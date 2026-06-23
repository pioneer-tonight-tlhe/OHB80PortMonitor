#include "firmware_upgrade_test_report_repository.h"

#include "customlogger.h"
#include "csvio.h"

#include <QDir>
#include <QFile>

#include <algorithm>

namespace {
const QStringList kSessionSummaryHeaders = {
    QStringLiteral("会话ID"),
    QStringLiteral("开始时间"),
    QStringLiteral("结束时间"),
    QStringLiteral("状态"),
    QStringLiteral("目标轮次"),
    QStringLiteral("已完成轮次"),
    QStringLiteral("轮次间隔毫秒"),
    QStringLiteral("设备数量"),
    QStringLiteral("成功轮次"),
    QStringLiteral("失败轮次"),
    QStringLiteral("固件文件路径")
};

const QStringList kRoundSummaryHeaders = {
    QStringLiteral("会话ID"),
    QStringLiteral("轮次"),
    QStringLiteral("开始时间"),
    QStringLiteral("结束时间"),
    QStringLiteral("设备总数"),
    QStringLiteral("成功设备数"),
    QStringLiteral("失败设备数"),
    QStringLiteral("结果"),
    QStringLiteral("截图路径")
};

const QStringList kFailureDetailHeaders = {
    QStringLiteral("会话ID"),
    QStringLiteral("轮次"),
    QStringLiteral("二维码"),
    QStringLiteral("失败阶段"),
    QStringLiteral("失败码"),
    QStringLiteral("失败原因"),
    QStringLiteral("失败时间"),
    QStringLiteral("截图路径")
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
        {QStringLiteral("会话ID"), record.sessionId},
        {QStringLiteral("开始时间"), toCsvTime(record.startTime)},
        {QStringLiteral("结束时间"), toCsvTime(record.endTime)},
        {QStringLiteral("状态"), record.status},
        {QStringLiteral("目标轮次"), record.targetRounds},
        {QStringLiteral("已完成轮次"), record.completedRounds},
        {QStringLiteral("轮次间隔毫秒"), record.intervalMs},
        {QStringLiteral("设备数量"), record.deviceCount},
        {QStringLiteral("成功轮次"), record.successRounds},
        {QStringLiteral("失败轮次"), record.failedRounds},
        {QStringLiteral("固件文件路径"), record.binFilePath}
    };
}

QJsonObject toJsonObject(const FirmwareUpgradeTestRoundSummaryRecord &record)
{
    return QJsonObject{
        {QStringLiteral("会话ID"), record.sessionId},
        {QStringLiteral("轮次"), record.roundIndex},
        {QStringLiteral("开始时间"), toCsvTime(record.startTime)},
        {QStringLiteral("结束时间"), toCsvTime(record.endTime)},
        {QStringLiteral("设备总数"), record.totalDevices},
        {QStringLiteral("成功设备数"), record.successDevices},
        {QStringLiteral("失败设备数"), record.failedDevices},
        {QStringLiteral("结果"), record.result},
        {QStringLiteral("截图路径"), record.screenshotPath}
    };
}

QJsonObject toJsonObject(const FirmwareUpgradeTestFailureDetailRecord &record)
{
    return QJsonObject{
        {QStringLiteral("会话ID"), record.sessionId},
        {QStringLiteral("轮次"), record.roundIndex},
        {QStringLiteral("二维码"), record.qrcode},
        {QStringLiteral("失败阶段"), record.phase},
        {QStringLiteral("失败码"), record.errorCode},
        {QStringLiteral("失败原因"), record.errorMessage},
        {QStringLiteral("失败时间"), toCsvTime(record.occurredTime)},
        {QStringLiteral("截图路径"), record.screenshotPath}
    };
}

FirmwareUpgradeTestSessionSummaryRecord sessionSummaryFromRow(const QStringList &headers,
                                                             const QStringList &row)
{
    FirmwareUpgradeTestSessionSummaryRecord record;
    record.sessionId = rowValue(headers, row, QStringLiteral("会话ID"));
    record.startTime = fromCsvTime(rowValue(headers, row, QStringLiteral("开始时间")));
    record.endTime = fromCsvTime(rowValue(headers, row, QStringLiteral("结束时间")));
    record.status = rowValue(headers, row, QStringLiteral("状态"));
    record.targetRounds = rowIntValue(headers, row, QStringLiteral("目标轮次"));
    record.completedRounds = rowIntValue(headers, row, QStringLiteral("已完成轮次"));
    record.intervalMs = rowIntValue(headers, row, QStringLiteral("轮次间隔毫秒"));
    record.deviceCount = rowIntValue(headers, row, QStringLiteral("设备数量"));
    record.successRounds = rowIntValue(headers, row, QStringLiteral("成功轮次"));
    record.failedRounds = rowIntValue(headers, row, QStringLiteral("失败轮次"));
    record.binFilePath = rowValue(headers, row, QStringLiteral("固件文件路径"));
    return record;
}

FirmwareUpgradeTestRoundSummaryRecord roundSummaryFromRow(const QStringList &headers,
                                                          const QStringList &row)
{
    FirmwareUpgradeTestRoundSummaryRecord record;
    record.sessionId = rowValue(headers, row, QStringLiteral("会话ID"));
    record.roundIndex = rowIntValue(headers, row, QStringLiteral("轮次"));
    record.startTime = fromCsvTime(rowValue(headers, row, QStringLiteral("开始时间")));
    record.endTime = fromCsvTime(rowValue(headers, row, QStringLiteral("结束时间")));
    record.totalDevices = rowIntValue(headers, row, QStringLiteral("设备总数"));
    record.successDevices = rowIntValue(headers, row, QStringLiteral("成功设备数"));
    record.failedDevices = rowIntValue(headers, row, QStringLiteral("失败设备数"));
    record.result = rowValue(headers, row, QStringLiteral("结果"));
    record.screenshotPath = rowValue(headers, row, QStringLiteral("截图路径"));
    return record;
}

FirmwareUpgradeTestFailureDetailRecord failureDetailFromRow(const QStringList &headers,
                                                            const QStringList &row)
{
    FirmwareUpgradeTestFailureDetailRecord record;
    record.sessionId = rowValue(headers, row, QStringLiteral("会话ID"));
    record.roundIndex = rowIntValue(headers, row, QStringLiteral("轮次"));
    record.qrcode = rowValue(headers, row, QStringLiteral("二维码"));
    record.phase = rowValue(headers, row, QStringLiteral("失败阶段"));
    record.errorCode = rowValue(headers, row, QStringLiteral("失败码"));
    record.errorMessage = rowValue(headers, row, QStringLiteral("失败原因"));
    record.occurredTime = fromCsvTime(rowValue(headers, row, QStringLiteral("失败时间")));
    record.screenshotPath = rowValue(headers, row, QStringLiteral("截图路径"));
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
