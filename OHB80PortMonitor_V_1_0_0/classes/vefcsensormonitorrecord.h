#ifndef VEFCSENSORMONITORRECORD_H
#define VEFCSENSORMONITORRECORD_H

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVector>

// ====================================================================
// VEFCSensorMonitorRecord —— 与 vefc_sensor_monitor 表结构对齐的纯数据记录类
//
// 表结构：
//   qr_code TEXT NOT NULL,
//   record_timestamp INTEGER NOT NULL,
//   gas_pressure REAL NOT NULL,
//   actual_flow REAL NOT NULL,
//   sensor_pressure REAL NOT NULL,
//   sensor_temperature REAL NOT NULL,
//   PRIMARY KEY (qr_code, record_timestamp)
//
// 说明：
//   - recordTimestamp 使用毫秒时间戳存储，展示时再转换成年月日时分秒毫秒。
//   - 跨线程通过信号/槽传递时，需先在 MetaTypes 中注册。
// ====================================================================
struct VEFCSensorMonitorRecord
{
    QString qrCode;                 // QRCode（设备标识）
    qint64  recordTimestamp = 0;    // 记录时间戳（毫秒）
    double  gasPressure = 0.0;      // 气体压力
    double  actualFlow = 0.0;       // 实际流量
    double  sensorPressure = 0.0;   // 传感器压力
    double  sensorTemperature = 0.0;// 传感器温度

    QString recordTimeString(const QString& format = QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")) const
    {
        if (recordTimestamp <= 0) {
            return QString();
        }
        return QDateTime::fromMSecsSinceEpoch(recordTimestamp).toString(format);
    }

    void reset()
    {
        qrCode.clear();
        recordTimestamp = 0;
        gasPressure = 0.0;
        actualFlow = 0.0;
        sensorPressure = 0.0;
        sensorTemperature = 0.0;
    }
};

Q_DECLARE_METATYPE(VEFCSensorMonitorRecord)
Q_DECLARE_METATYPE(QVector<VEFCSensorMonitorRecord>)

#endif // VEFCSENSORMONITORRECORD_H
