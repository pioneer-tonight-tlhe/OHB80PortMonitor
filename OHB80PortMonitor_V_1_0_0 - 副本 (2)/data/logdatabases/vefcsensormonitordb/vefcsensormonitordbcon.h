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
    // 必须传入外部 WriteSqlDBCon，本类不拥有其生命周期。
    VEFCSensorMonitorDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon, QObject* parent = nullptr);
    ~VEFCSensorMonitorDBCon();

    VEFCSensorMonitorDBCon() = delete;
    VEFCSensorMonitorDBCon(const VEFCSensorMonitorDBCon&) = delete;
    VEFCSensorMonitorDBCon& operator=(const VEFCSensorMonitorDBCon&) = delete;

    bool initialize();
    void cleanup();

    // 插入单条 VEFC 传感器监控记录。
    void insertRecord(const QString& qrCode,
                      qint64 recordTimestamp,
                      double gasPressure,
                      double actualFlow,
                      double sensorPressure,
                      double sensorTemperature);

    // 插入单条 VEFC 传感器监控记录。
    void insertRecord(const VEFCSensorMonitorRecord& record);

    // 批量插入 VEFC 传感器监控记录；适合同一轮 80 台设备一起落库。
    void insertRecords(const QVector<VEFCSensorMonitorRecord>& records);

    // 删除指定时间区间内的记录，区间为 [startTimestamp, endTimestamp)。
    void deleteByTimeRange(qint64 startTimestamp, qint64 endTimestamp);

    // 查询某一天所有字段平均值，区间为 [dayStartTimestamp, nextDayStartTimestamp)。
    QVariantMap queryDailyAverage(qint64 dayStartTimestamp, qint64 nextDayStartTimestamp);

    // 查询数据库中时间最久的一个星期记录；数据跨度不足 7 天时返回空列表。
    QVector<VEFCSensorMonitorRecord> queryOldestWeekRecords();

signals:
    // 本 DBCon 提交的单条 INSERT 已成功落库。
    void recordInserted(const VEFCSensorMonitorRecord& record);

    // 本 DBCon 提交的批量 INSERT 已成功落库。
    void recordsInserted(const QVector<VEFCSensorMonitorRecord>& records);

private slots:
    void onWriteTaskCompleted(const WriteResult& result);

private:
    static VEFCSensorMonitorRecord recordFromParams(const QVariantList& params, int offset);
    static QVector<VEFCSensorMonitorRecord> recordsFromParams(const QVariantList& params);

    QThread* m_workerThread;
    VEFCSensorMonitorSqlLogic* m_sqlLogic;
    WriteSqlDBCon* m_writeCon;
};

} // namespace LogDB

#endif // VEFCSENSORMONITORDBCON_H
