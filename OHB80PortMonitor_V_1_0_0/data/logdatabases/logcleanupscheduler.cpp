#include "logcleanupscheduler.h"
#include "loggermanager.h"
#include <QTimer>
#include <QDebug>

namespace LogDB {

LogCleanupScheduler::LogCleanupScheduler(const Config& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_timer(nullptr)
{
}

LogCleanupScheduler::~LogCleanupScheduler()
{
    stop();
}

void LogCleanupScheduler::setMonthRangeProvider(MonthRangeProvider provider)
{
    m_monthRangeProvider = std::move(provider);
}

void LogCleanupScheduler::setDeleteByRangeFn(DeleteByRangeFn fn)
{
    m_deleteByRangeFn = std::move(fn);
}

void LogCleanupScheduler::start()
{
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &LogCleanupScheduler::onTimerTick);
    }
    if (!m_timer->isActive()) {
        m_timer->start(m_config.checkIntervalMs);
    }
}

void LogCleanupScheduler::stop()
{
    if (m_timer && m_timer->isActive()) {
        m_timer->stop();
    }
}

void LogCleanupScheduler::triggerCleanupCheck()
{
    performCleanupCheck();
}

void LogCleanupScheduler::onTimerTick()
{
    const std::string& logPath = m_config.logPath;

    LoggerManager::instance().log(logPath, Level::INFO,
        QString("定期检测: 执行清理检查").toStdString());
    performCleanupCheck();
    LoggerManager::instance().flush(logPath);
}

void LogCleanupScheduler::performCleanupCheck()
{
    const std::string& logPath = m_config.logPath;

    if (!m_monthRangeProvider || !m_deleteByRangeFn) {
        LoggerManager::instance().log(logPath, Level::WARN,
            "Provider or delete function not set");
        LoggerManager::instance().flush(logPath);
        return;
    }

    QVariantMap monthRange = m_monthRangeProvider();
    QString earliestDate = monthRange.value("earliest_date").toString();
    QString latestDate = monthRange.value("latest_time").toString();

    if (earliestDate.isEmpty() || latestDate.isEmpty()) {
        LoggerManager::instance().log(logPath, Level::INFO,
            QString("检测情况: 无法获取月份范围 (earliest=%1, latest=%2)")
                .arg(earliestDate, latestDate).toStdString());
        LoggerManager::instance().flush(logPath);
        return;
    }

    int monthDiff = calculateMonthDifference(earliestDate, latestDate);
    bool needCleanup = monthDiff > m_config.retainMonths;

    LoggerManager::instance().log(logPath, Level::INFO,
        QString("检测情况: 最早日期=%1, 最新日期=%2, 覆盖月数=%3, 保留阈值=%4, 需要清理: %5")
            .arg(earliestDate.left(10), latestDate.left(10))
            .arg(monthDiff)
            .arg(m_config.retainMonths)
            .arg(needCleanup ? "是" : "否")
            .toStdString());

    if (!needCleanup) {
        LoggerManager::instance().flush(logPath);
        return;
    }

    LoggerManager::instance().log(logPath, Level::INFO,
        QString("日志覆盖月数: %1, 超过保留阈值: %2, 触发清理")
            .arg(monthDiff).arg(m_config.retainMonths).toStdString());

    // 计算要删除的时间区间：从最早日期开始到 +cleanupMonths 个月
    QDate earliest = QDate::fromString(earliestDate.left(10), "yyyy-MM-dd");
    if (!earliest.isValid()) {
        LoggerManager::instance().log(logPath, Level::ERROR,
            QString("无效的最早日期: %1").arg(earliestDate).toStdString());
        LoggerManager::instance().flush(logPath);
        return;
    }
    QDate cutoffDate = earliest.addMonths(m_config.cleanupMonths);

    QString startTime = earliest.toString("yyyy-MM-dd 00:00:00");
    QString endTime = cutoffDate.toString("yyyy-MM-dd 23:59:59");

    LoggerManager::instance().log(logPath, Level::INFO,
        QString("正在清理日志: %1 至 %2").arg(startTime, endTime).toStdString());

    m_deleteByRangeFn(startTime, endTime);

    LoggerManager::instance().log(logPath, Level::INFO,
        QString("清理完成: 已删除 %1 至 %2 的日志记录 (monthDiff=%3)")
            .arg(startTime, endTime).arg(monthDiff).toStdString());

    LoggerManager::instance().flush(logPath);
}

int LogCleanupScheduler::calculateMonthDifference(const QString& earliestDate, const QString& latestDate)
{
    QDate earliest = QDate::fromString(earliestDate.left(10), "yyyy-MM-dd");
    QDate latest = QDate::fromString(latestDate.left(10), "yyyy-MM-dd");
    if (!earliest.isValid() || !latest.isValid()) {
        return 0;
    }
    int years = latest.year() - earliest.year();
    int months = latest.month() - earliest.month();
    return years * 12 + months;
}

} // namespace LogDB
