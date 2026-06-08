#ifndef COMMUNICATELOGDBCON_H
#define COMMUNICATELOGDBCON_H

#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <QByteArray>
#include "communicatelogsqllogic.h"
#include "writesqldbcon.h"
#include "communicaterecord.h"

namespace LogDB {

class CommunicateLogDBCon : public QObject
{
    Q_OBJECT

public:
    // 必须传入外部 WriteSqlDBCon，本类不拥有其生命周期
    CommunicateLogDBCon(const QString& databasePath, WriteSqlDBCon* externalWriteCon, QObject* parent = nullptr);
    ~CommunicateLogDBCon();

    // 禁用默认构造、拷贝、赋值
    CommunicateLogDBCon() = delete;
    CommunicateLogDBCon(const CommunicateLogDBCon&) = delete;
    CommunicateLogDBCon& operator=(const CommunicateLogDBCon&) = delete;

    // 初始化
    bool initialize();
    void cleanup();

    // 查询接口
    // sortOrder 控制按 send_time 的排序方向，默认降序（最新在前）
    QList<CommunicateRecord> queryPageWithConditions(const QString& commandId,
                                                   const QString& qrCode,
                                                   int execStatus,
                                                   int retryCount,
                                                   const QString& startTime,
                                                   const QString& endTime,
                                                   int pageSize,
                                                   int pageNumber,
                                                   SortOrder sortOrder = SortOrder::Desc);

    int queryTotalCount();

    int queryTotalCountWithConditions(const QString& commandId,
                                      const QString& qrCode,
                                      int execStatus,
                                      int retryCount,
                                      const QString& startTime,
                                      const QString& endTime);

    int queryMonthRange();

    // 查询数据库中 send_time 的最早 / 最晚时间。
    // 输出为 "yyyy-MM-dd HH:mm:ss" 格式；表为空时返回两个空字符串。
    void queryTimeBounds(QString& earliestTime, QString& latestTime);

    // 插入接口
    // userPermission: 触发该通讯的用户权限级别（UserPermission 枚举），
    //                 默认 0（UserPermission::Guest），兼容旧调用方
    void insertRecord(const QString& sendTime,
                      const QString& responseTime,
                      const QString& commandId,
                      const QString& qrCode,
                      int execStatus,
                      int retryCount,
                      const QByteArray& sendFrame,
                      const QByteArray& responseFrame,
                      const QString& description,
                      int userPermission = 0);

    // 删除接口
    void deleteByTimeRange(const QString& startTime, const QString& endTime);

signals:
    // 实时事件：本 DBCon 提交的 INSERT 已成功落库
    // 携带 CommunicateRecord（包含 communicate_log 表所有字段）
    void recordInserted(const CommunicateRecord& record);

private slots:
    void onWriteTaskCompleted(const WriteResult& result);

private:
    QThread* m_workerThread;
    CommunicateLogSqlLogic* m_sqlLogic;
    WriteSqlDBCon* m_writeCon;
};

} // namespace LogDB

#endif // COMMUNICATELOGDBCON_H
