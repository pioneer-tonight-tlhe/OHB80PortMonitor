#ifndef SEND_COMMAND_TASK_H
#define SEND_COMMAND_TASK_H

#include "../../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QVariant>
#include <QVector>

class SendCommandTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SendCommandTask(QObject *parent = nullptr);
    ~SendCommandTask();

    // 设置：给指定 QRCode 列表的设备下发一条业务指令
    void setSendToDevices(const QVector<QString> &qrcodes,
                          const QString &commandName,
                          const QVector<quint16> &params = {});

    // 设置：给所有已注册的设备下发一条业务指令
    void setSendToAll(const QString &commandName,
                      const QVector<quint16> &params = {});

    // SchedulerTask 接口
    void start() override;
    void stop() override;
    QString taskType() const override { return "SendCommandTask"; }

signals:
    // 单设备响应结果
    void dataResult(const QString &qrcode, const ModbusCommand &cmd);

    // 全部设备完成后的汇总信号
    void allFinished(bool allSuccess, int successCount, int failCount,
                     const QStringList &failedIds);

    // 设备指令超时重试信号（通知 UI 层显示重试状态）
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    // 单设备指令完成回调（通过 lambda 捕获 masterId 传入）
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);

private:
    // 根据 quint16 参数构建请求帧的 registerValue（大端序）
    QByteArray buildRegisterValue(const QVector<quint16> &params) const;

    // 断开所有已建立的信号连接
    void disconnectAll();

    // 完成一台设备后检查是否全部结束
    void checkAllFinished();

    void writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    static QString safeLogPathSegment(const QString& value);
    ILogger& deviceDetailLogger();
    QString subFunctionName() const;

    // ---- 配置 ----
    QVector<QString> m_targetQrcodes;   // 为空时发给所有设备
    QString          m_commandName;
    QVector<quint16> m_params;
    bool             m_sendToAll = false;

    // ---- 执行状态 ----
    // uuid → masterId（QRCode）
    QHash<qint64, QString>           m_pendingMap;
    QList<QMetaObject::Connection>   m_connections;

    int        m_totalCount = 0;
    QAtomicInt m_completedCount{0};
    bool       m_stopped  = false;

    // ---- 汇总结果 ----
    int                         m_resultSuccessCount = 0;
    QStringList                 m_resultFailedIds;

    ILogger deviceLogger;               ///< 设备详细日志记录器（subFunction 级共享）
    bool m_loggerInitialized = false;   ///< deviceLogger 路径是否已设置
};

#endif // SEND_COMMAND_TASK_H
