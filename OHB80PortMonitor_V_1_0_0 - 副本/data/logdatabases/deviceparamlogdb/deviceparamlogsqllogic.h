#ifndef DEVICEPARAMLOGSQLLOGIC_H
#define DEVICEPARAMLOGSQLLOGIC_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include "sqlmapper.h"
#include "dbtypes.h"

namespace LogDB {

class LogCleanupScheduler;

class DeviceParamLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    explicit DeviceParamLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~DeviceParamLogSqlLogic();

public slots:
    // 初始化数据库连接
    bool initializeDatabase();

    // 插入数据
    // userPermission 默认 0（UserPermission::Guest），兼容旧调用方
    bool insertRecord(const QString& qrCode,
                      const QString& recordTime,
                      double inletPressure,
                      double outletPressure,
                      double inletFlow,
                      double humidity,
                      double temperature,
                      int foupStatus,
                      int userPermission = 0);

    // 分页条件查询
    // 任意条件传入空字符串表示不应用该条件
    QList<QVariantMap> queryPageWithConditions(const QString& qrCode,
                                               const QString& startTime,
                                               const QString& endTime,
                                               int pageSize,
                                               int pageNumber);

    // 总记录数（无条件，直接读取 log_record_count 缓存表）
    int queryTotalCount();

    // 条件查询的总记录数
    int queryTotalCountWithConditions(const QString& qrCode,
                                      const QString& startTime,
                                      const QString& endTime);

    // 查询数据库的最早/最晚时间（用于计算月份范围）
    QVariantMap queryMonthRange();

    // 批量删除时间区间记录
    bool deleteByTimeRange(const QString& startTime, const QString& endTime);

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

#endif // DEVICEPARAMLOGSQLLOGIC_H
