#ifndef DISK_PRESSURE_CLEANUP_TASK_H
#define DISK_PRESSURE_CLEANUP_TASK_H

#include "../scheduler_task.h"

#include <QDate>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>
#include <string>

class QTimer;
enum class Level;

class DiskPressureCleanupTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit DiskPressureCleanupTask(QObject* parent = nullptr);
    ~DiskPressureCleanupTask() override = default;

    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;

    QString taskType() const override { return QStringLiteral("DiskPressureCleanupTask"); }
    bool isPersistent() const override { return true; }
    bool isRecurring() const override { return true; }
    int intervalMs() const override { return CheckIntervalMs; }

private slots:
    void checkDiskPressure();

private:
    struct DiskUsageInfo {
        qint64 totalBytes = 0;
        qint64 usedBytes = 0;
        double usageRatio = 0.0;
        bool valid = false;
    };

    struct DatabaseCleanupTarget {
        QString databaseName;
        QString tableName;
        std::function<void(QString&, QString&)> queryTimeBounds;
        std::function<bool(const QString&, const QString&)> deleteByRange;
    };

    static constexpr int CheckIntervalMs = 60000;
    static constexpr double DiskUsageThreshold = 0.90;
    static constexpr int LoggerRetainMonths = 2;
    static constexpr int DatabaseMinRetainMonths = 6;
    static constexpr int DatabaseCleanupMonths = 1;

    DiskUsageInfo readDiskUsage() const;
    void performCleanup(const DiskUsageInfo& beforeUsage);
    bool cleanupLoggerManagerLogs();
    bool cleanupDatabase(const DatabaseCleanupTarget& target,
                         const DiskUsageInfo& triggerUsage);

    QList<DatabaseCleanupTarget> buildDatabaseCleanupTargets() const;
    static int calculateMonthDifference(const QString& earliestTime,
                                        const QString& latestTime);
    static QStringList buildMonthList(const QDate& earliestDate,
                                      const QDate& cutoffDate);
    static QString formatDiskUsage(const DiskUsageInfo& info);
    void writeLog(Level level, const QString& message) const;

    QTimer* m_timer = nullptr;
    bool m_cleanupRunning = false;
};

#endif // DISK_PRESSURE_CLEANUP_TASK_H
