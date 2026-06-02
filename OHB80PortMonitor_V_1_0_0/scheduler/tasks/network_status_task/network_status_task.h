#ifndef NETWORK_STATUS_TASK_H
#define NETWORK_STATUS_TASK_H

#include "../../scheduler_task.h"
#include "../init_check_task.h"
#include "network_status_task_logger.h"
#include "modbustcpmastermanager/modbustcpmaster/modbusconnecter.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QString>


// 网络连接状态监控任务（常驻任务）
// 监听所有 ModbusTcpMaster 的 ModbusConnecter::statusChanged 信号，
// 1. 任务初始阶段，启动自动重连
// 2. 时刻监控连接状态
// 3. 如果状态发生变化，根据 masterId 获取对应的 FoupOfOHBInfo 并设置告警
class NetworkStatusTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit NetworkStatusTask(QObject *parent = nullptr);
    ~NetworkStatusTask();

    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return "NetworkStatusTask"; }
    bool isPersistent() const override { return true; }

signals:
    // 转发 InitCheckTask 的初始化汇总结果
    void allInitFinished(bool allSuccess, int successCount, int failCount,
                         const QStringList &failedMasterIds);
    // 网络状态变更信号
    void statusChanged(ModbusConnecter::ConnectionStatus status, const QString &masterId);

private slots:
    // 单个设备连接状态变更回调
    void onStatusChanged(ModbusConnecter::ConnectionStatus status, const QString &masterId);
    // InitCheckTask 完成回调
    void onInitCheckFinished(bool allSuccess, int successCount, int failCount,
                             const QStringList &failedMasterIds);
    // WriteQRCode 指令响应回调
    void onWriteQRCodeFinished(ModbusCommand cmd, const QString &masterId);

private:
    void disconnectAll();
    void submitWriteQRCode(const QString &masterId, const QString &connectedLogContext = QString());
    void logWriteQRCodeSubmitFailure(const QString &masterId,
                                     const QString &connectedLogContext,
                                     const QString &reason);

    int m_totalCount = 0;
    bool m_stopped = false;

    // 记录每个设备上一次的连接状态
    QHash<QString, ModbusConnecter::ConnectionStatus> m_lastStatusMap;
    QHash<QString, bool> m_offlineReportedMap;
    QList<QMetaObject::Connection> m_connections;

    // 待处理的 WriteQRCode 指令：uuid -> masterId
    QHash<qint64, QString> m_writeQRCodePendingMap;
    QHash<qint64, QString> m_writeQRCodeContextMap;

    InitCheckTask *m_initCheckTask = nullptr;
    NetworkStatusTaskLogger *m_logger = nullptr;
};

#endif // NETWORK_STATUS_TASK_H
