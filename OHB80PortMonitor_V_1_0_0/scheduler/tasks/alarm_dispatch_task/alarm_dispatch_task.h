#ifndef ALARM_DISPATCH_TASK_H
#define ALARM_DISPATCH_TASK_H

#include "../../scheduler_task.h"
#include "alarm_dispatch_task_logger.h"
#include "alarminfo.h"
#include "alarmtype.h"
#include "config/alarmconfig.h"

#include <QHash>
#include <QMutex>
#include <QString>
#include <QTimer>
#include <QSet>
#include <QStringList>
#include <algorithm>

// ====================================================================
// AlarmDispatchTask 实现流程
//
//   1. 启动阶段
//      start() 从 alarm_log 数据库加载所有未解决告警，恢复到 m_active。
//      恢复完成后启动定时器，由定时器异步刷新 UI 上的设备告警状态。
//
//   2. 告警发生
//      外部任务只调用 submitAlarm() 上报告警，不直接修改 FoupOfOHBInfo。
//      submitAlarm() 会先 normalize() 补齐 alarmId、alarmLevel、occurTime，
//      再按 alarmId 对 m_active 去重，新的 active 告警才会写入数据库。
//
//   3. 告警恢复
//      外部任务只调用 submitResolve() 恢复告警。
//      submitResolve() 会从 m_active 移除对应 alarmId，并更新数据库解决状态。
//      移除后不能简单把设备清红，而是重新扫描该设备剩余 active 告警。
//
//   4. UI 告警状态同步
//      定时器触发 syncAllFoupAlarmStates() 后，再调用 syncFoupAlarmState() 写入
//      FoupOfOHBInfo::hasAlarm/alarmId 的位置。
//      如果同一设备还有其它 active 告警，设备继续保持告警状态；
//      如果没有任何 active 告警，才清除 hasAlarm 和 alarmId。
//
//   5. 多告警展示策略
//      一个设备可能同时存在离线、VEFC 异常、SH85 异常、湿度未达标等告警。
//      FoupOfOHBInfo 只能保存一个 alarmId 给 UI 展示，因此通过
//      selectedActiveAlarmIdForQrCode() 选择 alarmLevel 最高的告警作为主告警。
//
// 设计约束：
//   MonitorDataTask、NetworkStatusTask 等业务任务只负责提交/恢复告警事件；
//   告警状态是否标红、显示哪个 alarmId，统一由 AlarmDispatchTask 的定时器刷新决定。
// ====================================================================

// ====================================================================
// AlarmDispatchTask —— 警报调度常驻任务（取代老 AlarmLogicSystem）
//
// 职责：
//   1. 业务侧（任意线程）通过 SharedData::getAlarmDispatchTask() 拿到本任务，
//      调用 submitAlarm / submitResolve 提交事件。
//   2. 内部按 AlarmInfo::alarmId 字符串去重；活跃集合放在 m_active。
//   3. 持久化：调用 LogDB::AlarmLogDBCon::insertRecord / updateResolve。
//   4. 派发：emit alarmPublished / alarmResolved 供 UI live log 等订阅。
//
// 数据载体：
//   使用 classes/alarminfo.h 中的 AlarmInfo（与 alarm_log 表字段对齐）。
//   AlarmInfo::alarmId 由 generateAlarmId() 生成，规则：
//     "level(1) + sourceIdentifier(5) + type(4)"
//
// 用法：
//   // 提交（推荐简化入参；alarmLevel 由 alarmtype.h 推导）
//   SharedData::getAlarmDispatchTask()->submitAlarm(
//       static_cast<int>(AlarmType::DeviceOffline),
//       static_cast<int>(AlarmSource::Device),
//       qrCode,
//       QStringLiteral("Device %1 connection lost").arg(qrCode));
//
//   // 解决（同样的 type/source/identifier 即可还原 alarmId）
//   SharedData::getAlarmDispatchTask()->submitResolve(
//       static_cast<int>(AlarmType::DeviceOffline),
//       static_cast<int>(AlarmSource::Device),
//       qrCode);
//
//   // UI 订阅：
//   connect(SharedData::getAlarmDispatchTask(),
//           &AlarmDispatchTask::alarmPublished,
//           this, &MyWidget::onAlarmPublished);
// ====================================================================
class AlarmDispatchTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit AlarmDispatchTask(QObject* parent = nullptr);
    ~AlarmDispatchTask() override;

    // SchedulerTask 接口 —— 本任务为常驻、无周期，仅做事件派发
    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("AlarmDispatchTask"); }
    bool isPersistent() const override { return true; }

    // 简化提交（线程安全）：依据 alarmType 自动推导 alarmLevel，
    // occurTime 自动取当前时刻，alarmId 由 generateAlarmId 生成。
    // 返回构造完成后的 alarmId（可用于后续 submitResolve）。
    QString submitAlarm(int alarmType,
                        int alarmSource,
                        const QString& sourceIdentifier,
                        const QString& description);

    // 完整提交（线程安全）：调用方已构造好 AlarmInfo。
    // - occurTime 为空时自动取当前时刻
    // - alarmId 为空时调用 generateAlarmId
    // - alarmLevel 为 0 时按 alarmType 自动推导
    QString submitAlarm(AlarmInfo info);

    // 解决（线程安全）—— 重载 1：直接传 alarmId 字符串
    void submitResolve(const QString& alarmId);

    // 解决（线程安全）—— 重载 2：与 submitAlarm 对称，内部按相同规则生成
    // alarmId 后转发。业务侧通常无需缓存 alarmId。
    void submitResolve(int alarmType,
                       int alarmSource,
                       const QString& sourceIdentifier);

    int submitResolveAllBySourceIdentifier(int alarmSource,
                                           const QString& sourceIdentifier);
    int submitResolveAllByQRCode(const QString& qrCode);

    // 查询某 alarmId 是否仍活跃（线程安全）
    bool isActive(const QString& alarmId) const;

    // 当前活跃警报数量（线程安全）
    int activeCount() const;

    // 取出当前所有活跃警报的快照（线程安全）。
    // 用于 UI 订阅 alarmPublished 信号之前的补播 —— 避免 UI 控件构造时机晚于
    // AlarmDispatchTask::start() 时 loadActiveFromDb 已经 emit 完而丢失显示。
    QList<AlarmInfo> activeAlarms() const;

    // 清空活跃集合（不写库；保留接口给跨日重置等场景）
    void clearActive();

