#ifndef DBTYPES_H
#define DBTYPES_H

#include <QString>
#include <QVariant>
#include <QMetaType>
#include <QList>
#include <QPair>

namespace LogDB {

// 查询结果排序方向
enum class SortOrder : int
{
    Asc = 0,
    Desc = 1
};

// 运行日志类型枚举
enum class OperationLogType : int
{
    Message = 0,      // 消息
    Warn = 1,         // 警告
    Error = 2,        // 错误
    Fatal = 3         // 致命错误
};

// 获取运行日志类型的显示名称
inline QString operationLogTypeName(int logType)
{
    switch (logType) {
        case static_cast<int>(OperationLogType::Message): return QStringLiteral("Message");
        case static_cast<int>(OperationLogType::Warn):    return QStringLiteral("Warn");
        case static_cast<int>(OperationLogType::Error):   return QStringLiteral("Error");
        case static_cast<int>(OperationLogType::Fatal):   return QStringLiteral("Fatal");
        default: return QString::number(logType);
    }
}

// 获取所有运行日志类型的(名称, 值)列表，便于 UI 填充下拉框
inline QList<QPair<QString, int>> operationLogTypeList()
{
    return {
        { QStringLiteral("Message"), static_cast<int>(OperationLogType::Message) },
        { QStringLiteral("Warn"),    static_cast<int>(OperationLogType::Warn)    },
        { QStringLiteral("Error"),   static_cast<int>(OperationLogType::Error)   },
        { QStringLiteral("Fatal"),   static_cast<int>(OperationLogType::Fatal)   },
    };
}

// 运行日志记录信息
struct OperationLogRecordInfo
{
    int id;                     // 记录ID
    QString occurTime;          // 记录时间
    int logType;                // 日志类型 (OperationLogType)
    QString description;        // 描述内容
};

// 写入操作类型（用于驱动 log_record_count 表的更新）
enum class WriteOp : int
{
    Other  = 0,
    Insert = 1,
    Delete = 2
};

// 写入语句结果结构体
struct WriteResult
{
    QString connectionName;
    QString result;
    QString sqlStatement;
    QString sqlId;
    QVariantList params;
    // 事务 & 计数支持：目标日志表（如 "operation_log"）与操作类型
    QString tableName;
    int opType = static_cast<int>(WriteOp::Other);
};

} // namespace LogDB

Q_DECLARE_METATYPE(LogDB::WriteResult)

#endif // DBTYPES_H
