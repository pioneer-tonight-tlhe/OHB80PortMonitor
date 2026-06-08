#include "dbconnectionhelper.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

namespace LogDB {

QSqlDatabase DBConnectionHelper::openSqlite(const QString& dbFilePath,
                                            const QString& connectionName,
                                            const Options& options)
{
    QSqlDatabase db;
    if (QSqlDatabase::contains(connectionName)) {
        db = QSqlDatabase::database(connectionName);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    }

    db.setDatabaseName(dbFilePath);

    if (!db.open()) {
        qWarning() << "[DBConnectionHelper] Failed to open database:"
                   << dbFilePath << "connection:" << connectionName
                   << "error:" << db.lastError().text();
        return db;
    }

    // 应用优化PRAGMA
    QSqlQuery query(db);
    if (options.enableWAL) {
        query.exec("PRAGMA journal_mode = WAL;");
    }
    if (options.synchronousNormal) {
        query.exec("PRAGMA synchronous = NORMAL;");
    }
    if (options.busyTimeoutMs > 0) {
        query.exec(QString("PRAGMA busy_timeout = %1;").arg(options.busyTimeoutMs));
    }

    return db;
}

void DBConnectionHelper::closeAndRemove(const QString& connectionName)
{
    if (!QSqlDatabase::contains(connectionName)) {
        return;
    }
    {
        QSqlDatabase db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace LogDB
