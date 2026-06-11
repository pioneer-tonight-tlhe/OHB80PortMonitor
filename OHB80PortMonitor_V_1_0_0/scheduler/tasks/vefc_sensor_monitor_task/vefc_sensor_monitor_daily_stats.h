#ifndef VEFC_SENSOR_MONITOR_DAILY_STATS_H
#define VEFC_SENSOR_MONITOR_DAILY_STATS_H

#include "classes/vefcsensormonitorrecord.h"
#include "ilogger.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVector>

// ====================================================================
// VEFCSensorMonitorDailyStatsService - VEFC 传感器寿命日统计服务
//
// 设计目标：
//   1. 基于 vefc_sensor_monitor 原始记录计算每天每台设备的寿命观察指标。
//   2. 所有设备统计写入同一个 daily_stats.log，并通过设备边界区分 QRCode。
//   3. 输出顺序按 QRCode 转数字后升序排列，方便和现场设备编号直接对应。
// ====================================================================
namespace VEFCSensorMonitor {

struct DailyDeviceStats {
    QString qrCode;
    int sampleCount = 0;
    int validCount = 0;
    bool hasData = false;
    bool hasFlowPressureRatio = false;
    double sensorPressureAvg = 0.0;
    double sensorPressureStddev = 0.0;
    double sensorTemperatureAvg = 0.0;
    double sensorTemperatureMax = 0.0;
    double actualFlowAvg = 0.0;
    double actualFlowStddev = 0.0;
    double flowPressureRatioAvg = 0.0;
    bool hasSoftwareFirstOpenRecord = false;
    VEFCSensorMonitorRecord softwareFirstOpenRecord;
    VEFCSensorMonitorRecord firstRecord;
    VEFCSensorMonitorRecord lastRecord;
};

struct DailyStatsReport {
    QDate statDate;
    QDateTime rangeStart;
    QDateTime rangeEnd;
    QDateTime generatedAt;
    int deviceCount = 0;
    int validDeviceCount = 0;
    int noDataDeviceCount = 0;
    QList<DailyDeviceStats> devices;
};

class DailyStatsCalculator
{
public:
    // 按设备列表生成日统计报告；设备列表会按 QRCode 数字升序排序。
    static DailyStatsReport buildReport(const QDate& statDate,
                                        const QStringList& qrcodes,
                                        const QVector<VEFCSensorMonitorRecord>& records,
                                        const QHash<QString, VEFCSensorMonitorRecord>& softwareFirstOpenRecords);

private:
    static DailyDeviceStats buildDeviceStats(const QString& qrCode,
                                             const QVector<VEFCSensorMonitorRecord>& records,
                                             const QHash<QString, VEFCSensorMonitorRecord>& softwareFirstOpenRecords);
};

} // namespace VEFCSensorMonitor

class VEFCSensorMonitorDailyStatsService
{
public:
    VEFCSensorMonitorDailyStatsService();

    // 生成并写入某一天的 VEFC 寿命日统计日志。
    void writeDailyStats(const QDate& statDate,
                         const QStringList& qrcodes,
                         const QVector<VEFCSensorMonitorRecord>& records,
                         const QHash<QString, VEFCSensorMonitorRecord>& softwareFirstOpenRecords);

    // 数据库不可用或查询失败时写入统计失败日志。
    void writeDailyStatsFailed(const QDate& statDate, const QString& reason);

private:
    static QString formatReport(const VEFCSensorMonitor::DailyStatsReport& report);
    static QString formatDeviceStats(const VEFCSensorMonitor::DailyDeviceStats& stats);
    static QString formatRecordSummary(const VEFCSensorMonitorRecord& record);
    static QString formatDouble(double value, int precision, const QString& unit = QString());

private:
    ILogger m_dailyStatsLogger;
};

#endif // VEFC_SENSOR_MONITOR_DAILY_STATS_H
