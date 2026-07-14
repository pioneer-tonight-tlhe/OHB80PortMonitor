/*******************************************************************************************
 * @file alarmlogsqllogic.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class AlarmLogSqlLogic
 * @brief 在数据库工作线程中执行警报日志 SQL 初始化、查询和清理请求。
 *
 * 设计目标：
 *      1. 集中维护 `alarm_log` 表的 SQL 构造、分页查询和状态更新逻辑。
 *      2. 将时间范围、解决状态和权限过滤统一下沉到 data 层执行。
 *      3. 复用月度清理调度器，保持警报日志数据库生命周期管理一致。
 *******************************************************************************************/
#ifndef ALARMLOGSQLLOGIC_H
#define ALARMLOGSQLLOGIC_H

#include <QList>
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

class AlarmLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit AlarmLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~AlarmLogSqlLogic();

public slots:
    // ============================ 生命周期 ============================
    bool initializeDatabase();

    // ============================ 写入接口 ============================
    bool insertRecord(int alarmLevel,
                      const QString& occurTime,
                      const QString& qrCode,
                      const QString& alarmType,
                      int isResolved,
                      const QString& resolveTime,
                      const QString& description,
                      int userPermission = 0);

    // ============================ 查询接口 ============================
    QList<QVariantMap> queryPageWithConditions(int alarmLevel,
                                               const QString& qrCode,
                                               const QString& alarmType,
                                               int isResolved,
                                               const QString& startTime,
                                               const QString& endTime,
                                               int pageSize,
                                               int pageNumber,
                                               const QString& resolveStartTime = QString(),
                                               const QString& resolveEndTime = QString(),
                                               int maxUserPermission = -1);
    int queryTotalCount(int maxUserPermission = -1);
    int queryTotalCountWithConditions(int alarmLevel,
                                      const QString& qrCode,
                                      const QString& alarmType,
                                      int isResolved,
                                      const QString& startTime,
                                      const QString& endTime,
                                      const QString& resolveStartTime = QString(),
                                      const QString& resolveEndTime = QString(),
                                      int maxUserPermission = -1);
    QVariantMap queryMonthRange();

    // ============================ 清理接口 ============================
    bool deleteByTimeRange(const QString& startTime, const QString& endTime);
    bool updateResolve(const QString& qrCode, const QString& alarmType, const QString& resolveTime);
    QList<QVariantMap> resolveUnresolvedBatch(int batchSize, const QString& resolveTime);

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

#endif // ALARMLOGSQLLOGIC_H
