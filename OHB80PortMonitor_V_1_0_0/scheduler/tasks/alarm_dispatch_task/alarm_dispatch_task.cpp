#include "alarm_dispatch_task.h"

#include <QDateTime>
#include "logdatabases/databasemanager.h"
#include "logdatabases/alarmlogdb/alarmlogdbcon.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "app/shareddata.h"
#include "classes/foupofohbinfo.h"
#include "loggerconfig.h"
#include "defer/defer.h"

namespace {

QString alarmLevelDisplayName(int level)
{
    switch (level) {
        case static_cast<int>(AlarmLevel::Warn):
            return QStringLiteral("警告/Warn");
        case static_cast<int>(AlarmLevel::Error):
            return QStringLiteral("错误/Error");
        case static_cast<int>(AlarmLevel::Fatal):
            return QStringLiteral("致命错误/Fatal");
        default:
            return alarmLevelName(level);
    }
}

QString alarmLevelDisplayText(int level)
{
    return QStringLiteral("%1(%2)").arg(level).arg(alarmLevelDisplayName(level));
}

QString alarmTypeDisplayText(int type)
{
    return QStringLiteral("%1(%2)").arg(type).arg(alarmTypeName(type));
}

bool isBlockedForFoup(const AlarmInfo& info, const QSet<QString>& blockedAlarmTypes)
{
    return blockedAlarmTypes.contains(alarmTypeName(info.record.alarmType));
}

QString resolveDescriptionFor(const AlarmInfo& info)
{
    if (info.record.alarmType == static_cast<int>(AlarmType::HumidityNotReached)) {
        return QStringLiteral("[qrcode: %1] 湿度成功达到标准（30min充氮后湿度达标）. Alarm Code:5101")
            .arg(info.record.qrCode.trimmed());
    }

    return info.record.description;
}

void applyResolveDescription(AlarmInfo& info)
{
    info.record.description = resolveDescriptionFor(info);
}

} // namespace

AlarmDispatchTask::AlarmDispatchTask(QObject* parent)
    : SchedulerTask(parent)
{
    // 初始化日志接口类
    initAlarmDispatchTaskLogger();
    // 初始化汇总日志计时器
    initSummaryTimer();

    m_foupAlarmSyncTimer = new QTimer(this);
    m_foupAlarmSyncTimer->setInterval(kFoupAlarmSyncIntervalMs);
    connect(m_foupAlarmSyncTimer, &QTimer::timeout,
            this, &AlarmDispatchTask::syncAllFoupAlarmStates,
            Qt::QueuedConnection);

    m_logger->summaryLogger().info("[AlarmDispatchTask] 警报调度任务已构造");
}

AlarmDispatchTask::~AlarmDispatchTask()
{
    if (m_summaryTimer) {
        m_summaryTimer->stop();
        delete m_summaryTimer;
        m_summaryTimer = nullptr;
    }
    if (m_logger) {
        m_logger->summaryLogger().info("[AlarmDispatchTask] 警报调度任务已销毁");
    }
    delete m_logger;
}

void AlarmDispatchTask::initAlarmDispatchTaskLogger()
{
    const bool summary = LoggerConfig::getInstance()->isAlarmDispatchTaskSummaryEnabled();
    const bool devices = LoggerConfig::getInstance()->isAlarmDispatchTaskDevicesEnabled();
    m_logger = new AlarmDispatchTaskLogger(summary, devices);
}

void AlarmDispatchTask::start()
{
    setState(Running);
    loadActiveFromDb();
    startFoupAlarmSyncTimer();

    // 连接 DB 写入完成信号，用于在解决落库后 emit alarmResolvePersisted
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (db) {
        connect(db, &LogDB::AlarmLogDBCon::recordResolved,
                this, &AlarmDispatchTask::onAlarmDBRecordResolved,
                Qt::QueuedConnection);
        connect(db, &LogDB::AlarmLogDBCon::recordInserted,
                this, &AlarmDispatchTask::onAlarmDBRecordInserted,
                Qt::QueuedConnection);
    }

    m_logger->summaryLogger().info(QString("[start] 警报调度任务已启动，活跃警报数=%1").arg(activeCount()).toStdString());
    emit progress(0, QStringLiteral("Alarm dispatcher running, active=%1").arg(activeCount()));
}

