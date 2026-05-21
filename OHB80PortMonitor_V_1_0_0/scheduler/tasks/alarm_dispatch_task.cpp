#include "alarm_dispatch_task.h"

#include <QDateTime>

#include "logdatabases/databasemanager.h"
#include "logdatabases/alarmlogdb/alarmlogdbcon.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "app/shareddata.h"
#include "loggermanager.h"

AlarmDispatchTask::AlarmDispatchTask(QObject* parent)
    : SchedulerTask(parent)
{
    LoggerManager::instance().log(LOG_PATH, Level::INFO, "[AlarmDispatchTask] 警报调度任务已构造");
}

void AlarmDispatchTask::start()
{
    setState(Running);
    loadActiveFromDb();

    // 连接 DB 写入完成信号，用于在解决落库后 emit alarmResolvePersisted
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (db) {
        connect(db, &LogDB::AlarmLogDBCon::recordResolved,
                this, &AlarmDispatchTask::onAlarmDBRecordResolved,
                Qt::QueuedConnection);
    }

    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[start] 警报调度任务已启动，活跃警报数=%1").arg(activeCount()).toStdString());
    emit progress(0, QStringLiteral("Alarm dispatcher running, active=%1").arg(activeCount()));
}

void AlarmDispatchTask::stop()
{
    clearActive();
    setState(Cancelled);
    emit finished(false, QStringLiteral("Alarm dispatcher stopped"));
    LoggerManager::instance().log(LOG_PATH, Level::INFO, "[stop] 警报调度任务已停止");
}

// =====================================================================
// 默认值补齐
// =====================================================================
void AlarmDispatchTask::normalize(AlarmInfo& info) const
{
    if (info.record.alarmLevel == 0) {
        info.record.alarmLevel = alarmTypeToLevel(info.record.alarmType);
    }
    if (info.record.occurTime.isEmpty()) {
        info.record.occurTime = QDateTime::currentDateTime()
                             .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }
    if (info.alarmId.isEmpty()) {
        info.alarmId = info.generateAlarmId();
    }
}

// =====================================================================
// 提交：简化入参
// =====================================================================
QString AlarmDispatchTask::submitAlarm(int alarmType,
                                       int alarmSource,
                                       const QString& sourceIdentifier,
                                       const QString& description)
{
    AlarmInfo info;
    info.record.alarmType   = alarmType;
    info.alarmSource        = alarmSource;
    info.record.qrCode      = sourceIdentifier;
    info.record.description = description;
    info.record.alarmLevel  = alarmTypeToLevel(alarmType);
    return submitAlarm(info);
}

// =====================================================================
// 提交：完整 AlarmInfo
// =====================================================================
QString AlarmDispatchTask::submitAlarm(AlarmInfo info)
{
    normalize(info);

    // NoNeed 类型（如所有 SH85 自检报警）：直接落库，不参与活跃跟踪与去重
    const int resolvedStatus = alarmTypeToResolvedStatus(info.record.alarmType);
    if (resolvedStatus == static_cast<int>(AlarmResolvedStatus::NoNeed)) {
        info.record.isResolved  = resolvedStatus;
        info.record.resolveTime.clear();
        persistInsert(info);

        LoggerManager::instance().log(LOG_PATH, Level::INFO,
            QString("[submitAlarm] 提交警报(无需解决): 警报ID=%1, 类型=%2, 设备标识=%3, 描述=%4")
                .arg(info.alarmId).arg(info.record.alarmType)
                .arg(info.record.qrCode).arg(info.record.description).toStdString());

        // 记录运行日志：NoNeed 类型使用 Warn 级别
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            opTask->logWarn(info.record.description);
        }

        emit alarmPublished(info);
        return info.alarmId;
    }

    {
        QMutexLocker locker(&m_mutex);
        // 去重：同一 alarmId 已活跃则忽略
        if (m_active.contains(info.alarmId)) {
            LoggerManager::instance().log(LOG_PATH, Level::INFO,
                QString("[submitAlarm] 提交警报(重复，已忽略): 警报ID=%1").arg(info.alarmId).toStdString());
            return info.alarmId;
        }
        info.record.isResolved = 0;
        info.record.resolveTime.clear();
        m_active.insert(info.alarmId, info);
    }

    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[submitAlarm] 提交警报: 警报ID=%1, 类型=%2, 级别=%3, 设备标识=%4, 描述=%5")
            .arg(info.alarmId).arg(info.record.alarmType).arg(info.record.alarmLevel)
            .arg(info.record.qrCode).arg(info.record.description).toStdString());

    // 持久化：写 alarm_log（DBCon 内部 QueuedConnection 异步落盘）
    persistInsert(info);

    // 记录运行日志：根据告警级别调用不同日志方法
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        int alarmLevel = info.record.alarmLevel;
        if (alarmLevel == static_cast<int>(AlarmLevel::Error) ||
            alarmLevel == static_cast<int>(AlarmLevel::Fatal)) {
            opTask->logError(info.record.description);
        } else {
            opTask->logMessage(info.record.description);
        }
    }

    // 派发给订阅者（live log / 业务回调）
    emit alarmPublished(info);
    return info.alarmId;
}

