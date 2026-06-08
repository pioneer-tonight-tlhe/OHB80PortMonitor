#ifndef ALARMLOGSQLLOGIC_H
#define ALARMLOGSQLLOGIC_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include "sqlmapper.h"
#include "dbtypes.h"

namespace LogDB {

class LogCleanupScheduler;

class AlarmLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    explicit AlarmLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~AlarmLogSqlLogic();

public slots:
    // 初始化数据库连接
    bool initializeDatabase();

    // 插入数据
    // userPermission 默认 0（UserPermission::Guest），兼容旧调用方
    bool insertRecord(int alarmLevel,
                      const QString& occurTime,
                      const QString& qrCode,
                      const QString& alarmType,
                      int isResolved,
                      const QString& resolveTime,
                      const QString& description,
                      int userPermission = 0);

    // 分页条件查询
    // 任意条件传入空字符串/-1表示不应用该条件
    QList<QVariantMap> queryPageWithConditions(int alarmLevel,
                                               const QString& qrCode,
                                               const QString& alarmType,
                                               int isResolved,
                                               const QString& startTime,
                                               const QString& endTime,
                                               int pageSize,
                                               int pageNumber);

    // 总记录数（无条件，直接读取 log_record_count 缓存表）
    int queryTotalCount();

    // 条件查询的总记录数
    int queryTotalCountWithConditions(int alarmLevel,
                                      const QString& qrCode,
                                      const QString& alarmType,
                                      int isResolved,
                                      const QString& startTime,
                                      const QString& endTime);

    // 查询数据库的最早/最晚时间（用于计算月份范围）
    QVariantMap queryMonthRange();

    // 批量删除时间区间记录
    bool deleteByTimeRange(const QString& startTime, const QString& endTime);

    // 把指定 (qrCode, alarmType) 下 is_resolved=0 的记录原位标记为已解决
    bool updateResolve(const QString& qrCode, const QString& alarmType, const QString& resolveTime);

signals:
    // 写入语句执行结果信号
    void writeExecuted(const WriteResult& result);

private:
    void initializeCleanupScheduler();
    int calculateOffset(int pageSize, int pageNumber);

    QString m_databasePath;
    QString m_connectionName;
    SqlMapper* m_sqlMapper;
    QSqlDatabase m_database;
    LogCleanupScheduler* m_cleanupScheduler;
};

} // namespace LogDB

#endif // ALARMLOGSQLLOGIC_H