void AlarmDispatchTask::stop()
{
    stopFoupAlarmSyncTimer();
    clearActive();
    setState(Cancelled);
    emit finished(false, QStringLiteral("Alarm dispatcher stopped"));
    m_logger->summaryLogger().info("[stop] 警报调度任务已停止");
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
QString AlarmDispatchTask::selectedActiveAlarmIdForQrCode(const QString& qrCode) const
{
    return selectedActiveAlarmIdForQrCode(qrCode, AlarmConfig::getInstance().readBlockedAlarms());
}

QString AlarmDispatchTask::selectedActiveAlarmIdForQrCode(const QString& qrCode,
                                                          const QSet<QString>& blockedAlarmTypes) const
{
    const QString normalizedQrCode = qrCode.trimmed();
    if (normalizedQrCode.isEmpty()) {
        return QString();
    }

    QMutexLocker locker(&m_mutex);
    QString selectedAlarmId;
    int selectedLevel = -1;

    // 同一设备可能同时存在离线、VEFC 异常、湿度未达标等多个告警。
    // foup->alarmId 只能展示一个，因此选择 alarmLevel 最高的一条作为当前 UI 主告警。
    for (auto it = m_active.constBegin(); it != m_active.constEnd(); ++it) {
        const AlarmInfo& info = it.value();
        if (info.record.qrCode.trimmed() != normalizedQrCode) {
            continue;
        }
        if (isBlockedForFoup(info, blockedAlarmTypes)) {
            continue;
        }

        if (info.record.alarmLevel > selectedLevel) {
            selectedLevel = info.record.alarmLevel;
            selectedAlarmId = info.alarmId;
        }
    }

    return selectedAlarmId;
}

void AlarmDispatchTask::writeSummarySnapshot()
{
    QHash<int, QSet<QString>> groups;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_active.constBegin(); it != m_active.constEnd(); ++it) {
            const AlarmInfo& info = it.value();
            const QString qr = info.record.qrCode.trimmed();
            if (!qr.isEmpty()) {
                groups[info.record.alarmType].insert(qr);
            }
        }
    }

    if (groups.isEmpty()) {
        return;
    }

    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    QStringList lines;
    lines << QStringLiteral("[%1] 当前未恢复错误汇总").arg(ts);

    QList<int> types = groups.keys();
    std::sort(types.begin(), types.end());
    for (int type : types) {
        const QString typeName = alarmTypeName(type);
        QStringList qrs = QStringList(groups.value(type).values());
        std::sort(qrs.begin(), qrs.end(), [](const QString& a, const QString& b){
            bool ok1=false, ok2=false; int ia=a.toInt(&ok1); int ib=b.toInt(&ok2);
            if (ok1 && ok2) {
                return ia < ib;
            }
            return a < b;
        });
        const int count = qrs.size();
        const QString display = qrs.join(QStringLiteral("，"));
        lines << QStringLiteral("%1（%2个）：%3").arg(typeName).arg(count).arg(display);
    }

    const QString block = lines.join('\n');
    m_logger->summaryLogger().info(block.toStdString());
    m_logger->summaryLogger().flush();
}

void AlarmDispatchTask::initSummaryTimer()
{
    m_summaryTimer = new QTimer(this);
    m_summaryTimer->setObjectName("AlarmSummaryTimer");
    connect(m_summaryTimer, &QTimer::timeout, this, &AlarmDispatchTask::writeSummarySnapshot, Qt::QueuedConnection);
    m_logger->summaryLogger().info("汇总计时器AlarmSummaryTimer开启。。。");

    startSummaryTimer();
}