// =====================================================================
// 解决：按 alarmId 字符串
// =====================================================================
void AlarmDispatchTask::submitResolve(const QString& alarmId)
{
    AlarmInfo resolvedInfo;
    {
        QMutexLocker locker(&m_mutex);
        auto it = m_active.find(alarmId);
        if (it == m_active.end()) {
            LoggerManager::instance().log(LOG_PATH, Level::WARN,
                QString("[submitResolve] 提交解决(非活跃): 警报ID=%1").arg(alarmId).toStdString());
            return;
        }
        resolvedInfo = it.value();
        resolvedInfo.record.isResolved   = 1;
        resolvedInfo.record.resolveTime = QDateTime::currentDateTime()
                                        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        m_active.erase(it);
    }

    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[submitResolve] 提交解决: 警报ID=%1, 类型=%2, 设备标识=%3, 解决时间=%4")
            .arg(resolvedInfo.alarmId).arg(resolvedInfo.record.alarmType)
            .arg(resolvedInfo.record.qrCode).arg(resolvedInfo.record.resolveTime).toStdString());

    persistResolve(resolvedInfo);

    // 记录运行日志：根据告警级别调用不同日志方法
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        int alarmLevel = resolvedInfo.record.alarmLevel;
        QString resolveMsg = QString("[AlarmResolved] %1").arg(resolvedInfo.record.description);
        if (alarmLevel == static_cast<int>(AlarmLevel::Error) ||
            alarmLevel == static_cast<int>(AlarmLevel::Fatal)) {
            opTask->logError(resolveMsg);
        } else {
            opTask->logMessage(resolveMsg);
        }
    }

    emit alarmResolved(resolvedInfo);
}

// =====================================================================
// 解决：按 type/source/identifier 还原 alarmId
// =====================================================================
void AlarmDispatchTask::submitResolve(int alarmType,
                                     int alarmSource,
                                     const QString& sourceIdentifier)
{
    AlarmInfo probe;
    probe.record.alarmType   = alarmType;
    probe.alarmSource        = alarmSource;
    probe.record.qrCode      = sourceIdentifier;
    probe.record.alarmLevel  = alarmTypeToLevel(alarmType);
    submitResolve(probe.generateAlarmId());
}

// =====================================================================
// 查询接口
// =====================================================================
bool AlarmDispatchTask::isActive(const QString& alarmId) const
{
    QMutexLocker locker(&m_mutex);
    return m_active.contains(alarmId);
}

int AlarmDispatchTask::activeCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_active.size();
}

QList<AlarmInfo> AlarmDispatchTask::activeAlarms() const
{
    QMutexLocker locker(&m_mutex);
    return m_active.values();
}

void AlarmDispatchTask::clearActive()
{
    QMutexLocker locker(&m_mutex);
    m_active.clear();
}

// =====================================================================
// 启动加载：从 alarm_log 恢复 is_resolved=0 的警报到 m_active
// =====================================================================
void AlarmDispatchTask::loadActiveFromDb()
{
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        LoggerManager::instance().log(LOG_PATH, Level::WARN,
            "[loadActiveFromDb] 警报日志数据库不可用，跳过恢复");
        return;
    }

    // 全量取未解决记录（alarmLevel=-1 / qrCode="" / alarmType="" 表示不限）
    constexpr int kPageSize = 10000;
    const QList<AlarmRecord> rows = db->queryPageWithConditions(
        /*alarmLevel*/ -1,
        /*qrCode*/ QString(),
        /*alarmType*/ QString(),
        /*isResolved*/ 0,
        /*startTime*/ QString(),
        /*endTime*/ QString(),
        /*pageSize*/ kPageSize,
        /*pageNumber*/ 1);

    QMutexLocker locker(&m_mutex);
    int restored = 0;
    for (const AlarmRecord& row : rows) {
        AlarmInfo info;
        info.record.id              = row.id;
        info.record.alarmLevel      = row.alarmLevel;
        info.record.occurTime       = row.occurTime;
        info.record.qrCode          = row.qrCode;
        info.record.alarmType       = row.alarmType;
        info.record.isResolved      = 0;
        info.record.resolveTime     = row.resolveTime;
        info.record.description     = row.description;
        info.record.userPermission  = row.userPermission;
        // alarm_source 未落库；generateAlarmId() 仅需 level/identifier/type，
        // 默认 Device 不影响 alarmId 与后续去重
        info.alarmSource      = static_cast<int>(AlarmSource::Device);
        info.alarmId          = info.generateAlarmId();

        if (!m_active.contains(info.alarmId)) {
            m_active.insert(info.alarmId, info);
            ++restored;

            // 发出信号让 ScrollingTipLabel 等订阅者显示恢复的警报
            // 必须在锁外 emit，避免死锁（emit 可能触发回调访问 m_active）
            locker.unlock();
            emit alarmPublished(info);
            // 记录运行日志：警报恢复
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                opTask->logError(info.record.description);
            }
            locker.relock();
        }
    }
    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[loadActiveFromDb] 从数据库恢复 %1 条未解决警报").arg(restored).toStdString());
}

