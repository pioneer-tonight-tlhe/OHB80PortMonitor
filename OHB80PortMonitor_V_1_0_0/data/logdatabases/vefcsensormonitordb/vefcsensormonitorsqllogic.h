#ifndef VEFCSENSORMONITORSQLLOGIC_H
#define VEFCSENSORMONITORSQLLOGIC_H

#include <QObject>
#include <QSqlDatabase>
#include <QVariantMap>
#include "sqlmapper.h"
#include "dbtypes.h"
#include "vefcsensormonitorrecord.h"

namespace LogDB {

class VEFCSensorMonitorSqlLogic : public QObject
{
    Q_OBJECT

public:
    explicit VEFCSensorMonitorSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~VEFCSensorMonitorSqlLogic();

public slots:
    // 初始化独立查询连接；写入统一投递给 WriteSqlDBCon。
    bool initializeDatabase();

    // 插入单条 VEFC 传感器监控记录。
    bool insertRecord(const QString& qrCode,
                      qint64 recordTimestamp,
                      double gasPressure,
                      double actualFlow,
                      double sensorPressure,
                      double sensorTemperature);

    // 批量插入 VEFC 传感器监控记录。
    // 内部构建一条多 VALUES INSERT，由 WriteSqlDBCon 在同一个事务中执行。
    bool insertRecords(const QVector<VEFCSensorMonitorRecord>& records);

    // 删除指定时间区间内的记录，区间为 [startTimestamp, endTimestamp)。
    bool deleteByTimeRange(qint64 startTimestamp, qint64 endTimestamp);

    // 查询某一天所有字段平均值，区间为 [dayStartTimestamp, nextDayStartTimestamp)。
    QVariantMap queryDailyAverage(qint64 dayStartTimestamp, qint64 nextDayStartTimestamp);

    // 查询指定时间区间内的原始记录，区间为 [startTimestamp, endTimestamp)。
    QVector<VEFCSensorMonitorRecord> queryRecordsByTimeRange(qint64 startTimestamp, qint64 endTimestamp);

    // 查询数据库中时间最久的一个星期记录；数据跨度不足 7 天时返回空列表。
    QVector<VEFCSensorMonitorRecord> queryOldestWeekRecords();

signals:
    // 写入语句执行结果信号，由 DBCon 转交给统一写线程。
    void writeExecuted(const WriteResult& result);

private:
    WriteResult makeWriteResult(const QString& sqlId, const QString& sql, const QVariantList& params) const;

    QString m_databasePath;
    QString m_connectionName;
    SqlMapper* m_sqlMapper;
    QSqlDatabase m_database;
};

} // namespace LogDB

#endif // VEFCSENSORMONITORSQLLOGIC_H