void AlarmDispatchTask::startSummaryTimer()
{
    if (!m_summaryTimer || m_summaryTimer->isActive()) {
        return;
    }
    const int periodMs = LoggerConfig::getInstance()->getAlarmDispatchTaskSummaryPeriodMs();
    m_summaryTimer->setInterval(periodMs);
    m_summaryTimer->start();
    m_logger->summaryLogger().info(QString("[startSummaryTimer] Summary timer started, interval=%1 ms").arg(periodMs).toStdString());
}

void AlarmDispatchTask::stopSummaryTimer()
{
    if (!m_summaryTimer || !m_summaryTimer->isActive()) {
        return;
    }
    m_summaryTimer->stop();
    m_logger->summaryLogger().info("[stopSummaryTimer] Summary timer stopped");
}

void AlarmDispatchTask::startFoupAlarmSyncTimer()
{
    if (!m_foupAlarmSyncTimer || m_foupAlarmSyncTimer->isActive()) {
        return;
    }

    // 使用定时器异步刷新 FOUP 告警状态，避免提交/恢复告警时直接改 UI 数据。
    m_foupAlarmSyncTimer->start();
    m_logger->summaryLogger().info(QString("[startFoupAlarmSyncTimer] FOUP 告警状态刷新定时器已启动，interval=%1 ms")
        .arg(kFoupAlarmSyncIntervalMs).toStdString());
}

void AlarmDispatchTask::stopFoupAlarmSyncTimer()
{
    if (!m_foupAlarmSyncTimer || !m_foupAlarmSyncTimer->isActive()) {
        return;
    }

    m_foupAlarmSyncTimer->stop();
    m_logger->summaryLogger().info("[stopFoupAlarmSyncTimer] FOUP 告警状态刷新定时器已停止");
}

void AlarmDispatchTask::syncFoupAlarmState(const QString& qrCode)
{
    syncFoupAlarmState(qrCode, AlarmConfig::getInstance().readBlockedAlarms());
}

void AlarmDispatchTask::syncFoupAlarmState(const QString& qrCode,
                                           const QSet<QString>& blockedAlarmTypes)
{
    const QString normalizedQrCode = qrCode.trimmed();
    if (normalizedQrCode.isEmpty()) {
        return;
    }

    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(normalizedQrCode);
    if (!foup) {
        return;
    }

    // 没有未屏蔽的 active 告警时，alarmId 为空，UI 对应设备恢复为非告警状态。
    const QString alarmId = selectedActiveAlarmIdForQrCode(normalizedQrCode, blockedAlarmTypes);
    foup->setHasAlarm(!alarmId.isEmpty());
    foup->setAlarmId(alarmId);
}

void AlarmDispatchTask::syncAllFoupAlarmStates()
{
    // 全量同步用于启动恢复和批量清空，避免部分设备保留旧的 UI 告警状态。
    const QSet<QString> blockedAlarmTypes = AlarmConfig::getInstance().readBlockedAlarms();
    const QStringList qrCodes = SharedData::getAllQrcodes();
    QSet<QString> currentQrCodes;
    for (const QString& qrCode : qrCodes) {
        const QString normalizedQrCode = qrCode.trimmed();
        if (normalizedQrCode.isEmpty()) {
            continue;
        }
        currentQrCodes.insert(normalizedQrCode);
        syncFoupAlarmState(normalizedQrCode, blockedAlarmTypes);
    }
    resolveMissingQRCodeActiveAlarms(currentQrCodes);
}

