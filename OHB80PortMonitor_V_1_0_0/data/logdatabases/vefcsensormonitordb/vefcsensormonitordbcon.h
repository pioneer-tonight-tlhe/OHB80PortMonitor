/*******************************************************************************************
 * @file vefcsensormonitordbcon.h
 * @author Simon <工号：13> 2026-06-11
 *
 * @class VEFCSensorMonitorDBCon
 * @brief 负责 VEFC 采集数据库连接线程与上层访问接口的协调。
 *
 * 设计目标：
 *      1. 统一封装 VEFC 采样数据库的跨线程访问入口，减少任务层直接操作 SQL 逻辑对象。
 *      2. 将查询调用、写入投递和写入结果回传收口到同一连接类，保持上层调用语义稳定。
 *      3. 复用外部写线程执行实际落库，避免在当前连接类中重复维护写事务能力。
 *******************************************************************************************/
#ifndef VEFCSENSORMONITORDBCON_H
#define VEFCSENSORMONITORDBCON_H

#include <QObject>
#include <QThread>
#include <QVariantMap>
#include "vefcsensormonitorsqllogic.h"
#include "writesqldbcon.h"
#include "vefcsensormonitorrecord.h"

namespace LogDB {

class VEFCSensorMonitorDBCon : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造与析构 ============================
    VEFCSensorMonitorDBCon(const QString& databasePath,
                           WriteSqlDBCon* externalWriteCon,
                           QObject* parent = nullptr);
    ~VEFCSensorMonitorDBCon();

    VEFCSensorMonitorDBCon() = delete;
    VEFCSensorMonitorDBCon(const VEFCSensorMonitorDBCon&) = delete;
    VEFCSensorMonitorDBCon& operator=(const VEFCSensorMonitorDBCon&) = delete;

    // ============================ 生命周期管理 ============================
    bool initialize();
    void cleanup();

    // ============================ 采样数据写入 ============================
    void insertRecord(const QString& qrCode,
                      qint64 recordTimestamp,
                      double gasPressure,
                      double actualFlow,
                      double sensorPressure,
                      double sensorTemperature);
    void insertRecord(const VEFCSensorMonitorRecord& record);
    void insertRecords(const QVector<VEFCSensorMonitorRecord>& records);
    void deleteByTimeRange(qint64 startTimestamp, qint64 endTimestamp);
    bool deleteByDateTimeRange(const QString& startTime, const QString& endTime);

    // ============================ 采样数据查询 ============================
    QVariantMap queryDailyAverage(qint64 dayStartTimestamp, qint64 nextDayStartTimestamp);
    QVector<VEFCSensorMonitorRecord> queryRecordsByTimeRange(qint64 startTimestamp, qint64 endTimestamp);
    QVector<VEFCSensorMonitorRecord> queryOldestWeekRecords();
    void queryTimeBounds(QString& earliestTime, QString& latestTime);

private:
    // ---- 写入结果转换 ----
    static VEFCSensorMonitorRecord recordFromParams(const QVariantList& params, int offset);
    static QVector<VEFCSensorMonitorRecord> recordsFromParams(const QVariantList& params);

signals:
    // ---- 采样数据写入事件 ----
    void recordInserted(const VEFCSensorMonitorRecord& record);
    void recordsInserted(const QVector<VEFCSensorMonitorRecord>& records);

private slots:
    // ---- 写入结果处理 ----
    void onWriteTaskCompleted(const WriteResult& result);

private:
    // ---- 状态成员 ----
    // 承载 SQL 逻辑对象的工作线程，确保数据库查询与调度逻辑在独立线程中执行。
    QThread* m_workerThread;

    // VEFC 采集库的 SQL 逻辑对象，负责具体查询、写入投递封装和清理调度初始化。
    VEFCSensorMonitorSqlLogic* m_sqlLogic;

    // 外部统一写线程连接对象，仅引用不持有，用于接收写入任务并完成实际落库。
    WriteSqlDBCon* m_writeCon;
};

} // namespace LogDB

#endif // VEFCSENSORMONITORDBCON_H