signals:
    // 新警报发布（去重通过后；emit 时已成功投递到 LogDB）
    void alarmPublished(const AlarmInfo& info);

    // 警报已解决（携带带有 resolvedTime 的完整 info）
    void alarmResolved(const AlarmInfo& info);

    // 警报日志插入完成信号（携带 AlarmRecord）
    void alarmLogInserted(const AlarmRecord& record);

    // 警报解决已落库信号（DB UPDATE 完成后 emit，携带完整 AlarmRecord 含 id）
    // 用于 UI 端（如 scrollingTipLabel）根据 id 从队列中移除
    void alarmResolvePersisted(const AlarmRecord& record);

private:
    // 把 AlarmInfo 写入 alarm_log（INSERT）
    void persistInsert(const AlarmInfo& info);

    static constexpr int kFoupAlarmSyncIntervalMs = 50;

    // 把同 alarm_type + 未解决的行原位标记为已解决
    void persistResolve(const AlarmInfo& info);

    // 给一个未填齐的 AlarmInfo 补默认字段
    void normalize(AlarmInfo& info) const;

    // 初始化告警调度任务专属日志模块。
    void initAlarmDispatchTaskLogger();

    // 启动/停止 FOUP 告警状态刷新定时器。
    // submitAlarm/submitResolve 只维护 m_active，不直接写 FoupOfOHBInfo。
    void startFoupAlarmSyncTimer();
    void stopFoupAlarmSyncTimer();

    // 启动时从 alarm_log 表加载所有 is_resolved=0 的记录到 m_active，
    // 让本次启动能继续监控上一次未解决的警报（避免重复 INSERT、可对其 submitResolve）
    void loadActiveFromDb();

    // DB 写入完成回调：检测到 update_resolve 完成后查询完整记录并 emit alarmResolvePersisted
    void onAlarmDBRecordResolved(const QString& qrCode, const QString& alarmType, const QString& resolveTime);

    // UI 告警状态只允许由定时器根据 m_active 统一刷新。
    // 这样可以避免 MonitorDataTask / NetworkStatusTask 各自清红，误清其它仍然活跃的告警。
    //
    // 根据当前 active 告警集合，统一同步指定设备的 FoupOfOHBInfo::hasAlarm/alarmId。
    void syncFoupAlarmState(const QString& qrCode);
    void syncFoupAlarmState(const QString& qrCode, const QSet<QString>& blockedAlarmTypes);

    // 启动恢复、批量重置后使用：按 m_active 全量刷新所有设备的告警显示状态。
    void syncAllFoupAlarmStates();

    // 当前 SharedData 已不存在的设备 QRCode 不应继续保留 active 告警。
    void resolveMissingQRCodeActiveAlarms(const QSet<QString>& currentQrCodes);

    // 从 m_active 中选择指定设备当前最需要展示的告警 ID；优先级按 alarmLevel 从高到低。
    QString selectedActiveAlarmIdForQrCode(const QString& qrCode) const;
    QString selectedActiveAlarmIdForQrCode(const QString& qrCode, const QSet<QString>& blockedAlarmTypes) const;

    QHash<QString, AlarmInfo> m_active;
    mutable QMutex            m_mutex;
    AlarmDispatchTaskLogger*  m_logger = nullptr;
    QTimer*                   m_foupAlarmSyncTimer = nullptr;

/*汇总日志*/
private slots:
    // 输出当前未恢复错误的汇总快照。
    void writeSummarySnapshot();

private:
    void initSummaryTimer();
    // 启动/停止 Summary 周期输出定时器。
    void startSummaryTimer();
    void stopSummaryTimer();


    QTimer* m_summaryTimer = nullptr;
    
};

#endif // ALARM_DISPATCH_TASK_H