void AlarmDispatchTask::resolveMissingQRCodeActiveAlarms(const QSet<QString>& currentQrCodes)
{
    QSet<QString> missingQrCodes;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_active.constBegin(); it != m_active.constEnd(); ++it) {
            const AlarmInfo& info = it.value();
            if (info.alarmSource != static_cast<int>(AlarmSource::Device)) {
                continue;
            }

            const QString qrCode = info.record.qrCode.trimmed();
            if (!qrCode.isEmpty() && !currentQrCodes.contains(qrCode)) {
                missingQrCodes.insert(qrCode);
            }
        }
    }

    if (missingQrCodes.isEmpty()) {
        return;
    }

    QStringList sortedQrCodes = missingQrCodes.values();
    std::sort(sortedQrCodes.begin(), sortedQrCodes.end(), [](const QString& lhs, const QString& rhs) {
        bool lhsOk = false;
        bool rhsOk = false;
        const int lhsValue = lhs.toInt(&lhsOk);
        const int rhsValue = rhs.toInt(&rhsOk);
        if (lhsOk && rhsOk) {
            return lhsValue < rhsValue;
        }
        return lhs < rhs;
    });

    for (const QString& qrCode : qAsConst(sortedQrCodes)) {
        const int resolvedCount = submitResolveAllByQRCode(qrCode);
        if (resolvedCount > 0) {
            m_logger->summaryLogger().info(
                QString("[resolveMissingQRCodeActiveAlarms] Resolved %1 active alarms for missing QRCode %2")
                    .arg(resolvedCount)
                    .arg(qrCode)
                    .toStdString());
        }
    }
}

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

        const QString message = QString("[submitAlarm] 提交警报(无需解决)\n"
                                        "警报ID: %1\n"
                                        "告警类型: %2\n"
                                        "警报级别: %3\n"
                                        "设备标识: %4\n"
                                        "描述: %5")
            .arg(info.alarmId)
            .arg(alarmTypeDisplayText(info.record.alarmType))
            .arg(alarmLevelDisplayText(info.record.alarmLevel))
            .arg(info.record.qrCode)
            .arg(info.record.description);
        // m_logger->summaryLogger().info(message.toStdString());
        m_logger->deviceLogger(info.record.qrCode).info(message.toStdString());

        // 记录运行日志：NoNeed 类型使用 Warn 级别
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            opTask->logWarn(info.record.description);
        }

        emit alarmPublished(info);
        return info.alarmId;
    }

    bool duplicated = false;
    {
        QMutexLocker locker(&m_mutex);
        // m_active 是当前未恢复告警的唯一事实来源；相同 alarmId 重复上报直接忽略。
        if (m_active.contains(info.alarmId)) {
            duplicated = true;
        } else {
            info.record.isResolved = 0;
            info.record.resolveTime.clear();
            m_active.insert(info.alarmId, info);
        }
    }

    if (duplicated) {
        // 周期任务会持续上报同一 active 告警；重复上报属于稳定态，不写日志也不落库。
        return info.alarmId;
    }

    const QString submitMessage = QString("[submitAlarm] 提交警报\n"
                                          "警报ID: %1\n"
                                          "告警类型: %2\n"
                                          "警报级别: %3\n"
                                          "设备标识: %4\n"
                                          "描述: %5")
        .arg(info.alarmId)
        .arg(alarmTypeDisplayText(info.record.alarmType))
        .arg(alarmLevelDisplayText(info.record.alarmLevel))
        .arg(info.record.qrCode)
        .arg(info.record.description);
    // m_logger->summaryLogger().warn(submitMessage.toStdString());
    m_logger->deviceLogger(info.record.qrCode).warn(submitMessage.toStdString());

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
            // 恢复请求具备幂等性：如果当前没有对应 active 告警，说明该告警未发生或已经恢复。
            // 这种请求不写日志、不落库、不发信号，避免无效恢复日志淹没有价值的告警记录。
            return;
        }

        resolvedInfo = it.value();
        resolvedInfo.record.isResolved   = 1;
        resolvedInfo.record.resolveTime = QDateTime::currentDateTime()
                                        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        applyResolveDescription(resolvedInfo);
        // 只有成功从 active 集合移除，才说明本次恢复请求真正生效。
        m_active.erase(it);
    }

    const QString resolveMessage = QString("[submitResolve] 提交解决\n"
                                           "警报ID: %1\n"
                                           "告警类型: %2\n"
                                           "警报级别: %3\n"
                                           "设备标识: %4\n"
                                           "解决时间: %5\n"
                                           "描述: %6")
        .arg(resolvedInfo.alarmId)
        .arg(alarmTypeDisplayText(resolvedInfo.record.alarmType))
        .arg(alarmLevelDisplayText(resolvedInfo.record.alarmLevel))
        .arg(resolvedInfo.record.qrCode)
        .arg(resolvedInfo.record.resolveTime)
        .arg(resolvedInfo.record.description);
    // m_logger->summaryLogger().info(resolveMessage.toStdString());
    m_logger->deviceLogger(resolvedInfo.record.qrCode).info(resolveMessage.toStdString());

    persistResolve(resolvedInfo);

    // 记录运行日志：根据告警级别调用不同日志方法
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        // int alarmLevel = resolvedInfo.record.alarmLevel;
        QString resolveMsg = QString("[AlarmResolved] %1").arg(resolvedInfo.record.description);
        opTask->logMessage(resolveMsg);
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
int AlarmDispatchTask::submitResolveAllBySourceIdentifier(int alarmSource,
                                                          const QString& sourceIdentifier)
{
    const QString normalizedIdentifier = sourceIdentifier.trimmed();
    if (normalizedIdentifier.isEmpty()) {
        return 0;
    }

    QList<AlarmInfo> resolvedInfos;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_active.begin(); it != m_active.end(); ) {
            AlarmInfo info = it.value();
            if (info.alarmSource == alarmSource
                && info.record.qrCode.trimmed() == normalizedIdentifier) {
                info.record.isResolved = 1;
                info.record.resolveTime = QDateTime::currentDateTime()
                    .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                applyResolveDescription(info);
                resolvedInfos.append(info);
                it = m_active.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const AlarmInfo& resolvedInfo : qAsConst(resolvedInfos)) {
        const QString resolveMessage = QString("[submitResolveAllBySourceIdentifier] Resolve submitted\n"
                                               "Alarm ID: %1\n"
                                               "Alarm type: %2\n"
                                               "Alarm level: %3\n"
                                               "Source identifier: %4\n"
                                               "Resolve time: %5\n"
                                               "Description: %6")
            .arg(resolvedInfo.alarmId)
            .arg(alarmTypeDisplayText(resolvedInfo.record.alarmType))
            .arg(alarmLevelDisplayText(resolvedInfo.record.alarmLevel))
            .arg(resolvedInfo.record.qrCode)
            .arg(resolvedInfo.record.resolveTime)
            .arg(resolvedInfo.record.description);
        m_logger->deviceLogger(resolvedInfo.record.qrCode).info(resolveMessage.toStdString());

        persistResolve(resolvedInfo);

        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            const QString resolveMsg = QString("[AlarmResolved] %1").arg(resolvedInfo.record.description);
            opTask->logMessage(resolveMsg);
        }
        emit alarmResolved(resolvedInfo);
    }

    return resolvedInfos.count();
}

