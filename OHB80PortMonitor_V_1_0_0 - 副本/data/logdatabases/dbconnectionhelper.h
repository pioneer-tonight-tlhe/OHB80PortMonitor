#ifndef DBCONNECTIONHELPER_H
#define DBCONNECTIONHELPER_H

#include <QSqlDatabase>
#include <QString>

namespace LogDB {

// SQLite数据库连接通用工具
// 提供创建/打开/优化/关闭连接的统一接口，方便各逻辑类复用
class DBConnectionHelper
{
public:
    struct Options {
        bool enableWAL;          // 启用WAL日志模式
        bool synchronousNormal;  // synchronous=NORMAL（提高写入性能）
        int busyTimeoutMs;       // busy timeout（毫秒），0表示不设置

        Options()
            : enableWAL(true)
            , synchronousNormal(true)
            , busyTimeoutMs(5000)
        {}
    };

    // 打开一个SQLite连接（使用QSQLITE驱动）
    // dbFilePath: 数据库文件完整路径
    // connectionName: Qt连接名（同名连接已存在则复用）
    // options: 连接优化选项
    // 返回打开的QSqlDatabase；失败时返回的连接isOpen()为false
    static QSqlDatabase openSqlite(const QString& dbFilePath,
                                   const QString& connectionName,
                                   const Options& options = Options());

    // 关闭并移除指定的Qt数据库连接
    // 注意：调用前应确保该连接的所有QSqlQuery对象已销毁
    static void closeAndRemove(const QString& connectionName);
};

} // namespace LogDB

#endif // DBCONNECTIONHELPER_H
