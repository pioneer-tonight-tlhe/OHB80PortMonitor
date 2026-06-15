/*******************************************************************************************
 * @file operationlogdbcon.h
 * @author Simon <工号:13> 2026-06-15
 *
 * @class OperationLogDBCon
 * @brief 封装运行日志数据库线程访问、异步写入转发和查询结果类型转换。
 *
 * 设计目标:
 *      1. 将运行日志 SQL 逻辑固定在独立工作线程中执行。
 *      2. 通过统一写入连接提交 INSERT/DELETE，避免多个写连接竞争。
 *      3. 为 UI 和调度任务提供带用户权限过滤的类型化查询接口。
 *******************************************************************************************/
#ifndef OPERATIONLOGDBCON_H
#define OPERATIONLOGDBCON_H

#include <QList>
#include <QObject>
#include <QString>
#include <QThread>

#include "operationlogsqllogic.h"
#include "operationrecord.h"
#include "writesqldbcon.h"

namespace LogDB {

class OperationLogDBCon : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    OperationLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon,
                      QObject* parent = nullptr);
    ~OperationLogDBCon();

    OperationLogDBCon() = delete;
    OperationLogDBCon(const OperationLogDBCon&) = delete;
    OperationLogDBCon& operator=(const OperationLogDBCon&) = delete;

    // ============================ 生命周期 ============================
    bool initialize();
    void cleanup();

    // ============================ 查询接口 ============================
    QList<OperationRecord> queryPagination(int pageSize, int pageNumber, int maxUserPermission);
    int queryTotalCount(int maxUserPermission);
    QList<OperationRecord> queryPaginationInRange(const QString& startTime, const QString& endTime,
                                                  int pageSize, int pageNumber,
                                                  int maxUserPermission);
    QList<OperationRecord> queryPaginationWithBaseConditions(const QString& startTime, const QString& endTime,
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
    QList<OperationRecord> queryPageWithConditions(const QString& startTime, const QString& endTime,
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
    int queryMonthRange();
    void queryTimeBounds(QString& earliestTime, QString& latestTime);

    // ============================ 写入接口 ============================
    void insertRecord(const QString& occurTime, int logType, const QString& description,
                      int userPermission = 0);
    void deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    // ---- 写入结果 ----
    void recordInserted(const OperationRecord& record);

private slots:
    // ---- 写入结果 ----
    void onWriteTaskCompleted(const WriteResult& result);

private:
    // ---- 线程与逻辑成员 ----
    QThread* m_workerThread;
    OperationLogSqlLogic* m_sqlLogic;
    WriteSqlDBCon* m_writeCon;
};

} // namespace LogDB

#endif // OPERATIONLOGDBCON_H
