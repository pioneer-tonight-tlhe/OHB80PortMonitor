/*******************************************************************************************
 * @file dbtypes.h
 * @brief 定义日志数据库模块共用的枚举、轻量记录结构和写入结果类型。
 * @author Simon <工号：13> 2026-07-01
 *
 * 设计目标：
 *      1. 统一保存警报、通讯和运行日志查询链路共用的基础类型定义。
 *      2. 为 UI、scheduler 和 data 层提供一致的日志类型和排序语义。
 *      3. 减少跨模块重复定义，保持写入结果和查询辅助结构的单一来源。
 *******************************************************************************************/
#ifndef DBTYPES_H
#define DBTYPES_H

#include <QList>
#include <QMetaType>
#include <QPair>
#include <QString>
#include <QVariant>

namespace LogDB {

enum class SortOrder : int
{
    Asc = 0,
    Desc = 1
};

enum class OperationLogType : int
{
    Message = 0,
    Warn = 1,
    Error = 2,
    Fatal = 3
};

inline QString operationLogTypeName(int logType)
{
    switch (logType) {
        case static_cast<int>(OperationLogType::Message):
            return QStringLiteral("Message");
        case static_cast<int>(OperationLogType::Warn):
            return QStringLiteral("Warn");
        case static_cast<int>(OperationLogType::Error):
            return QStringLiteral("Error");
        case static_cast<int>(OperationLogType::Fatal):
            return QStringLiteral("Fatal");
        default:
            return QString::number(logType);
    }
}

inline QList<QPair<QString, int>> operationLogTypeList()
{
    return {
        {QStringLiteral("Message"), static_cast<int>(OperationLogType::Message)},
        {QStringLiteral("Warn"), static_cast<int>(OperationLogType::Warn)},
        {QStringLiteral("Error"), static_cast<int>(OperationLogType::Error)},
        {QStringLiteral("Fatal"), static_cast<int>(OperationLogType::Fatal)},
    };
}

struct OperationLogRecordInfo
{
    int id = 0;
    QString occurTime;
    int logType = 0;
    QString description;
};

enum class WriteOp : int
{
    Other = 0,
    Insert = 1,
    Delete = 2
};

struct WriteResult
{
    QString connectionName;
    QString result;
    QString sqlStatement;
    QString sqlId;
    QVariantList params;
    QString tableName;
    int opType = static_cast<int>(WriteOp::Other);
};

} // namespace LogDB

Q_DECLARE_METATYPE(LogDB::WriteResult)

#endif // DBTYPES_H
