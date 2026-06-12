#include "logcleanupscheduler.h"
#include "loggermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QStringList>
#include <QTimer>

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
    const QDateTime now = QDateTime::currentDateTime();
    if (now.time().hour() != m_config.dailyCheckTime.hour()
            || now.time().minute() < m_config.dailyCheckTime.minute()) {
        return;
    }

    if (m_lastCleanupCheckDate == now.date()) {
        return;
    }

    m_lastCleanupCheckDate = now.date();
    performCleanupCheck();
}

void LogCleanupScheduler::performCleanupCheck()
{
    const std::string& logPath = m_config.logPath;

    if (!m_monthRangeProvider || !m_deleteByRangeFn) {
        LoggerManager::getInstance()->log(logPath, Level::WARN,
            QString("\n--------------------------------------------------------------------------------\n"
                    "数据库定时清理配置异常\n"
                    "数据库：%1\n"
                    "表名：%2\n"
                    "失败原因：月份范围查询或删除回调未设置\n"
                    "--------------------------------------------------------------------------------")
                .arg(m_config.databaseName, m_config.tableName)
                .toStdString());
        LoggerManager::getInstance()->flush(logPath);
        return;
    }

    const QVariantMap monthRange = m_monthRangeProvider();
    const QString earliestDate = monthRange.value(QStringLiteral("earliest_date")).toString();
    const QString latestDate = monthRange.value(QStringLiteral("latest_time")).toString();

    if (earliestDate.isEmpty() || latestDate.isEmpty()) {
        LoggerManager::getInstance()->log(logPath, Level::INFO,
            QString("\n--------------------------------------------------------------------------------\n"
                    "数据库定时清理检查\n"
                    "数据库：%1\n"
                    "表名：%2\n"
                    "保留策略：保留最近 %3 个月\n"
                    "每次清理：最早 %4 个月\n"
                    "检查结果：无可清理数据\n"
                    "数据库最早记录：N/A\n"
                    "数据库最新记录：N/A\n"
                    "--------------------------------------------------------------------------------")
                .arg(m_config.databaseName, m_config.tableName)
                .arg(m_config.retainMonths)
                .arg(m_config.cleanupMonths)
                .toStdString());
        LoggerManager::getInstance()->flush(logPath);
        return;
    }

    const int monthDiff = calculateMonthDifference(earliestDate, latestDate);
    const bool needCleanup = monthDiff > m_config.retainMonths;

    if (!needCleanup) {
        LoggerManager::getInstance()->log(logPath, Level::INFO,
            buildCleanupLogBlock(QStringLiteral("数据库定时清理检查"),
                                 earliestDate,
                                 latestDate,
                                 monthDiff,
                                 false,
                                 QString(),
                                 QString(),
                                 QString(),
                                 QStringLiteral("未触发清理")).toStdString());
        LoggerManager::getInstance()->flush(logPath);
        return;
    }

    const QDate earliest = QDate::fromString(earliestDate.left(10), QStringLiteral("yyyy-MM-dd"));
    if (!earliest.isValid()) {
        LoggerManager::getInstance()->log(logPath, Level::ERROR,
            QString("\n--------------------------------------------------------------------------------\n"
                    "数据库定时清理失败\n"
                    "数据库：%1\n"
                    "表名：%2\n"
                    "失败原因：无效的最早记录日期 %3\n"
                    "--------------------------------------------------------------------------------")
                .arg(m_config.databaseName, m_config.tableName, earliestDate)
                .toStdString());
        LoggerManager::getInstance()->flush(logPath);
        return;
    }

    const QDate cutoffDate = earliest.addMonths(m_config.cleanupMonths);
    const QString startTime = earliest.toString(QStringLiteral("yyyy-MM-dd 00:00:00"));
    const QString endTime = cutoffDate.toString(QStringLiteral("yyyy-MM-dd 23:59:59"));

    QStringList monthsList;
    QDate monthIter = earliest;
    while (monthIter <= cutoffDate) {
        monthsList.append(monthIter.toString(QStringLiteral("yyyy-MM")));
        monthIter = monthIter.addMonths(1);
        monthIter = QDate(monthIter.year(), monthIter.month(), 1);
    }
    monthsList.removeDuplicates();
    const QString monthsStr = monthsList.join(QStringLiteral(", "));

    LoggerManager::getInstance()->log(logPath, Level::INFO,
        buildCleanupLogBlock(QStringLiteral("数据库定时清理触发"),
                             earliestDate,
                             latestDate,
                             monthDiff,
                             true,
                             startTime,
                             endTime,
                             monthsStr,
                             QStringLiteral("正在提交删除任务")).toStdString());

    const bool deleteSubmitted = m_deleteByRangeFn(startTime, endTime);
    LoggerManager::getInstance()->log(logPath, deleteSubmitted ? Level::INFO : Level::ERROR,
        buildCleanupLogBlock(deleteSubmitted ? QStringLiteral("数据库定时清理提交完成")
                                             : QStringLiteral("数据库定时清理提交失败"),
                             earliestDate,
                             latestDate,
                             monthDiff,
                             true,
                             startTime,
                             endTime,
                             monthsStr,
                             deleteSubmitted ? QStringLiteral("已提交删除任务")
                                             : QStringLiteral("删除任务提交失败")).toStdString());

    LoggerManager::getInstance()->flush(logPath);
}

