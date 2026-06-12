/*******************************************************************************************
 * @file disk_pressure_cleanup_task.h
 * @author pioneer-cytc <工号：未知> 2026-06-12
 *
 * @class DiskPressureCleanupTask
 * @brief 负责在磁盘占用超过阈值时协调日志与数据库的分级清理。
 *
 * 设计目标：
 *      1. 统一封装磁盘水位检测、阈值判断和周期调度，避免清理逻辑分散在配置层与数据库层。
 *      2. 先清理 LoggerManager 日志，再按数据库优先级逐步释放空间，并保持最小数据保留边界。
 *      3. 让清理动作具备可观测性，所有触发、跳过和失败结果都通过专用日志落盘，便于问题追踪。
 *******************************************************************************************/
#ifndef DISK_PRESSURE_CLEANUP_TASK_H
#define DISK_PRESSURE_CLEANUP_TASK_H

#include "../../scheduler_task.h"

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
    // ============================ 公共数据类型 ============================
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

    // ============================ 构造与析构 ============================
    explicit DiskPressureCleanupTask(QObject* parent = nullptr);
    ~DiskPressureCleanupTask() override = default;

    // ============================ 基类相关接口 ============================
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;

    QString taskType() const override { return QStringLiteral("DiskPressureCleanupTask"); }
    bool isPersistent() const override { return true; }
    bool isRecurring() const override { return true; }
    int intervalMs() const override { return CheckIntervalMs; }

private:
    // ============================ 磁盘压力检查 ============================
    DiskUsageInfo readDiskUsage() const;
    void performCleanup(const DiskUsageInfo& beforeUsage);
    bool cleanupLoggerManagerLogs();
    bool cleanupDatabase(const DatabaseCleanupTarget& target,
                         const DiskUsageInfo& triggerUsage);

    // ============================ 清理策略计算 ============================
    QList<DatabaseCleanupTarget> buildDatabaseCleanupTargets() const;
    static int calculateMonthDifference(const QString& earliestTime,
                                        const QString& latestTime);
    static QStringList buildMonthList(const QDate& earliestDate,
                                      const QDate& cutoffDate);
    static QString formatDiskUsage(const DiskUsageInfo& info);
    void writeLog(Level level, const QString& message) const;

private slots:
    // ---- 磁盘压力检查 ----
    void checkDiskPressure();

private:
    // ---- 调度参数 ----
    static constexpr int CheckIntervalMs = 60000;
    static constexpr double DiskUsageThreshold = 0.90;
    static constexpr int LoggerRetainMonths = 2;
    static constexpr int DatabaseMinRetainMonths = 6;
    static constexpr int DatabaseCleanupMonths = 1;

    // ---- 状态成员 ----
    QTimer* m_timer = nullptr;
    bool m_cleanupRunning = false;
};

#endif // DISK_PRESSURE_CLEANUP_TASK_H
