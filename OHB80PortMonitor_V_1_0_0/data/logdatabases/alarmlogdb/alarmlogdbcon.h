/*******************************************************************************************
 * @file alarmlogdbcon.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class AlarmLogDBCon
 * @brief 封装警报日志数据库线程访问、异步写入转发和查询结果类型转换。
 *
 * 设计目标：
 *      1. 将警报日志 SQL 逻辑固定在独立工作线程中执行，隔离 UI 线程阻塞风险。
 *      2. 通过统一写入连接提交 INSERT、UPDATE 和 DELETE，避免多连接写竞争。
 *      3. 为 scheduler 和 UI 提供带权限过滤的类型化查询与实时信号接口。
 *******************************************************************************************/
#ifndef ALARMLOGDBCON_H
#define ALARMLOGDBCON_H

#include <QList>
#include <QObject>
#include <QString>
#include <QThread>

#include "alarmlogsqllogic.h"
#include "alarmrecord.h"
#include "writesqldbcon.h"

namespace LogDB {

class AlarmLogDBCon : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    AlarmLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon,
                  QObject* parent = nullptr);
    ~AlarmLogDBCon();

    AlarmLogDBCon() = delete;
    AlarmLogDBCon(const AlarmLogDBCon&) = delete;
    AlarmLogDBCon& operator=(const AlarmLogDBCon&) = delete;

    // ============================ 生命周期 ============================
    bool initialize();
    void cleanup();

    // ============================ 查询接口 ============================
    QList<AlarmRecord> queryPageWithConditions(int alarmLevel,
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
    int queryMonthRange();
    void queryTimeBounds(QString& earliestTime, QString& latestTime);

    // ============================ 写入接口 ============================
    void insertRecord(int alarmLevel,
                      const QString& occurTime,
                      const QString& qrCode,
                      const QString& alarmType,
                      int isResolved,
                      const QString& resolveTime,
                      const QString& description,
                      int userPermission = 0);
    void deleteByTimeRange(const QString& startTime, const QString& endTime);
    void updateResolve(const QString& qrCode, const QString& alarmType, const QString& resolveTime);

signals:
    // ---- 实时结果 ----
    void recordInserted(const AlarmRecord& record);
    void recordResolved(const QString& qrCode, const QString& alarmType, const QString& resolveTime);

private slots:
    // ---- 写入结果 ----
    void onWriteTaskCompleted(const WriteResult& result);

private:
    // ---- 线程与逻辑成员 ----
    QThread* m_workerThread;
    AlarmLogSqlLogic* m_sqlLogic;
    WriteSqlDBCon* m_writeCon;
};

} // namespace LogDB

#endif // ALARMLOGDBCON_H