// =====================================================================
// 持久化：INSERT
// =====================================================================
void AlarmDispatchTask::persistInsert(const AlarmInfo& info)
{
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        LoggerManager::instance().log(LOG_PATH, Level::ERROR,
            QString("[persistInsert] 警报日志数据库不可用，丢弃警报: %1").arg(info.alarmId).toStdString());
        return;
    }

    db->insertRecord(
        info.record.alarmLevel,
        info.record.occurTime,
        info.record.qrCode,                          // 复用 qr_code 列存设备来源
        QString::number(info.record.alarmType),      // alarm_type 列为 TEXT
        info.record.isResolved,
        info.record.resolveTime,
        info.record.description,
        info.record.userPermission);

    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[persistInsert] 持久化插入: 警报ID=%1, 级别=%2, 类型=%3").arg(info.alarmId)
            .arg(info.record.alarmLevel).arg(info.record.alarmType).toStdString());

    // 发出插入完成信号，供 UI 接收显示
    emit alarmLogInserted(info.record);
}

// =====================================================================
// 持久化：UPDATE（按 alarm_type 把同类型未解决行原位标为已解决）
// =====================================================================
void AlarmDispatchTask::persistResolve(const AlarmInfo& info)
{
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        LoggerManager::instance().log(LOG_PATH, Level::ERROR,
            QString("[persistResolve] 警报日志数据库不可用，跳过解决: %1").arg(info.alarmId).toStdString());
        return;
    }
    // updateResolve 按 (qr_code, alarm_type) 联合定位单条
    db->updateResolve(info.record.qrCode,
                      QString::number(info.record.alarmType),
                      info.record.resolveTime);

    LoggerManager::instance().log(LOG_PATH, Level::INFO,
        QString("[persistResolve] 持久化更新: 警报ID=%1, 设备标识=%2, 类型=%3, 解决时间=%4")
            .arg(info.alarmId).arg(info.record.qrCode)
            .arg(info.record.alarmType).arg(info.record.resolveTime).toStdString());
}

// =====================================================================
// DB 写入完成回调：查询完整记录并 emit alarmResolvePersisted
// =====================================================================
void AlarmDispatchTask::onAlarmDBRecordResolved(const QString& qrCode, const QString& alarmType, const QString& resolveTime)
{
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) return;

    // 查询该 (qrCode, alarmType, isResolved=1, resolveTime) 的完整记录
    const QList<AlarmRecord> records = db->queryPageWithConditions(
        /*alarmLevel*/ -1,
        /*qrCode*/ qrCode,
        /*alarmType*/ alarmType,
        /*isResolved*/ 1,
        /*startTime*/ QString(),
        /*endTime*/ QString(),
        /*pageSize*/ 1,
        /*pageNumber*/ 1);

    if (records.isEmpty()) {
        LoggerManager::instance().log(LOG_PATH, Level::WARN,
            QString("[onAlarmDBRecordResolved] 数据库写入后未找到解决记录: 设备标识=%1, 类型=%2, 解决时间=%3")
                .arg(qrCode).arg(alarmType).arg(resolveTime).toStdString());
        return;
    }

    // 取最新的一条（按 resolveTime 匹配）
    for (const AlarmRecord& rec : records) {
        if (rec.resolveTime == resolveTime) {
            emit alarmResolvePersisted(rec);
            return;
        }
    }
    LoggerManager::instance().log(LOG_PATH, Level::WARN,
        QString("[onAlarmDBRecordResolved] 未找到匹配解决时间的记录: 设备标识=%1, 类型=%2, 解决时间=%3")
            .arg(qrCode).arg(alarmType).arg(resolveTime).toStdString());
}