int AlarmDispatchTask::submitResolveAllByQRCode(const QString& qrCode)
{
    const QString normalizedQRCode = qrCode.trimmed();
    if (normalizedQRCode.isEmpty()) {
        return 0;
    }

    rememberRecentQRCodeResolve(normalizedQRCode);

    QList<AlarmInfo> resolvedInfos;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_active.begin(); it != m_active.end(); ) {
            AlarmInfo info = it.value();
            if (info.record.qrCode.trimmed() == normalizedQRCode) {
                info.record.isResolved = 1;
                info.record.resolveTime = QDateTime::currentDateTime()
                    .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                applyResolveDescription(info);
                resolvedInfos.append(info);
                it = m_active.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const AlarmInfo& resolvedInfo : qAsConst(resolvedInfos)) {
        const QString resolveMessage = QString("[submitResolveAllByQRCode] Resolve submitted\n"
                                               "Alarm ID: %1\n"
                                               "Alarm type: %2\n"
                                               "Alarm level: %3\n"
                                               "QRCode: %4\n"
                                               "Resolve time: %5\n"
                                               "Description: %6")
            .arg(resolvedInfo.alarmId)
            .arg(alarmTypeDisplayText(resolvedInfo.record.alarmType))
            .arg(alarmLevelDisplayText(resolvedInfo.record.alarmLevel))
            .arg(resolvedInfo.record.qrCode)
            .arg(resolvedInfo.record.resolveTime)
            .arg(resolvedInfo.record.description);
        m_logger->deviceLogger(resolvedInfo.record.qrCode).info(resolveMessage.toStdString());

        persistResolve(resolvedInfo);

        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            const QString resolveMsg = QString("[AlarmResolved] %1").arg(resolvedInfo.record.description);
            opTask->logMessage(resolveMsg);
        }
        emit alarmResolved(resolvedInfo);
    }

    const int dbResolvedCount = resolveUnresolvedDbRowsByQRCode(normalizedQRCode);
    scheduleResolveUnresolvedDbRowsByQRCode(normalizedQRCode);

    return resolvedInfos.count() + dbResolvedCount;
}

