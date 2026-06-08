#ifndef OPERATIONLOGDBCON_H
#define OPERATIONLOGDBCON_H

#include <QObject>
#include <QThread>
#include <QVariantMap>
#include "operationlogsqllogic.h"
#include "writesqldbcon.h"
#include "operationrecord.h"

namespace LogDB {

class OperationLogDBCon : public QObject
{
    Q_OBJECT

public:
    // 必须传入外部 WriteSqlDBCon，本类不拥有其生命周期
    OperationLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon, QObject* parent = nullptr);
    ~OperationLogDBCon();

    // 禁用默认构造、拷贝、赋值
    OperationLogDBCon() = delete;
    OperationLogDBCon(const OperationLogDBCon&) = delete;
    OperationLogDBCon& operator=(const OperationLogDBCon&) = delete;

    // 初始化
    bool initialize();
    void cleanup();

    // 查询接口
    QList<OperationRecord> queryPagination(int pageSize, int pageNumber);
    int queryTotalCount();
    QList<OperationRecord> queryPaginationInRange(const QString& startTime, const QString& endTime,
                                               int pageSize, int pageNumber);
    int queryTotalCountInRange(const QString& startTime, const QString& endTime);
    int queryRecordPageInRange(int recordId, const QString& startTime, const QString& endTime,
                               int pageSize);
    QList<OperationRecord> queryPageWithConditions(const QString& startTime, const QString& endTime,
                                                int logType, const QString& keyword,
                                                int pageSize, int pageNumber);
    int queryTotalCountWithConditions(const QString& startTime, const QString& endTime,
                                       int logType, const QString& keyword);
    int queryRecordPosition(int recordId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    int queryFirstRecordPage(const QString& startTime, const QString& endTime,
                             int logType, const QString& keyword, int pageSize);
    int queryFirstMatchedId(const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    int queryPrevMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    int queryNextMatchingId(int anchorId, const QString& startTime, const QString& endTime,
                            int logType, const QString& keyword);
    int queryMonthRange();

    // 查询数据库中 occur_time 的最早 / 最晚时间。
    // 输出为 "yyyy-MM-dd HH:mm:ss" 格式；表为空时返回两个空字符串。
    void queryTimeBounds(QString& earliestTime, QString& latestTime);

    // 插入接口
    // userPermission: 触发该日志的用户权限级别（UserPermission 枚举），
    //                 默认 0（UserPermission::Guest），兼容旧调用方
    void insertRecord(const QString& occurTime, int logType, const QString& description,
                      int userPermission = 0);

    // 删除接口
    void deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    // 实时事件：本 DBCon 提交的 INSERT 已成功落库
    // 携带 OperationRecord（包含 operation_log 表所有字段）
    void recordInserted(const OperationRecord& record);

private slots:
    void onWriteTaskCompleted(const WriteResult& result);

private:
    QThread* m_workerThread;
    OperationLogSqlLogic* m_sqlLogic;
    WriteSqlDBCon* m_writeCon;
};

} // namespace LogDB

#endif // OPERATIONLOGDBCON_H
