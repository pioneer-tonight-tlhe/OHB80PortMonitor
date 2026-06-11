#include "vefc_sensor_monitor_daily_stats.h"

#include "config/loggerconfig.h"

#include <QHash>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace {

constexpr double kZeroPressureEpsilon = 0.000001;

bool qrcodeNumericLess(const QString& left, const QString& right)
{
    bool leftOk = false;
    bool rightOk = false;
    const int leftValue = left.toInt(&leftOk);
    const int rightValue = right.toInt(&rightOk);
    if (leftOk && rightOk && leftValue != rightValue) {
        return leftValue < rightValue;
    }
    return left < right;
}

double average(const QVector<double>& values)
{
    if (values.isEmpty()) {
        return 0.0;
    }

    double sum = 0.0;
    for (double value : values) {
        sum += value;
    }
    return sum / values.size();
}

double standardDeviation(const QVector<double>& values)
{
    if (values.size() <= 1) {
        return 0.0;
    }

    const double avg = average(values);
    double varianceSum = 0.0;
    for (double value : values) {
        const double diff = value - avg;
        varianceSum += diff * diff;
    }
    return std::sqrt(varianceSum / values.size());
}

QString formatTime(const QDateTime& dateTime)
{
    return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

} // namespace

namespace VEFCSensorMonitor {

DailyStatsReport DailyStatsCalculator::buildReport(const QDate& statDate,
                                                   const QStringList& qrcodes,
                                                   const QVector<VEFCSensorMonitorRecord>& records,
                                                   const QHash<QString, VEFCSensorMonitorRecord>& softwareFirstOpenRecords)
{
    DailyStatsReport report;
    report.statDate = statDate;
    report.rangeStart = QDateTime(statDate, QTime(0, 0, 0));
    report.rangeEnd = QDateTime(statDate, QTime(23, 59, 59));
    report.generatedAt = QDateTime::currentDateTime();

    QStringList sortedQrcodes = qrcodes;
    sortedQrcodes.removeDuplicates();
    std::sort(sortedQrcodes.begin(), sortedQrcodes.end(), qrcodeNumericLess);

    QHash<QString, QVector<VEFCSensorMonitorRecord>> groupedRecords;
    for (const VEFCSensorMonitorRecord& record : records) {
        groupedRecords[record.qrCode].append(record);
    }

    report.deviceCount = sortedQrcodes.size();
    report.devices.reserve(sortedQrcodes.size());

    for (const QString& qrCode : sortedQrcodes) {
        DailyDeviceStats stats = buildDeviceStats(qrCode, groupedRecords.value(qrCode), softwareFirstOpenRecords);
        if (stats.hasData) {
            ++report.validDeviceCount;
        } else {
            ++report.noDataDeviceCount;
        }
        report.devices.append(stats);
    }

    return report;
}

DailyDeviceStats DailyStatsCalculator::buildDeviceStats(const QString& qrCode,
                                                        const QVector<VEFCSensorMonitorRecord>& records,
                                                        const QHash<QString, VEFCSensorMonitorRecord>& softwareFirstOpenRecords)
{
    DailyDeviceStats stats;
    stats.qrCode = qrCode;
    if (softwareFirstOpenRecords.contains(qrCode)) {
        stats.hasSoftwareFirstOpenRecord = true;
        stats.softwareFirstOpenRecord = softwareFirstOpenRecords.value(qrCode);
    }
    stats.sampleCount = records.size();
    stats.validCount = records.size();
    stats.hasData = !records.isEmpty();
    if (!stats.hasData) {
        return stats;
    }

    QVector<VEFCSensorMonitorRecord> sortedRecords = records;
    std::sort(sortedRecords.begin(), sortedRecords.end(),
              [](const VEFCSensorMonitorRecord& left, const VEFCSensorMonitorRecord& right) {
                  return left.recordTimestamp < right.recordTimestamp;
              });

    QVector<double> sensorPressures;
    QVector<double> sensorTemperatures;
    QVector<double> actualFlows;
    QVector<double> flowPressureRatios;
    sensorPressures.reserve(sortedRecords.size());
    sensorTemperatures.reserve(sortedRecords.size());
    actualFlows.reserve(sortedRecords.size());
    flowPressureRatios.reserve(sortedRecords.size());

    stats.firstRecord = sortedRecords.first();
    stats.lastRecord = sortedRecords.last();
    stats.sensorTemperatureMax = sortedRecords.first().sensorTemperature;

    for (const VEFCSensorMonitorRecord& record : sortedRecords) {
        sensorPressures.append(record.sensorPressure);
        sensorTemperatures.append(record.sensorTemperature);
        actualFlows.append(record.actualFlow);
        stats.sensorTemperatureMax = qMax(stats.sensorTemperatureMax, record.sensorTemperature);

        if (std::fabs(record.gasPressure) > kZeroPressureEpsilon) {
            flowPressureRatios.append(record.actualFlow / record.gasPressure);
        }
    }

    stats.sensorPressureAvg = average(sensorPressures);
    stats.sensorPressureStddev = standardDeviation(sensorPressures);
    stats.sensorTemperatureAvg = average(sensorTemperatures);
    stats.actualFlowAvg = average(actualFlows);
    stats.actualFlowStddev = standardDeviation(actualFlows);
    stats.hasFlowPressureRatio = !flowPressureRatios.isEmpty();
    stats.flowPressureRatioAvg = average(flowPressureRatios);
    return stats;
}

} // namespace VEFCSensorMonitor

VEFCSensorMonitorDailyStatsService::VEFCSensorMonitorDailyStatsService()
    : m_dailyStatsLogger("scheduler/vefc_sensor_monitor_task/summary",
                         LoggerConfig::getInstance()->isVEFCSensorMonitorTaskSummaryEnabled())
{
}

