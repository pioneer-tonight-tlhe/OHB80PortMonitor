#ifndef COMMUNICATERECORD_H
#define COMMUNICATERECORD_H

#include <QString>
#include <QByteArray>
#include <QMetaType>

// ====================================================================
// CommunicateRecord —— 与 communicate_log 表结构对齐的纯数据记录类
//
// 表结构：
//   id INTEGER PRIMARY KEY AUTOINCREMENT,
//   send_time TEXT NOT NULL,
//   response_time TEXT,
//   command_id TEXT NOT NULL,
//   qr_code TEXT NOT NULL,
//   exec_status INTEGER NOT NULL,
//   retry_count INTEGER NOT NULL,
//   send_frame BLOB,
//   response_frame BLOB,
//   description TEXT,
//   user_permission INTEGER NOT NULL DEFAULT 0
// ====================================================================
struct CommunicateRecord
{
    int        id              = 0;     // 主键
    QString    sendTime;               // 发送时间 "yyyy-MM-dd HH:mm:ss"
    QString    responseTime;           // 响应时间 "yyyy-MM-dd HH:mm:ss"
    QString    commandId;              // 命令ID
    QString    qrCode;                 // 设备二维码
    int        execStatus      = 0;     // 执行状态（ExecStatus：0=Success, 1=Failure, 2=Timeout）
    int        retryCount      = 0;     // 重试次数
    QByteArray sendFrame;              // 发送帧（BLOB）
    QByteArray responseFrame;          // 响应帧（BLOB）
    QString    description;            // 描述
    int        userPermission  = 0;     // 触发该通讯的用户权限（UserPermission）

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
