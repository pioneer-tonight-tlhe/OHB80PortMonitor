#ifndef DEVICEPARAMRECORD_H
#define DEVICEPARAMRECORD_H

#include <QString>
#include <QMetaType>

// ====================================================================
// DeviceParamRecord —— 与 device_param_log 表结构对齐的纯数据记录类
//
// 表结构：
//   id INTEGER PRIMARY KEY AUTOINCREMENT,
//   qr_code TEXT NOT NULL,
//   record_time TEXT NOT NULL,
//   inlet_pressure REAL NOT NULL,
//   outlet_pressure REAL NOT NULL,
//   inlet_flow REAL NOT NULL,
//   humidity REAL NOT NULL,
//   temperature REAL NOT NULL,
//   foup_status INTEGER NOT NULL CHECK(foup_status IN (0, 1)),
//   user_permission INTEGER NOT NULL DEFAULT 0
// ====================================================================
struct DeviceParamRecord
{
    int     id              = 0;     // 主键
    QString qrCode;                  // 设备二维码
    QString recordTime;              // 记录时间 "yyyy-MM-dd HH:mm:ss"
    double  inletPressure   = 0.0;   // 入口压力
    double  outletPressure  = 0.0;   // 出口压力
    double  inletFlow       = 0.0;   // 入口流量
    double  humidity        = 0.0;   // 湿度
    double  temperature     = 0.0;   // 温度
    int     foupStatus      = 0;     // FOUP 状态（0=out, 1=in）
    int     userPermission  = 0;     // 触发该采样的用户权限（UserPermission）

    void reset()
    {
        id = 0;
        qrCode.clear();
        recordTime.clear();
        inletPressure = 0.0;
        outletPressure = 0.0;
        inletFlow = 0.0;
        humidity = 0.0;
        temperature = 0.0;
        foupStatus = 0;
        userPermission = 0;
    }
};

Q_DECLARE_METATYPE(DeviceParamRecord)

#endif // DEVICEPARAMRECORD_H