void VEFCSensorMonitorDailyStatsService::writeDailyStats(const QDate& statDate,
                                                         const QStringList& qrcodes,
                                                         const QVector<VEFCSensorMonitorRecord>& records,
                                                         const QHash<QString, VEFCSensorMonitorRecord>& softwareFirstOpenRecords)
{
    const VEFCSensorMonitor::DailyStatsReport report =
        VEFCSensorMonitor::DailyStatsCalculator::buildReport(statDate, qrcodes, records, softwareFirstOpenRecords);
    m_dailyStatsLogger.info(formatReport(report).toStdString());
    m_dailyStatsLogger.flush();
}

void VEFCSensorMonitorDailyStatsService::writeDailyStatsFailed(const QDate& statDate, const QString& reason)
{
    m_dailyStatsLogger.error(QString(
        "================================================================================\n"
        "VEFC 传感器寿命日统计失败\n"
        "统计日期：%1\n"
        "生成时间：%2\n"
        "失败原因：%3\n"
        "================================================================================")
        .arg(statDate.toString(QStringLiteral("yyyy-MM-dd")))
        .arg(formatTime(QDateTime::currentDateTime()))
        .arg(reason)
        .toStdString());
    m_dailyStatsLogger.flush();
}

QString VEFCSensorMonitorDailyStatsService::formatReport(const VEFCSensorMonitor::DailyStatsReport& report)
{
    QStringList lines;
    lines << QStringLiteral("================================================================================");
    lines << QStringLiteral("VEFC 传感器寿命日统计");
    lines << QStringLiteral("统计日期：%1").arg(report.statDate.toString(QStringLiteral("yyyy-MM-dd")));
    lines << QStringLiteral("统计范围：%1 ~ %2")
        .arg(formatTime(report.rangeStart))
        .arg(formatTime(report.rangeEnd));
    lines << QStringLiteral("生成时间：%1").arg(formatTime(report.generatedAt));
    lines << QStringLiteral("设备总数：%1").arg(report.deviceCount);
    lines << QStringLiteral("排序规则：按 QRCode 转数字后升序排列");
    lines << QStringLiteral("================================================================================");

    for (const VEFCSensorMonitor::DailyDeviceStats& stats : report.devices) {
        lines << QString();
        lines << formatDeviceStats(stats);
    }

    lines << QString();
    lines << QStringLiteral("================================================================================");
    lines << QStringLiteral("VEFC 传感器寿命日统计结束");
    lines << QStringLiteral("统计日期：%1").arg(report.statDate.toString(QStringLiteral("yyyy-MM-dd")));
    lines << QStringLiteral("================================================================================");
    return lines.join(QStringLiteral("\n"));
}

QString VEFCSensorMonitorDailyStatsService::formatDeviceStats(const VEFCSensorMonitor::DailyDeviceStats& stats)
{
    QStringList lines;
    lines << QStringLiteral("--------------------------------------------------------------------------------");
    lines << QStringLiteral("设备统计开始");
    lines << QStringLiteral("二维码：%1").arg(stats.qrCode);
    lines << QStringLiteral("采样总数：%1").arg(stats.sampleCount);
    lines << QStringLiteral("--------------------------------------------------------------------------------");

    lines << QStringLiteral("软件第一次打开记录：%1").arg(
        stats.hasSoftwareFirstOpenRecord ? formatRecordSummary(stats.softwareFirstOpenRecord)
                                         : QStringLiteral("N/A"));

    if (stats.hasData) {
        lines << QStringLiteral("VEFC压力平均值：%1").arg(formatDouble(stats.sensorPressureAvg, 2, QStringLiteral("KPa")));
        lines << QStringLiteral("VEFC温度平均值：%1").arg(formatDouble(stats.sensorTemperatureAvg, 2, QStringLiteral("℃")));
        lines << QStringLiteral("今日第一次记录：%1").arg(formatRecordSummary(stats.firstRecord));
        lines << QStringLiteral("今日最后一次记录：%1").arg(formatRecordSummary(stats.lastRecord));
    } else {
        lines << QStringLiteral("VEFC压力平均值：N/A");
        lines << QStringLiteral("VEFC温度平均值：N/A");
        lines << QStringLiteral("今日第一次记录：N/A");
        lines << QStringLiteral("今日最后一次记录：N/A");
    }

    lines << QStringLiteral("--------------------------------------------------------------------------------");
    lines << QStringLiteral("设备统计结束");
    lines << QStringLiteral("二维码：%1").arg(stats.qrCode);
    lines << QStringLiteral("--------------------------------------------------------------------------------");
    return lines.join(QStringLiteral("\n"));
}

QString VEFCSensorMonitorDailyStatsService::formatRecordSummary(const VEFCSensorMonitorRecord& record)
{
    return QStringLiteral(
               "记录时间=%1, 气体气压=%2, 实际流量=%3, VEFC压力=%4, VEFC温度=%5")
        .arg(record.recordTimeString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
        .arg(formatDouble(record.gasPressure, 2, QStringLiteral("KPa")))
        .arg(formatDouble(record.actualFlow, 2, QStringLiteral("L/Min")))
        .arg(formatDouble(record.sensorPressure, 2, QStringLiteral("KPa")))
        .arg(formatDouble(record.sensorTemperature, 2, QStringLiteral("℃")));
}

QString VEFCSensorMonitorDailyStatsService::formatDouble(double value, int precision, const QString& unit)
{
    const QString number = QString::number(value, 'f', precision);
    return unit.isEmpty() ? number : QStringLiteral("%1 %2").arg(number, unit);
}
