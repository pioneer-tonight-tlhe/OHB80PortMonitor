#ifndef SET_PURGE_FLOW_TASK_H
#define SET_PURGE_FLOW_TASK_H

#include "../../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QVector>


class OperationDispatchTask;

// ====================================================================
// SetPurgeFlowTask — 设置 Purge 流量大小（VEFC 流量）
//
//   底层指令：WritePurgeFlow（FC 0x06, addr 0x0000）
//   寄存器值 = flow × 10（例：35 → 350）
//   说明：仅 FOUP IN 时有效，FOUP OUT 状态固定为 0
//
//   信号 allFinished：
//     allSuccess     — 是否全部成功
//     successCount   — 成功设备数
//     failedQrCodes  — 失败设备 QRCode 列表
//     flowValue      — 设置的流量值（原始整数）
// ====================================================================
class SetPurgeFlowTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SetPurgeFlowTask(const QVector<QString> &qrcodes,
                              int flowValue,
                              QObject *parent = nullptr);
    ~SetPurgeFlowTask();

    void start() override;
    void stop()  override;
    QString taskType() const override { return "SetPurgeFlowTask"; }

    // 寄存器值倍率：flow × kRegisterScale 写入设备
    static constexpr int kRegisterScale = 10;

signals:
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     int flowValue);

    // 设备指令超时重试信号（通知 UI 层显示重试状态）
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);

private:
    QByteArray buildRegisterValue(quint16 value) const;
    void disconnectAll();
    void checkAllFinished();
    void forceFinish();
    bool persistConfig(QString *errorMessage);
    void logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode);

    void writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    static QString safeLogPathSegment(const QString& value);
    ILogger& deviceDetailLogger();
    QString subFunctionName() const;

private:
    QVector<QString> m_qrcodes;
    int              m_flowValue;

    QHash<qint64, QString>         m_pendingMap;
    QList<QMetaObject::Connection> m_connections;
    int        m_totalCount = 0;
    QAtomicInt m_completedCount{0};
    bool       m_stopped = false;

    int         m_successCount = 0;
    QStringList m_failedQrCodes;

    bool    m_allFinishedEmitted = false;

    QStringList m_targetQrCodes;  // 本次任务的目标设备列表（用于边界日志）

    ILogger deviceLogger;               ///< 设备详细日志记录器（subFunction 级共享）
    bool m_loggerInitialized = false;   ///< deviceLogger 路径是否已设置
};

#endif // SET_PURGE_FLOW_TASK_H
