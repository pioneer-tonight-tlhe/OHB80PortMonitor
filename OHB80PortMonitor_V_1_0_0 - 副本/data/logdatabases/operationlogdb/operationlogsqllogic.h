#ifndef OPERATIONLOGSQLLOGIC_H
#define OPERATIONLOGSQLLOGIC_H

#include <QObject>
#include <QSqlDatabase>
#include <QDate>
#include <QVariant>
#include <QVariantMap>
#include <QVariantList>
#include "sqlmapper.h"
#include "dbtypes.h"

namespace LogDB {

class LogCleanupScheduler;

class OperationLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    explicit OperationLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~OperationLogSqlLogic();

public slots:
    // 初始化数据库连接
    bool initializeDatabase();

    // 插入数据
    // userPermission 默认 0（UserPermission::Guest），兼容旧调用方
    bool insertRecord(const QString& occurTime, int logType, const QString& description,
                      int userPermission = 0);

    // 查询方法
    QList<QVariantMap> queryPagination(int pageSize, int pageNumber);
    int queryTotalCount();
    // 范围内分页 / 范围内总数 / 记录在范围内所在页号
    QList<QVariantMap> queryPaginationInRange(const QString& startTime, const QString& endTime,
                                               int pageSize, int pageNumber);
    int queryTotalCountInRange(const QString& startTime, const QString& endTime);
    int queryRecordPageInRange(int recordId, const QString& startTime, const QString& endTime,
                               int pageSize);
    QList<QVariantMap> queryPageWithConditions(const QString& startTime, const QString& endTime,
                                                 int logType, const QString& keyword,
                                                 int pageSize, int pageNumber);
    int queryTotalCountWithConditions(const QString& startTime, const QString& endTime,
                                      int logType, const QString& keyword);
    int queryRecordPosition(int recordId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    int queryFirstRecordPage(const QString& startTime, const QString& endTime,
                             int logType, const QString& keyword, int pageSize);
    // 范围 + 条件下首条命中记录的 id（0 表示不存在）
    int queryFirstMatchedId(const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    // 基于 anchorId 查询上/下一条满足条件的记录 ID（0 表示不存在）
    int queryPrevMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    int queryNextMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    QVariantMap queryMonthRange();
    bool deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    // 写入语句执行结果信号
    void writeExecuted(const WriteResult& result);

private:
    // 初始化清理调度器
    void initializeCleanupScheduler();

    // 计算偏移量
    int calculateOffset(int pageSize, int pageNumber);

    QString m_databasePath;
    QString m_connectionName;
    SqlMapper* m_sqlMapper;
    QSqlDatabase m_database;
    LogCleanupScheduler* m_cleanupScheduler;
};

} // namespace LogDB

#endif // OPERATIONLOGSQLLOGIC_H
