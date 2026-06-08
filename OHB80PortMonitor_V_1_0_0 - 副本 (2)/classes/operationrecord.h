#ifndef OPERATIONRECORD_H
#define OPERATIONRECORD_H

#include <QString>
#include <QMetaType>

// ====================================================================
// OperationRecord —— 与 operation_log 表结构对齐的纯数据记录类
//
// 表结构：
//   id INTEGER PRIMARY KEY AUTOINCREMENT,
//   occur_time TEXT NOT NULL,
//   log_type INTEGER NOT NULL,
//   description TEXT NOT NULL,
//   user_permission INTEGER NOT NULL DEFAULT 0
// ====================================================================
struct OperationRecord
{
    int     id              = 0;     // 主键
    QString occurTime;               // 发生时间 "yyyy-MM-dd HH:mm:ss"
    int     logType         = 0;     // 日志类型（LogType：0=Message, 1=Warn, 2=Error）
    QString description;             // 操作描述
    int     userPermission  = 0;     // 触发该操作的用户权限（UserPermission）

    // 描述格式字符串（用于格式化 description）
    QString m_descFormat;

    // 设置描述格式
    void setDescFormat(const QString& format) { m_descFormat = format; }

    // 使用格式字符串和参数列表设置 description
    void setDescription(const QStringList& args)
    {
        description = m_descFormat;
        for (const QString& arg : args) {
            description = description.arg(arg);
        }
    }

    // 便捷重载：支持 1-3 个参数
    void setDescription(const QString& arg1)
    {
        setDescription(QStringList{arg1});
    }

    void setDescription(const QString& arg1, const QString& arg2)
    {
        setDescription(QStringList{arg1, arg2});
    }

    void setDescription(const QString& arg1, const QString& arg2, const QString& arg3)
    {
        setDescription(QStringList{arg1, arg2, arg3});
    }

    void resetDescFormat()
    {
        id = 0;
        occurTime.clear();
        logType = 0;
        description.clear();
        userPermission = 0;
        m_descFormat.clear();
    }
};

Q_DECLARE_METATYPE(OperationRecord)

#endif // OPERATIONRECORD_H