void AlarmDispatchTask::rememberRecentQRCodeResolve(const QString& qrCode)
{
    const QString normalizedQRCode = qrCode.trimmed();
    if (normalizedQRCode.isEmpty()) {
        return;
    }

    constexpr qint64 kRecentResolveGraceMs = 30000;
    const qint64 expireAt = QDateTime::currentMSecsSinceEpoch() + kRecentResolveGraceMs;
    QMutexLocker locker(&m_mutex);
    m_recentQRCodeResolveUntilMs.insert(normalizedQRCode, expireAt);
}

bool AlarmDispatchTask::shouldAutoResolveInsertedRecord(const AlarmRecord& record)
{
    if (record.isResolved != static_cast<int>(AlarmResolvedStatus::Unresolved)) {
        return false;
    }

    const QString qrCode = record.qrCode.trimmed();
    if (qrCode.isEmpty()) {
        return false;
    }

    bool numericQRCode = false;
    qrCode.toInt(&numericQRCode);
    if (!numericQRCode) {
        return false;
    }

    if (!SharedData::getAllQrcodes().contains(qrCode)) {
        return true;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QMutexLocker locker(&m_mutex);
    auto it = m_recentQRCodeResolveUntilMs.find(qrCode);
    if (it == m_recentQRCodeResolveUntilMs.end()) {
        return false;
    }
    if (it.value() < now) {
        m_recentQRCodeResolveUntilMs.erase(it);
        return false;
    }
    return true;
}

int AlarmDispatchTask::resolveUnresolvedDbRowsByQRCode(const QString& qrCode)
{
    const QString normalizedQRCode = qrCode.trimmed();
    if (normalizedQRCode.isEmpty()) {
        return 0;
    }

    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        return 0;
    }

    constexpr int kPageSize = 10000;
    const QList<AlarmRecord> rows = db->queryPageWithConditions(
        /*alarmLevel*/ -1,
        /*qrCode*/ normalizedQRCode,
        /*alarmType*/ QString(),
        /*isResolved*/ static_cast<int>(AlarmResolvedStatus::Unresolved),
        /*startTime*/ QString(),
        /*endTime*/ QString(),
        /*pageSize*/ kPageSize,
        /*pageNumber*/ 1);

    QSet<int> alarmTypes;
    for (const AlarmRecord& row : rows) {
        if (row.qrCode.trimmed() == normalizedQRCode
            && row.isResolved == static_cast<int>(AlarmResolvedStatus::Unresolved)) {
            alarmTypes.insert(row.alarmType);
        }
    }

    if (alarmTypes.isEmpty()) {
        return 0;
    }

    const QString resolveTime = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    for (int alarmType : qAsConst(alarmTypes)) {
        db->updateResolve(normalizedQRCode, QString::number(alarmType), resolveTime);
    }

    const QString message = QString("[resolveUnresolvedDbRowsByQRCode] Resolved DB rows for QRCode %1, alarmTypeCount=%2")
        .arg(normalizedQRCode)
        .arg(alarmTypes.count());
    m_logger->deviceLogger(normalizedQRCode).info(message.toStdString());
    return alarmTypes.count();
}

