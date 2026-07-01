/*******************************************************************************************
 * @file communicaterecord.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class CommunicateRecord
 * @brief 定义与 `communicate_log` 表字段一一对应的通讯日志记录结构。
 *
 * 设计目标：
 *      1. 统一封装通讯日志在 data、scheduler 和 UI 之间传递的记录字段。
 *      2. 保留帧数据、执行状态和权限字段，便于历史查询与实时显示复用。
 *      3. 支持 Qt 元对象系统注册后跨线程通过信号槽传递。
 *******************************************************************************************/
#ifndef COMMUNICATERECORD_H
#define COMMUNICATERECORD_H

#include <QByteArray>
#include <QMetaType>
#include <QString>

struct CommunicateRecord
{
    int id = 0;
    QString sendTime;
    QString responseTime;
    QString commandId;
    QString qrCode;
    int execStatus = 0;
    int retryCount = 0;
    QByteArray sendFrame;
    QByteArray responseFrame;
    QString description;
    int userPermission = 0;

    void reset()
    {
        id = 0;
        sendTime.clear();
        responseTime.clear();
        commandId.clear();
        qrCode.clear();
        execStatus = 0;
        retryCount = 0;
        sendFrame.clear();
        responseFrame.clear();
        description.clear();
        userPermission = 0;
    }
};

Q_DECLARE_METATYPE(CommunicateRecord)

#endif // COMMUNICATERECORD_H
