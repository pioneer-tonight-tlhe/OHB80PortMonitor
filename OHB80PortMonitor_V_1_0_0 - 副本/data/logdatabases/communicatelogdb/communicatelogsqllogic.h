#ifndef COMMUNICATELOGSQLLOGIC_H
#define COMMUNICATELOGSQLLOGIC_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include <QByteArray>
#include "sqlmapper.h"
#include "dbtypes.h"

class QTimer;

namespace LogDB {

class LogCleanupScheduler;

class CommunicateLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    explicit CommunicateLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~CommunicateLogSqlLogic();

public slots:
    // 初始化数据库连接
    bool initializeDatabase();

    // 插入数据
    // userPermission 默认 0（UserPermission::Guest），兼容旧调用方
    bool insertRecord(const QString& sendTime,
                      const QString& responseTime,
                      const QString& commandId,
                      const QString& qrCode,
                      int execStatus,
                      int retryCount,
                      const QByteArray& sendFrame,
                      const QByteArray& responseFrame,
                      const QString& description,
                      int userPermission = 0);

    // 分页条件查询
    // 任意条件传入空字符串/-1表示不应用该条件
    // sortOrder 通过 int 传递（对应 LogDB::SortOrder），便于跨线程 invokeMethod
    QList<QVariantMap> queryPageWithConditions(const QString& commandId,
                                               const QString& qrCode,
                                               int execStatus,
                                               int retryCount,
                                               const QString& startTime,
                                               const QString& endTime,
                                               int pageSize,
                                               int pageNumber,
                                               int sortOrder);

    // 总记录数（无条件，直接读取 log_record_count 缓存表）
    int queryTotalCount();

    // 条件查询的总记录数
    int queryTotalCountWithConditions(const QString& commandId,
                                      const QString& qrCode,
                                      int execStatus,
                                      int retryCount,
                                      const QString& startTime,
                                      const QString& endTime);

    // 查询数据库的最早/最晚时间（用于计算月份范围）
    QVariantMap queryMonthRange();

    // 批量删除时间区间记录
    bool deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    // 写入语句执行结果信号
    void writeExecuted(const WriteResult& result);

private slots:
    // 定期检查磁盘空间，超阈值时清理最早的日志
    void checkDiskSpaceAndCleanup();

private:
    // 初始化清理调度器
    void initializeCleanupScheduler();

    // 初始化磁盘空间检查定时器
    void initializeDiskCheckTimer();

    // 计算偏移量
    int calculateOffset(int pageSize, int pageNumber);

    // 磁盘空间检查常量
    static constexpr int    DISK_CHECK_INTERVAL_MS = 60000;  // 检查间隔 60s
    static constexpr double DISK_USAGE_THRESHOLD   = 0.90;   // 磁盘使用率 90% 触发清理
    static constexpr int    DISK_CLEANUP_MONTHS    = 1;      // 每次清理最早的 1 个月

    QString m_databasePath;
    QString m_connectionName;
    SqlMapper* m_sqlMapper;
    QSqlDatabase m_database;
    LogCleanupScheduler* m_cleanupScheduler;
    QTimer* m_diskCheckTimer;
};

} // namespace LogDB

#endif // COMMUNICATELOGSQLLOGIC_H
