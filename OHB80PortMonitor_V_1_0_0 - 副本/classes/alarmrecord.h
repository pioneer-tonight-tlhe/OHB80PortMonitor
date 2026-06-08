#ifndef ALARMRECORD_H
#define ALARMRECORD_H

#include <QString>
#include <QMetaType>

// ====================================================================
// AlarmRecord —— 与 alarm_log 表结构对齐的纯数据记录类
//
// 表结构：
//   id INTEGER PRIMARY KEY AUTOINCREMENT,
//   alarm_level INTEGER NOT NULL,
//   occur_time TEXT NOT NULL,
//   qr_code TEXT NOT NULL,
//   alarm_type TEXT NOT NULL,
//   is_resolved INTEGER NOT NULL DEFAULT 2 CHECK(is_resolved IN (0, 1, 2)),
//   resolve_time TEXT,
//   description TEXT NOT NULL,
//   user_permission INTEGER NOT NULL DEFAULT 0
//
// 说明：
//   - 跨线程通过信号/槽传递时，需先在 MetaTypes 中注册（已注册）。
// ====================================================================
struct AlarmRecord
{
    int     id              = 0;     // 主键
    int     alarmLevel      = 0;     // 警报级别（AlarmLevel）
    QString occurTime;               // 发生时间 "yyyy-MM-dd HH:mm:ss"
    QString qrCode;                  // 来源标识（设备 qrCode 或用户名等）
    int     alarmType       = 0;     // 警报类型（AlarmType 枚举）；落库时 to QString
    int     isResolved      = 2;     // 0=未解决, 1=已解决, 2=Warn 无需解决
    QString resolveTime;             // 解决时间
    QString description;             // 描述
    int     userPermission  = 0;     // 触发该警报的用户权限（UserPermission）

    void reset()
    {
        id = 0;
        alarmLevel = 0;
        occurTime.clear();
        qrCode.clear();
        alarmType = 0;
        isResolved = 2;
        resolveTime.clear();
        description.clear();
        userPermission = 0;
    }
};

Q_DECLARE_METATYPE(AlarmRecord)

#endif // ALARMRECORD_H
