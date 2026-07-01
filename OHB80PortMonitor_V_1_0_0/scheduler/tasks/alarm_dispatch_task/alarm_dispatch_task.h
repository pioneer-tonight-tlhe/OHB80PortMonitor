/*******************************************************************************************
 * @file alarm_dispatch_task.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class AlarmDispatchTask
 * @brief 负责统一接收、去重、持久化和分发系统警报事件。
 *
 * 设计目标：
 *      1. 为各业务任务提供线程安全的警报上报与恢复入口，统一调度告警生命周期。
 *      2. 维护活跃警报集合并同步落库，避免多处直接修改警报状态导致不一致。
 *      3. 统一驱动 UI 实时警报显示、FOUP 警报状态刷新和警报恢复后的信号分发。
 *******************************************************************************************/
#ifndef ALARM_DISPATCH_TASK_H
#define ALARM_DISPATCH_TASK_H

#include <QHash>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

#include "../../scheduler_task.h"
#include "alarm_dispatch_task_logger.h"
#include "alarminfo.h"
#include "alarmrecord.h"
#include "alarmtype.h"
#include "config/alarmconfig.h"

class AlarmDispatchTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit AlarmDispatchTask(QObject* parent = nullptr);
    ~AlarmDispatchTask() override;

    // ============================ 任务生命周期 ============================
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("AlarmDispatchTask"); }
    bool isPersistent() const override { return true; }

    // ============================ 警报提交接口 ============================
    QString submitAlarm(int alarmType,
                        int alarmSource,
                        const QString& sourceIdentifier,
                        const QString& description);
    QString submitAlarm(AlarmInfo info);
    void submitResolve(const QString& alarmId);
    void submitResolve(int alarmType, int alarmSource, const QString& sourceIdentifier);
    int submitResolveAllBySourceIdentifier(int alarmSource, const QString& sourceIdentifier);
    int submitResolveAllByQRCode(const QString& qrCode);

    // ============================ 状态查询接口 ============================
    bool isActive(const QString& alarmId) const;
    int activeCount() const;
    QList<AlarmInfo> activeAlarms() const;
    void clearActive();

private:
    // ---- 持久化与标准化 ----
    void persistInsert(const AlarmInfo& info);
    void persistResolve(const AlarmInfo& info);
    void normalize(AlarmInfo& info) const;

    // ---- 日志与定时器 ----
    void initAlarmDispatchTaskLogger();
    void startFoupAlarmSyncTimer();
    void stopFoupAlarmSyncTimer();
    void initSummaryTimer();
    void startSummaryTimer();
    void stopSummaryTimer();

    // ---- 启动恢复与数据库回调 ----
    void loadActiveFromDb();
    void onAlarmDBRecordResolved(const QString& qrCode, const QString& alarmType, const QString& resolveTime);
    void onAlarmDBRecordInserted(const AlarmRecord& record);

    // ---- 自动恢复辅助 ----
    void rememberRecentQRCodeResolve(const QString& qrCode);
    bool shouldAutoResolveInsertedRecord(const AlarmRecord& record);
    int resolveUnresolvedDbRowsByQRCode(const QString& qrCode);
    void scheduleResolveUnresolvedDbRowsByQRCode(const QString& qrCode);

    // ---- FOUP 状态同步 ----
    void syncFoupAlarmState(const QString& qrCode);
    void syncFoupAlarmState(const QString& qrCode, const QSet<QString>& blockedAlarmTypes);
    void syncAllFoupAlarmStates();
    void resolveMissingQRCodeActiveAlarms(const QSet<QString>& currentQrCodes);
    QString selectedActiveAlarmIdForQrCode(const QString& qrCode) const;
    QString selectedActiveAlarmIdForQrCode(const QString& qrCode,
                                           const QSet<QString>& blockedAlarmTypes) const;

signals:
    // ---- 实时结果 ----
    void alarmPublished(const AlarmInfo& info);
    void alarmResolved(const AlarmInfo& info);
    void alarmLogInserted(const AlarmRecord& record);
    void alarmResolvePersisted(const AlarmRecord& record);

private slots:
    // ---- 汇总日志 ----
    void writeSummarySnapshot();

private:
    // ---- 常量成员 ----
    static constexpr int kFoupAlarmSyncIntervalMs = 50;

    // ---- 活跃状态成员 ----
    QHash<QString, AlarmInfo> m_active;
    QHash<QString, qint64> m_recentQRCodeResolveUntilMs;
    mutable QMutex m_mutex;

    // ---- 辅助对象成员 ----
    AlarmDispatchTaskLogger* m_logger = nullptr;
    QTimer* m_foupAlarmSyncTimer = nullptr;
    QTimer* m_summaryTimer = nullptr;
};

#endif // ALARM_DISPATCH_TASK_H
