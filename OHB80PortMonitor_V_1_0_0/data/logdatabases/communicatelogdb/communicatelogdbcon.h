/*******************************************************************************************
 * @file communicatelogdbcon.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class CommunicateLogDBCon
 * @brief 封装通讯日志数据库线程访问、异步写入转发和查询结果类型转换。
 *
 * 设计目标：
 *      1. 将通讯日志 SQL 逻辑固定在独立工作线程中执行，隔离 UI 线程阻塞风险。
 *      2. 通过统一写入连接提交 INSERT 和 DELETE，避免多连接写竞争。
 *      3. 为 scheduler 和 UI 提供带排序、权限过滤的类型化查询接口。
 *******************************************************************************************/
#ifndef COMMUNICATELOGDBCON_H
#define COMMUNICATELOGDBCON_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QThread>

#include "communicaterecord.h"
#include "communicatelogsqllogic.h"
#include "writesqldbcon.h"

namespace LogDB {

class CommunicateLogDBCon : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    CommunicateLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon,
                        QObject* parent = nullptr);
    ~CommunicateLogDBCon();

    CommunicateLogDBCon() = delete;
    CommunicateLogDBCon(const CommunicateLogDBCon&) = delete;
    CommunicateLogDBCon& operator=(const CommunicateLogDBCon&) = delete;

    // ============================ 生命周期 ============================
    bool initialize();
    void cleanup();

    // ============================ 查询接口 ============================
    QList<CommunicateRecord> queryPageWithConditions(const QString& commandId,
                                                     const QString& qrCode,
                                                     int execStatus,
                                                     int retryCount,
                                                     const QString& startTime,
                                                     const QString& endTime,
                                                     int pageSize,
                                                     int pageNumber,
                                                     SortOrder sortOrder = SortOrder::Desc,
                                                     int maxUserPermission = 0);
    int queryTotalCount();
    int queryTotalCountWithConditions(const QString& commandId,
                                      const QString& qrCode,
                                      int execStatus,
                                      int retryCount,
                                      const QString& startTime,
                                      const QString& endTime,
                                      int maxUserPermission = 0);
    int queryMonthRange();
    void queryTimeBounds(QString& earliestTime, QString& latestTime);

    // ============================ 写入接口 ============================
    void insertRecord(const QString& sendTime,
                      const QString& responseTime,
                      const QString& commandId,
                      const QString& qrCode,
                      int execStatus,
                      int retryCount,
                      const QByteArray& sendFrame,
                      const QByteArray& responseFrame,
                      const QString& description,
                      int userPermission = 0);
    void deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    // ---- 实时结果 ----
    void recordInserted(const CommunicateRecord& record);

private slots:
    // ---- 写入结果 ----
    void onWriteTaskCompleted(const WriteResult& result);

private:
    // ---- 线程与逻辑成员 ----
    QThread* m_workerThread;
    CommunicateLogSqlLogic* m_sqlLogic;
    WriteSqlDBCon* m_writeCon;
};

} // namespace LogDB

#endif // COMMUNICATELOGDBCON_H
