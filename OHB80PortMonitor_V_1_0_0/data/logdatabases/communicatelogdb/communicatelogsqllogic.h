#ifndef COMMUNICATELOGSQLLOGIC_H
#define COMMUNICATELOGSQLLOGIC_H

#include <QByteArray>
#include <QObject>
#include <QSqlDatabase>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include "dbtypes.h"
#include "sqlmapper.h"

namespace LogDB {

class LogCleanupScheduler;

class CommunicateLogSqlLogic : public QObject
{
    Q_OBJECT

public:
    explicit CommunicateLogSqlLogic(const QString& databasePath, QObject* parent = nullptr);
    ~CommunicateLogSqlLogic();

public slots:
    bool initializeDatabase();

    bool insertRecord(const QString& sendTime,
                      const QString& responseTime,
                      const QString& commandId,
                      const QString& qrCode,
                      int execStatus,
                      int retryCount,
                      const QByteArray& sendFrame,
                      const QByteArray& responseFrame,
                      const QString& description,
                      int userPermission = 0);

    QList<QVariantMap> queryPageWithConditions(const QString& commandId,
                                               const QString& qrCode,
                                               int execStatus,
                                               int retryCount,
                                               const QString& startTime,
                                               const QString& endTime,
                                               int pageSize,
                                               int pageNumber,
                                               int sortOrder,
                                               int maxUserPermission);

    int queryTotalCount();

    int queryTotalCountWithConditions(const QString& commandId,
                                      const QString& qrCode,
                                      int execStatus,
                                      int retryCount,
                                      const QString& startTime,
                                      const QString& endTime,
                                      int maxUserPermission);

    QVariantMap queryMonthRange();

    bool deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    void writeExecuted(const WriteResult& result);

private:
    void initializeCleanupScheduler();
    int calculateOffset(int pageSize, int pageNumber);

    QString m_databasePath;
    QString m_connectionName;
    SqlMapper* m_sqlMapper;
    QSqlDatabase m_database;
    LogCleanupScheduler* m_cleanupScheduler;
};

} // namespace LogDB

#endif // COMMUNICATELOGSQLLOGIC_H