QString LogCleanupScheduler::buildCleanupLogBlock(const QString& title,
                                                  const QString& earliestDate,
                                                  const QString& latestDate,
                                                  int monthDiff,
                                                  bool needCleanup,
                                                  const QString& startTime,
                                                  const QString& endTime,
                                                  const QString& monthsStr,
                                                  const QString& resultText) const
{
    QStringList lines;
    lines << QStringLiteral("")
          << QStringLiteral("--------------------------------------------------------------------------------")
          << title
          << QStringLiteral("数据库：%1").arg(m_config.databaseName)
          << QStringLiteral("表名：%1").arg(m_config.tableName)
          << QStringLiteral("保留策略：保留最近 %1 个月").arg(m_config.retainMonths)
          << QStringLiteral("每次清理：最早 %1 个月").arg(m_config.cleanupMonths)
          << QStringLiteral("触发条件：数据覆盖月份 %1 %2 保留月份 %3")
                 .arg(monthDiff)
                 .arg(needCleanup ? QStringLiteral(">") : QStringLiteral("<="))
                 .arg(m_config.retainMonths)
          << QStringLiteral("数据库最早记录：%1").arg(earliestDate)
          << QStringLiteral("数据库最新记录：%1").arg(latestDate);

    if (!startTime.isEmpty() || !endTime.isEmpty()) {
        lines << QStringLiteral("清理范围：%1 至 %2").arg(startTime, endTime);
    }
    if (!monthsStr.isEmpty()) {
        lines << QStringLiteral("涉及月份：[%1]").arg(monthsStr);
    }
    if (!resultText.isEmpty()) {
        lines << QStringLiteral("执行结果：%1").arg(resultText);
    }

    lines << QStringLiteral("--------------------------------------------------------------------------------");
    return lines.join(QStringLiteral("\n"));
}

int LogCleanupScheduler::calculateMonthDifference(const QString& earliestDate, const QString& latestDate)
{
    const QDate earliest = QDate::fromString(earliestDate.left(10), QStringLiteral("yyyy-MM-dd"));
    const QDate latest = QDate::fromString(latestDate.left(10), QStringLiteral("yyyy-MM-dd"));
    if (!earliest.isValid() || !latest.isValid()) {
        return 0;
    }
    const int years = latest.year() - earliest.year();
    const int months = latest.month() - earliest.month();
    return years * 12 + months;
}

} // namespace LogDB