void AlarmDispatchTask::scheduleResolveUnresolvedDbRowsByQRCode(const QString& qrCode)
{
    const QString normalizedQRCode = qrCode.trimmed();
    if (normalizedQRCode.isEmpty()) {
        return;
    }

    const QList<int> retryDelaysMs = { 200, 1000, 3000 };
    for (int delayMs : retryDelaysMs) {
        QTimer::singleShot(delayMs, this, [this, normalizedQRCode]() {
            resolveUnresolvedDbRowsByQRCode(normalizedQRCode);
        });
    }
}

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
        m_logger->summaryLogger().warn("[loadActiveFromDb] 警报日志数据库不可用，跳过恢复");
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
            const QString restoreMessage = QString("[loadActiveFromDb] 恢复未解决警报\n"
                                                   "警报ID: %1\n"
                                                   "告警类型: %2\n"
                                                   "警报级别: %3\n"
                                                   "设备标识: %4\n"
                                                   "描述: %5")
                .arg(info.alarmId)
                .arg(alarmTypeDisplayText(info.record.alarmType))
                .arg(alarmLevelDisplayText(info.record.alarmLevel))
                .arg(info.record.qrCode)
                .arg(info.record.description);
            m_logger->deviceLogger(info.record.qrCode).warn(restoreMessage.toStdString());
            emit alarmPublished(info);
            // 记录运行日志：警报恢复
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                opTask->logError(info.record.description);
            }
            locker.relock();
        }
    }
    m_logger->summaryLogger().info(QString("[loadActiveFromDb] 从数据库恢复 %1 条未解决警报").arg(restored).toStdString());
}

// =====================================================================
// 持久化：INSERT
// =====================================================================
void AlarmDispatchTask::persistInsert(const AlarmInfo& info)
{
    Tool::Defer defer([this]() {
        m_logger->summaryLogger().flush();
    });    
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        const QString message = QString("[persistInsert] 警报日志数据库不可用，丢弃警报\n"
                                        "警报ID: %1\n"
                                        "设备标识: %2\n"
                                        "告警类型: %3")
            .arg(info.alarmId)
            .arg(info.record.qrCode)
            .arg(alarmTypeDisplayText(info.record.alarmType));
        // m_logger->summaryLogger().error(message.toStdString());
        m_logger->deviceLogger(info.record.qrCode).error(message.toStdString());
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

    const QString insertMessage = QString("[persistInsert] 持久化插入\n"
                                          "警报ID: %1\n"
                                          "告警类型: %2\n"
                                          "警报级别: %3\n"
                                          "设备标识: %4\n"
                                          "发生时间: %5\n"
                                          "描述: %6")
        .arg(info.alarmId)
        .arg(alarmTypeDisplayText(info.record.alarmType))
        .arg(alarmLevelDisplayText(info.record.alarmLevel))
        .arg(info.record.qrCode)
        .arg(info.record.occurTime)
        .arg(info.record.description);
    // m_logger->summaryLogger().info(insertMessage.toStdString());
    m_logger->deviceLogger(info.record.qrCode).info(insertMessage.toStdString());

    // 发出插入完成信号，供 UI 接收显示
    emit alarmLogInserted(info.record);
}

