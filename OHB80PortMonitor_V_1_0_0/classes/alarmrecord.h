/*******************************************************************************************
 * @file alarmrecord.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class AlarmRecord
 * @brief 定义与 `alarm_log` 表字段一一对应的警报日志记录结构。
 *
 * 设计目标：
 *      1. 统一封装警报日志在 data、scheduler 和 UI 之间传递的记录字段。
 *      2. 保持结构字段命名与业务语义对应，减少 QVariantMap 的弱类型使用。
 *      3. 支持 Qt 元对象系统注册后跨线程通过信号槽传递。
 *******************************************************************************************/
#ifndef ALARMRECORD_H
#define ALARMRECORD_H

#include <QMetaType>
#include <QString>

struct AlarmRecord
{
    int id = 0;
    int alarmLevel = 0;
    QString occurTime;
    QString qrCode;
    int alarmType = 0;
    int isResolved = 2;
    QString resolveTime;
    QString description;
    int userPermission = 0;

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
