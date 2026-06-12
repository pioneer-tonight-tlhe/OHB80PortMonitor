#ifndef NETWORK_STATUS_TASK_H
#define NETWORK_STATUS_TASK_H

#include "../../scheduler_task.h"
#include "../init_check_task/init_check_task.h"
#include "network_status_task_logger.h"
#include "modbustcpmastermanager/modbustcpmaster/modbusconnecter.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QString>

class QRCodeWriteLogger; // 前向声明：QRCode 写入专用日志

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
    // 断开并清理所有已建立的状态监听连接
    void disconnectAll(); 
    // 连接成功后下发 WriteQRCode 指令，附带上下文日志
    void submitWriteQRCode(const QString &masterId, const QString &connectedLogContext = QString()); 
    // 记录 WriteQRCode 下发失败原因（如 sender/指令池不可用等）
    void logWriteQRCodeSubmitFailure(const QString &masterId,
                                     const QString &connectedLogContext,
                                     const QString &reason); 
    // 启动初始化检查任务并连接完成回调
    void startInitCheckTask(); 
    // 为所有设备启动自动重连（并输出统计日志）
    void startAutoReconnect(const QStringList &ids); 
    // 统计初始连接状态并挂接 statusChanged 信号；四个输出参数依次为：已连接/连接中/已断开/错误 数量
    void processInitialStatusAndConnectSignals(const QStringList &ids,
                                               int &initialConnectedCount,
                                               int &initialConnectingCount,
                                               int &initialDisconnectedCount,
                                               int &initialErrorCount);

    int m_totalCount = 0;   // 监听的设备数量（已成功挂接状态监听的设备数）
    bool m_stopped = false; // 任务停止标志（stop() 后为 true，用于屏蔽后续回调处理）

    // 记录每个设备上一次的连接状态
    QHash<QString, ModbusConnecter::ConnectionStatus> m_lastStatusMap;
    QHash<QString, bool> m_offlineReportedMap;
    QList<QMetaObject::Connection> m_connections;
    int m_statusConnCount = 0;   // statusChanged 连接数量
    int m_qrConnCount = 0;       // WriteQRCode commandFinished 连接数量

    // 待处理的 WriteQRCode 指令：uuid -> masterId
    QHash<qint64, QString> m_writeQRCodePendingMap;
    QHash<qint64, QString> m_writeQRCodeContextMap;

    // 初始下发指令任务
    InitCheckTask *m_initCheckTask = nullptr;
    NetworkStatusTaskLogger *m_logger = nullptr;
    QRCodeWriteLogger *m_qrcodeLogger = nullptr; // QRCode 写入专用日志
};

#endif // NETWORK_STATUS_TASK_H