// =====================================================================
// 持久化：UPDATE（按 alarm_type 把同类型未解决行原位标为已解决）
// =====================================================================
void AlarmDispatchTask::persistResolve(const AlarmInfo& info)
{
    Tool::Defer defer([this]() {
        m_logger->summaryLogger().flush();
    });    
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        const QString message = QString("[persistResolve] 警报日志数据库不可用，跳过解决\n"
                                        "警报ID: %1\n"
                                        "设备标识: %2\n"
                                        "告警类型: %3")
            .arg(info.alarmId)
            .arg(info.record.qrCode)
            .arg(alarmTypeDisplayText(info.record.alarmType));
        // m_logger->summaryLogger().error(message.toStdString());
        m_logger->deviceLogger(info.record.qrCode).error(message.toStdString());
        return;
    }
    // updateResolve 按 (qr_code, alarm_type) 联合定位单条
    db->updateResolve(info.record.qrCode,
                      QString::number(info.record.alarmType),
                      info.record.resolveTime);

    const QString persistResolveMessage = QString("[persistResolve] 持久化更新\n"
                                                  "警报ID: %1\n"
                                                  "告警类型: %2\n"
                                                  "警报级别: %3\n"
                                                  "设备标识: %4\n"
                                                  "解决时间: %5\n"
                                                  "描述: %6")
        .arg(info.alarmId)
        .arg(alarmTypeDisplayText(info.record.alarmType))
        .arg(alarmLevelDisplayText(info.record.alarmLevel))
        .arg(info.record.qrCode)
        .arg(info.record.resolveTime)
        .arg(info.record.description);
    // m_logger->summaryLogger().info(persistResolveMessage.toStdString());
    m_logger->deviceLogger(info.record.qrCode).info(persistResolveMessage.toStdString());
}

// =====================================================================
// DB 写入完成回调：查询完整记录并 emit alarmResolvePersisted
// =====================================================================
void AlarmDispatchTask::onAlarmDBRecordResolved(const QString& qrCode, const QString& alarmType, const QString& resolveTime)
{
    Tool::Defer defer([this]() {
        m_logger->summaryLogger().flush();
    });
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
        const QString message = QString("[onAlarmDBRecordResolved] 数据库写入后未找到解决记录: 设备标识=%1, 类型=%2, 解决时间=%3")
            .arg(qrCode)
            .arg(alarmTypeDisplayText(alarmType.toInt()))
            .arg(resolveTime);
        // m_logger->summaryLogger().warn(message.toStdString());
        m_logger->deviceLogger(qrCode).warn(message.toStdString());
        return;
    }

    // 取最新的一条（按 resolveTime 匹配）
    for (const AlarmRecord& rec : records) {
        if (rec.resolveTime == resolveTime) {
            emit alarmResolvePersisted(rec);
            return;
        }
    }
    const QString message = QString("[onAlarmDBRecordResolved] 未找到匹配解决时间的记录: 设备标识=%1, 类型=%2, 解决时间=%3")
        .arg(qrCode)
        .arg(alarmTypeDisplayText(alarmType.toInt()))
        .arg(resolveTime);
    // m_logger->summaryLogger().warn(message.toStdString());
    m_logger->deviceLogger(qrCode).warn(message.toStdString());
}

void AlarmDispatchTask::onAlarmDBRecordInserted(const AlarmRecord& record)
{
    if (!shouldAutoResolveInsertedRecord(record)) {
        return;
    }

    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        return;
    }

    const QString qrCode = record.qrCode.trimmed();
    const QString resolveTime = QDateTime::currentDateTime()
        .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    db->updateResolve(qrCode, QString::number(record.alarmType), resolveTime);

    const QString message = QString("[onAlarmDBRecordInserted] Auto-resolved late inserted alarm for QRCode %1, alarmType=%2")
        .arg(qrCode)
        .arg(alarmTypeDisplayText(record.alarmType));
    m_logger->deviceLogger(qrCode).info(message.toStdString());
}
