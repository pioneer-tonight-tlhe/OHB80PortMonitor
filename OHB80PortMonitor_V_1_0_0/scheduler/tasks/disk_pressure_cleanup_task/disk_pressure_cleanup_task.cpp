#include "disk_pressure_cleanup_task.h"

#include "appconfig.h"
#include "logdatabases/databasemanager.h"
#include "loggermanager.h"
#include "qthelper.h"

#include <QDateTime>
#include <QTimer>

namespace {

const std::string DiskPressureCleanupLogPath = "scheduler/disk_pressure_cleanup_task/clean";

double bytesToGb(qint64 bytes)
{
    return bytes / 1024.0 / 1024.0 / 1024.0;
}

} // namespace

DiskPressureCleanupTask::DiskPressureCleanupTask(QObject* parent)
    : SchedulerTask(parent)
{
}

void DiskPressureCleanupTask::start()
{
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout,
                this, &DiskPressureCleanupTask::checkDiskPressure);
    }

    if (!m_timer->isActive()) {
        m_timer->start(CheckIntervalMs);
    }

    setState(Running);
    emit progress(0, QStringLiteral("Disk pressure cleanup task ready"));
}

void DiskPressureCleanupTask::stop()
{
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }

    setState(Cancelled);
    emit finished(false, QStringLiteral("Disk pressure cleanup task stopped"));
}

void DiskPressureCleanupTask::checkDiskPressure()
{
    if (m_cleanupRunning) {
        return;
    }

    const DiskUsageInfo usage = readDiskUsage();
    if (!usage.valid) {
        writeLog(Level::WARN,
                 QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                "磁盘高水位清理检查失败\n"
                                "检查路径：%1\n"
                                "失败原因：无法读取磁盘容量或已用容量\n"
                                "--------------------------------------------------------------------------------")
                     .arg(AppConfig::getInstance().getDatabaseDir()));
        return;
    }

    if (usage.usageRatio < DiskUsageThreshold) {
        return;
    }

    m_cleanupRunning = true;
    performCleanup(usage);
    m_cleanupRunning = false;
}

DiskPressureCleanupTask::DiskUsageInfo DiskPressureCleanupTask::readDiskUsage() const
{
    DiskUsageInfo info;
    const QString checkPath = AppConfig::getInstance().getDatabaseDir();
    info.totalBytes = QtHelper::diskTotalBytes(checkPath);
    info.usedBytes = QtHelper::diskUsedBytes(checkPath);
    info.valid = info.totalBytes > 0 && info.usedBytes >= 0;
    if (info.valid) {
        info.usageRatio = static_cast<double>(info.usedBytes) / info.totalBytes;
    }
    return info;
}

void DiskPressureCleanupTask::performCleanup(const DiskUsageInfo& beforeUsage)
{
    writeLog(Level::WARN,
             QStringLiteral("\n--------------------------------------------------------------------------------\n"
                            "磁盘高水位清理触发\n"
                            "检查路径：%1\n"
                            "当前磁盘：%2\n"
                            "触发阈值：%3%\n"
                            "清理顺序：LoggerManager日志 -> communicatelogdb -> vefcsensormonitordb -> operationlogdb -> alarmlogdb\n"
                            "--------------------------------------------------------------------------------")
                 .arg(AppConfig::getInstance().getDatabaseDir(),
                      formatDiskUsage(beforeUsage))
                 .arg(QString::number(DiskUsageThreshold * 100.0, 'f', 1)));

    cleanupLoggerManagerLogs();

    const DiskUsageInfo afterLoggerCleanup = readDiskUsage();
    if (afterLoggerCleanup.valid && afterLoggerCleanup.usageRatio < DiskUsageThreshold) {
        writeLog(Level::INFO,
                 QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                "磁盘高水位清理结束\n"
                                "结束原因：LoggerManager日志清理后磁盘使用率已低于阈值\n"
                                "清理后磁盘：%1\n"
                                "--------------------------------------------------------------------------------")
                     .arg(formatDiskUsage(afterLoggerCleanup)));
        return;
    }

    const QList<DatabaseCleanupTarget> targets = buildDatabaseCleanupTargets();
    if (targets.isEmpty()) {
        writeLog(Level::WARN,
                 QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                "磁盘高水位数据库清理跳过\n"
                                "跳过原因：数据库连接未就绪\n"
                                "--------------------------------------------------------------------------------"));
        return;
    }

    for (const DatabaseCleanupTarget& target : targets) {
        cleanupDatabase(target, beforeUsage);

        const DiskUsageInfo currentUsage = readDiskUsage();
        if (currentUsage.valid && currentUsage.usageRatio < DiskUsageThreshold) {
            writeLog(Level::INFO,
                     QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                    "磁盘高水位数据库清理结束\n"
                                    "结束原因：当前磁盘使用率已低于阈值\n"
                                    "当前磁盘：%1\n"
                                    "--------------------------------------------------------------------------------")
                         .arg(formatDiskUsage(currentUsage)));
            return;
        }
    }
}

