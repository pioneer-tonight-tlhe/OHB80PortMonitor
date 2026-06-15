/*******************************************************************************************
 * @file operationlogsqllogic.h
 * @author Simon <工号:13> 2026-06-15
 *
 * @class OperationLogSqlLogic
 * @brief 在数据库工作线程中执行运行日志 SQL 初始化、查询和清理请求提交。
 *
 * 设计目标:
 *      1. 集中维护 operation_log 表的 SQL 查询与写入请求构造。
 *      2. 将用户权限过滤放入 SQL 查询条件，保证分页统计和可见记录一致。
 *      3. 持有运行日志月度清理调度器，统一处理保留周期清理。
 *******************************************************************************************/
#ifndef OPERATIONLOGSQLLOGIC_H
#define OPERATIONLOGSQLLOGIC_H

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include "dbtypes.h"
#include "sqlmapper.h"

namespace LogDB {

class LogCleanupScheduler;

class OperationLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit OperationLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~OperationLogSqlLogic();

public slots:
    // ============================ 生命周期 ============================
    bool initializeDatabase();

    // ============================ 写入接口 ============================
    bool insertRecord(const QString& occurTime, int logType, const QString& description,
                      int userPermission = 0);

    // ============================ 查询接口 ============================
    QList<QVariantMap> queryPagination(int pageSize, int pageNumber, int maxUserPermission);
    int queryTotalCount(int maxUserPermission);
    QList<QVariantMap> queryPaginationInRange(const QString& startTime, const QString& endTime,
                                              int pageSize, int pageNumber,
                                              int maxUserPermission);
    QList<QVariantMap> queryPaginationWithBaseConditions(const QString& startTime, const QString& endTime,
                                                         int logType, int pageSize, int pageNumber,
                                                         int maxUserPermission);
    int queryTotalCountInRange(const QString& startTime, const QString& endTime,
                               int maxUserPermission);
    int queryTotalCountWithBaseConditions(const QString& startTime, const QString& endTime,
                                          int logType, int maxUserPermission);
    int queryRecordPageInRange(int recordId, const QString& startTime, const QString& endTime,
                               int pageSize, int maxUserPermission);
    int queryRecordPageWithBaseConditions(int recordId, const QString& startTime, const QString& endTime,
                                          int logType, int pageSize, int maxUserPermission);
    QList<QVariantMap> queryPageWithConditions(const QString& startTime, const QString& endTime,
                                               int logType, const QString& keyword,
                                               int pageSize, int pageNumber,
                                               int maxUserPermission);
    int queryTotalCountWithConditions(const QString& startTime, const QString& endTime,
                                      int logType, const QString& keyword,
                                      int maxUserPermission);
    int queryRecordPosition(int recordId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword, int maxUserPermission);
    int queryFirstRecordPage(const QString& startTime, const QString& endTime,
                             int logType, const QString& keyword, int pageSize,
                             int maxUserPermission);
    int queryFirstMatchedId(const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword, int maxUserPermission);
    int queryLastMatchedId(const QString& startTime, const QString& endTime,
                           int logType, const QString& keyword, int maxUserPermission);
    int queryMatchedIdByPosition(int position, const QString& startTime, const QString& endTime,
                                 int logType, const QString& keyword, int maxUserPermission);
    int queryPrevMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword, int maxUserPermission);
    int queryNextMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword, int maxUserPermission);
    QVariantMap queryMonthRange();

    // ============================ 清理接口 ============================
    bool deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    // ---- 写入请求 ----
    void writeExecuted(const WriteResult& result);

private:
    // ---- 初始化辅助 ----
    void initializeCleanupScheduler();

    // ---- 查询辅助 ----
    int calculateOffset(int pageSize, int pageNumber);

    // ---- 数据库状态成员 ----
    QString m_databasePath;
    QString m_connectionName;
    SqlMapper* m_sqlMapper;
    QSqlDatabase m_database;
    LogCleanupScheduler* m_cleanupScheduler;
};

} // namespace LogDB

#endif // OPERATIONLOGSQLLOGIC_H
