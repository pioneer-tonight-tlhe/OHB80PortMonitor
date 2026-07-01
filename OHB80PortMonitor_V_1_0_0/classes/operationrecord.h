/*******************************************************************************************
 * @file operationrecord.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class OperationRecord
 * @brief 定义与 `operation_log` 表字段一一对应的运行日志记录结构。
 *
 * 设计目标：
 *      1. 统一封装运行日志在 data、scheduler 和 UI 之间传递的记录字段。
 *      2. 为描述模板格式化保留轻量辅助接口，避免调度层重复拼接逻辑。
 *      3. 支持 Qt 元对象系统注册后跨线程通过信号槽传递。
 *******************************************************************************************/
#ifndef OPERATIONRECORD_H
#define OPERATIONRECORD_H

#include <QMetaType>
#include <QString>
#include <QStringList>

struct OperationRecord
{
    int id = 0;
    QString occurTime;
    int logType = 0;
    QString description;
    int userPermission = 0;
    QString m_descFormat;

    void setDescFormat(const QString& format)
    {
        m_descFormat = format;
    }

    void setDescription(const QStringList& args)
    {
        description = m_descFormat;
        for (const QString& arg : args) {
            description = description.arg(arg);
        }
    }

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