bool DiskPressureCleanupTask::cleanupLoggerManagerLogs()
{
    const int removedCount =
        LoggerManager::getInstance()->cleanupLogsKeepRecentMonths(LoggerRetainMonths);

    writeLog(Level::INFO,
             QStringLiteral("\n--------------------------------------------------------------------------------\n"
                            "LoggerManager日志清理\n"
                            "保留策略：仅保留最近 %1 个月（包含今天所在月份）的日志日期目录\n"
                            "删除结果：已删除 %2 个过期日期目录\n"
                            "--------------------------------------------------------------------------------")
                 .arg(LoggerRetainMonths)
                 .arg(removedCount));

    return removedCount > 0;
}

bool DiskPressureCleanupTask::cleanupDatabase(const DatabaseCleanupTarget& target,
                                              const DiskUsageInfo& triggerUsage)
{
    QString earliestTime;
    QString latestTime;
    if (!target.queryTimeBounds || !target.deleteByRange) {
        writeLog(Level::WARN,
                 QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                "磁盘高水位数据库清理跳过\n"
                                "数据库：%1\n"
                                "表名：%2\n"
                                "跳过原因：清理回调未配置\n"
                                "--------------------------------------------------------------------------------")
                     .arg(target.databaseName, target.tableName));
        return false;
    }

    target.queryTimeBounds(earliestTime, latestTime);
    if (earliestTime.isEmpty() || latestTime.isEmpty()) {
        writeLog(Level::INFO,
                 QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                "磁盘高水位数据库清理跳过\n"
                                "数据库：%1\n"
                                "表名：%2\n"
                                "跳过原因：数据库无可清理记录\n"
                                "--------------------------------------------------------------------------------")
                     .arg(target.databaseName, target.tableName));
        return false;
    }

    const int monthDiff = calculateMonthDifference(earliestTime, latestTime);
    if (monthDiff <= DatabaseMinRetainMonths) {
        writeLog(Level::INFO,
                 QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                "磁盘高水位数据库清理跳过\n"
                                "数据库：%1\n"
                                "表名：%2\n"
                                "保留底线：至少保留最近 %3 个月\n"
                                "当前跨度：%4 个月\n"
                                "数据库最早记录：%5\n"
                                "数据库最新记录：%6\n"
                                "跳过原因：继续删除会低于保留底线\n"
                                "--------------------------------------------------------------------------------")
                     .arg(target.databaseName, target.tableName)
                     .arg(DatabaseMinRetainMonths)
                     .arg(monthDiff)
                     .arg(earliestTime, latestTime));
        return false;
    }

    const QDate earliestDate = QDate::fromString(earliestTime.left(10), QStringLiteral("yyyy-MM-dd"));
    if (!earliestDate.isValid()) {
        writeLog(Level::ERROR,
                 QStringLiteral("\n--------------------------------------------------------------------------------\n"
                                "磁盘高水位数据库清理失败\n"
                                "数据库：%1\n"
                                "表名：%2\n"
                                "失败原因：最早记录时间格式无效\n"
                                "数据库最早记录：%3\n"
                                "--------------------------------------------------------------------------------")
                     .arg(target.databaseName, target.tableName, earliestTime));
        return false;
    }

    const QDate cutoffDate = earliestDate.addMonths(DatabaseCleanupMonths);
    const QString startTime = earliestDate.toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    const QString endTime = cutoffDate.toString(QStringLiteral("yyyy-MM-dd 23:59:59"));
    const QString monthsText = buildMonthList(earliestDate, cutoffDate).join(QStringLiteral(", "));

    const bool submitted = target.deleteByRange(startTime, endTime);
    writeLog(submitted ? Level::WARN : Level::ERROR,
             QStringLiteral("\n--------------------------------------------------------------------------------\n"
                            "磁盘高水位数据库清理%1\n"
                            "数据库：%2\n"
                            "表名：%3\n"
                            "触发磁盘：%4\n"
                            "保留底线：至少保留最近 %5 个月\n"
                            "当前跨度：%6 个月\n"
                            "每次清理：最早 %7 个月\n"
                            "数据库最早记录：%8\n"
                            "数据库最新记录：%9\n"
                            "清理范围：%10 至 %11\n"
                            "涉及月份：[%12]\n"
                            "提交结果：%13\n"
                            "--------------------------------------------------------------------------------")
                 .arg(submitted ? QStringLiteral("触发") : QStringLiteral("失败"),
                      target.databaseName,
                      target.tableName,
                      formatDiskUsage(triggerUsage))
                 .arg(DatabaseMinRetainMonths)
                 .arg(monthDiff)
                 .arg(DatabaseCleanupMonths)
                 .arg(earliestTime,
                      latestTime,
                      startTime,
                      endTime,
                      monthsText,
                      submitted ? QStringLiteral("已提交删除任务")
                                : QStringLiteral("删除任务提交失败")));

    return submitted;
}

