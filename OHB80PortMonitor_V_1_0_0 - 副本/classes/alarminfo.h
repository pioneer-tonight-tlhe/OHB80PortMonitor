#ifndef ALARMINFO_H
#define ALARMINFO_H

#include <QString>
#include <QMetaType>
#include "alarmtype.h"
#include "alarmrecord.h"


// ====================================================================
// AlarmInfo —— 警报业务信息
//
// 内部组合 AlarmRecord（数据库行字段），并在其之上扩展业务字段：
//   - alarmSource:  警报来源（Device/User/System），不入库
//   - alarmId:      由级别 + 来源标识 + 类型生成的 10 位字符串，不入库
//
// 与 AlarmRecord 重合的字段（id / alarmLevel / alarmType / qrCode /
// occurTime / resolveTime / isResolved / description / userPermission）
// 全部由 record 持有，业务侧请通过 info.record.xxx 访问。
// ====================================================================
struct AlarmInfo
{
    AlarmRecord record;              // DB 行字段（id/level/type/qrCode/...）

    int     alarmSource = 0;         // 警报来源（AlarmSource: Device=1,User=2,System=3）
    QString alarmId;                 // 警报ID（generateAlarmId() 生成）

    AlarmInfo() = default;

    // 生成警报ID
    // 格式：【警报级别(1位)】+【来源标识(5位)】+【警告类型(4位)】
    QString generateAlarmId() const;

    // 重置所有字段
    void reset()
    {
        record.reset();
        alarmSource = 0;
        alarmId.clear();
    }

    // 检查是否有效
    bool isValid() const
    {
        return record.id > 0 && record.alarmLevel > 0
            && record.alarmType > 0 && alarmSource > 0;
    }
};

Q_DECLARE_METATYPE(AlarmInfo)

#endif // ALARMINFO_H
