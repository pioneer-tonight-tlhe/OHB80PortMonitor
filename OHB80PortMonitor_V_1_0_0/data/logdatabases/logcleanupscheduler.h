#ifndef LOGCLEANUPSCHEDULER_H
#define LOGCLEANUPSCHEDULER_H

#include <QObject>
#include <QDate>
#include <QString>
#include <QVariantMap>
#include <functional>
#include <string>

class QTimer;

namespace LogDB {

// 通用日志定期清理调度器
//
// 与具体表无关：通过回调函数获取月份范围、执行删除操作。
// 调度策略：
//   - 每隔 checkIntervalMs 毫秒检查一次（默认每分钟）
//   - 仅当日期切换到下一天时才执行清理判断
//   - 当数据库覆盖月数 > retainMonths 时，删除最早的 cleanupMonths 个月
//
// 使用示例：
//   LogCleanupScheduler::Config cfg;
//   cfg.retainMonths = 12;
//   cfg.cleanupMonths = 3;
//   auto* scheduler = new LogCleanupScheduler(cfg, this);
//   scheduler->setMonthRangeProvider([this]{ return queryMonthRange(); });
//   scheduler->setDeleteByRangeFn([this](const QString& s, const QString& e){ deleteByTimeRange(s, e); });
//   scheduler->start();
class LogCleanupScheduler : public QObject
{
    Q_OBJECT

public:
    // 月份范围提供者：返回包含 earliest_date / latest_time 的QVariantMap
    using MonthRangeProvider = std::function<QVariantMap()>;

    // 时间区间删除函数
    using DeleteByRangeFn = std::function<void(const QString& startTime, const QString& endTime)>;

    struct Config {
        int checkIntervalMs;  // 检查间隔（毫秒），默认60秒
        int retainMonths;    // 数据库最多保留多少个月的日志
        int cleanupMonths;   // 触发清理时一次性删除最早的多少个月
        std::string logPath; // LoggerManager 日志路径，格式: log_db/{db_name}/month_clean

        Config()
            : checkIntervalMs(60000)
            , retainMonths(12)
            , cleanupMonths(3)
            , logPath("log_db/month_clean")
        {}
    };

    explicit LogCleanupScheduler(const Config& config = Config(), QObject* parent = nullptr);
    ~LogCleanupScheduler();

    // 设置回调（必须在start()前设置）
    void setMonthRangeProvider(MonthRangeProvider provider);
    void setDeleteByRangeFn(DeleteByRangeFn fn);

    // 启动/停止定时器
    void start();
    void stop();

    // 手动触发一次清理判断（不等待日期切换；仅检查月份是否超限）
    void triggerCleanupCheck();

private slots:
    void onTimerTick();

private:
    // 计算两个日期字符串（yyyy-MM-dd 或 yyyy-MM-dd HH:mm:ss）之间的月份差
    static int calculateMonthDifference(const QString& earliestDate, const QString& latestDate);

    // 实际执行清理判断逻辑
    void performCleanupCheck();

    Config m_config;
    QTimer* m_timer;
    MonthRangeProvider m_monthRangeProvider;
    DeleteByRangeFn m_deleteByRangeFn;
};

} // namespace LogDB

#endif // LOGCLEANUPSCHEDULER_H