QList<DiskPressureCleanupTask::DatabaseCleanupTarget>
DiskPressureCleanupTask::buildDatabaseCleanupTargets() const
{
    QList<DatabaseCleanupTarget> targets;
    LogDB::DatabaseManager& manager = LogDB::DatabaseManager::instance();

    if (auto* db = manager.communicateLogCon()) {
        targets.append({
            QStringLiteral("通信日志数据库"),
            QStringLiteral("communicate_log"),
            [db](QString& earliest, QString& latest) { db->queryTimeBounds(earliest, latest); },
            [db](const QString& startTime, const QString& endTime) {
                db->deleteByTimeRange(startTime, endTime);
                return true;
            }
        });
    }

    if (auto* db = manager.vefcSensorMonitorCon()) {
        targets.append({
            QStringLiteral("VEFC采集数据库"),
            QStringLiteral("vefc_sensor_monitor"),
            [db](QString& earliest, QString& latest) { db->queryTimeBounds(earliest, latest); },
            [db](const QString& startTime, const QString& endTime) {
                return db->deleteByDateTimeRange(startTime, endTime);
            }
        });
    }

    if (auto* db = manager.operationLogCon()) {
        targets.append({
            QStringLiteral("操作日志数据库"),
            QStringLiteral("operation_log"),
            [db](QString& earliest, QString& latest) { db->queryTimeBounds(earliest, latest); },
            [db](const QString& startTime, const QString& endTime) {
                db->deleteByTimeRange(startTime, endTime);
                return true;
            }
        });
    }

    if (auto* db = manager.alarmLogCon()) {
        targets.append({
            QStringLiteral("报警日志数据库"),
            QStringLiteral("alarm_log"),
            [db](QString& earliest, QString& latest) { db->queryTimeBounds(earliest, latest); },
            [db](const QString& startTime, const QString& endTime) {
                db->deleteByTimeRange(startTime, endTime);
                return true;
            }
        });
    }

    return targets;
}

int DiskPressureCleanupTask::calculateMonthDifference(const QString& earliestTime,
                                                      const QString& latestTime)
{
    const QDate earliestDate = QDate::fromString(earliestTime.left(10), QStringLiteral("yyyy-MM-dd"));
    const QDate latestDate = QDate::fromString(latestTime.left(10), QStringLiteral("yyyy-MM-dd"));
    if (!earliestDate.isValid() || !latestDate.isValid()) {
        return 0;
    }

    return (latestDate.year() - earliestDate.year()) * 12
           + latestDate.month() - earliestDate.month();
}

QStringList DiskPressureCleanupTask::buildMonthList(const QDate& earliestDate,
                                                    const QDate& cutoffDate)
{
    QStringList months;
    QDate monthIter(earliestDate.year(), earliestDate.month(), 1);
    while (monthIter <= cutoffDate) {
        months.append(monthIter.toString(QStringLiteral("yyyy-MM")));
        monthIter = monthIter.addMonths(1);
    }
    months.removeDuplicates();
    return months;
}

QString DiskPressureCleanupTask::formatDiskUsage(const DiskUsageInfo& info)
{
    if (!info.valid) {
        return QStringLiteral("N/A");
    }

    return QStringLiteral("%1%（已用 %2 GB / 总计 %3 GB）")
        .arg(QString::number(info.usageRatio * 100.0, 'f', 1),
             QString::number(bytesToGb(info.usedBytes), 'f', 2),
             QString::number(bytesToGb(info.totalBytes), 'f', 2));
}

void DiskPressureCleanupTask::writeLog(Level level, const QString& message) const
{
    LoggerManager::getInstance()->log(DiskPressureCleanupLogPath, level, message.toStdString());
    LoggerManager::getInstance()->flush(DiskPressureCleanupLogPath);
}
