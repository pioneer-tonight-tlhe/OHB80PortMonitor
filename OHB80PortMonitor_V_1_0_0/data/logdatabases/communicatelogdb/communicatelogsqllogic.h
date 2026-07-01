/*******************************************************************************************
 * @file communicatelogsqllogic.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class CommunicateLogSqlLogic
 * @brief 在数据库工作线程中执行通讯日志 SQL 初始化、查询和清理请求。
 *
 * 设计目标：
 *      1. 集中维护 `communicate_log` 表的 SQL 构造、分页查询和写入逻辑。
 *      2. 将排序方向、条件过滤和权限过滤统一下沉到 data 层执行。
 *      3. 复用月度清理调度器，保持通讯日志数据库生命周期管理一致。
 *******************************************************************************************/
#ifndef COMMUNICATELOGSQLLOGIC_H
#define COMMUNICATELOGSQLLOGIC_H

#include <QByteArray>
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

class CommunicateLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit CommunicateLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~CommunicateLogSqlLogic();

public slots:
    // ============================ 生命周期 ============================
    bool initializeDatabase();

    // ============================ 写入接口 ============================
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

    // ============================ 查询接口 ============================
    QList<QVariantMap> queryPageWithConditions(const QString& commandId,
                                               const QString& qrCode,
                                               int execStatus,
                                               int retryCount,
                                               const QString& startTime,
                                               const QString& endTime,
                                               int pageSize,
                                               int pageNumber,
                                               int sortOrder,
                                               int maxUserPermission);
    int queryTotalCount();
    int queryTotalCountWithConditions(const QString& commandId,
                                      const QString& qrCode,
                                      int execStatus,
                                      int retryCount,
                                      const QString& startTime,
                                      const QString& endTime,
                                      int maxUserPermission);
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

#endif // COMMUNICATELOGSQLLOGIC_H
