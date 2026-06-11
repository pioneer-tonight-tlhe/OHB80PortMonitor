/*******************************************************************************************
 * @file vefcsensormonitorsqllogic.h
 * @author Simon <工号：13> 2026-06-11
 *
 * @class VEFCSensorMonitorSqlLogic
 * @brief 负责 VEFC 采集数据库的查询、写入投递和定时清理协调。
 *
 * 设计目标：
 *      1. 统一封装 VEFC 采样记录的查询接口与写入投递入口，减少上层模块直接拼接 SQL。
 *      2. 复用通用清理调度器，在逻辑层内完成“仅保留 1 个月采集数据”的生命周期管理。
 *      3. 保持读写职责边界清晰，查询走独立连接，写入与删除统一投递到写线程执行。
 *******************************************************************************************/
#ifndef VEFCSENSORMONITORSQLLOGIC_H
#define VEFCSENSORMONITORSQLLOGIC_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariantMap>
#include "sqlmapper.h"
#include "dbtypes.h"
#include "vefcsensormonitorrecord.h"

namespace LogDB {

class LogCleanupScheduler;

class VEFCSensorMonitorSqlLogic : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造与析构 ============================
    explicit VEFCSensorMonitorSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~VEFCSensorMonitorSqlLogic();

public slots:
    // ============================ 数据库初始化 ============================
    bool initializeDatabase();

    // ============================ 采样数据写入 ============================
    bool insertRecord(const QString& qrCode,
                      qint64 recordTimestamp,
                      double gasPressure,
                      double actualFlow,
                      double sensorPressure,
                      double sensorTemperature);

    bool insertRecords(const QVector<VEFCSensorMonitorRecord>& records);
    bool deleteByTimeRange(qint64 startTimestamp, qint64 endTimestamp);

    // ============================ 采样数据查询 ============================
    QVariantMap queryDailyAverage(qint64 dayStartTimestamp, qint64 nextDayStartTimestamp);
    QVector<VEFCSensorMonitorRecord> queryRecordsByTimeRange(qint64 startTimestamp, qint64 endTimestamp);
    QVector<VEFCSensorMonitorRecord> queryOldestWeekRecords();
    QVariantMap queryMonthRange();

private:
    // ---- 定时清理 ----
    void initializeCleanupScheduler();
    bool deleteByDateTimeRange(const QString& startTime, const QString& endTime);

    // ---- 写入任务构建 ----
    WriteResult makeWriteResult(const QString& sqlId,
                                const QString& sql,
                                const QVariantList& params,
                                WriteOp opType = WriteOp::Other) const;

signals:
    // ---- 写入任务投递 ----
    void writeExecuted(const WriteResult& result);

private:
    // ---- 状态成员 ----
    // 当前 VEFC 采集数据库目录路径，用于定位 SQL 映射文件和 SQLite 数据文件。
    QString m_databasePath;

    // 独立查询连接的连接名，用于区分当前逻辑层持有的 SQLite 连接实例。
    QString m_connectionName;

    // SQL 映射器，负责按 SQL ID 读取并返回对应的 SQL 语句模板。
    SqlMapper* m_sqlMapper;

    // 当前逻辑层持有的查询数据库连接，仅用于同步查询与清理范围判断。
    QSqlDatabase m_database;

    // 月度清理调度器，负责按周期检查 VEFC 采集库并触发“仅保留 1 个月数据”的清理流程。
    LogCleanupScheduler* m_cleanupScheduler;
};

} // namespace LogDB

#endif // VEFCSENSORMONITORSQLLOGIC_H
