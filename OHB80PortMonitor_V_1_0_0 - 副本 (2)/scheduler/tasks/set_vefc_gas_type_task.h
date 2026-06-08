#ifndef SET_VEFC_GAS_TYPE_TASK_H
#define SET_VEFC_GAS_TYPE_TASK_H

#include "../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QTimer>
#include <QVector>


class OperationDispatchTask;

// ====================================================================
// SetVEFCGasTypeTask — 写入 VEFC 气体介质类型（掉电保持）
//
//   底层指令：WriteVEFCGasType（FC 0x06, addr 0x0001）
//   寄存器值（枚举）：
//     0x0000 = CDA  0x0001 = N2  0x0002 = Ar  0x0003 = CO2  0x0004 = O2
//
//   信号 allFinished：
//     allSuccess     — 是否全部成功
//     successCount   — 成功设备数
//     failedQrCodes  — 失败设备 QRCode 列表
//     gasType        — 设置的气体类型枚举值
// ====================================================================
class SetVEFCGasTypeTask : public SchedulerTask
{
    Q_OBJECT

public:
    enum GasType {
        CDA = 0x0000,
        N2  = 0x0001,
        Ar  = 0x0002,
        CO2 = 0x0003,
        O2  = 0x0004
    };
    Q_ENUM(GasType)

    explicit SetVEFCGasTypeTask(const QVector<QString> &qrcodes,
                                int gasType,
                                QObject *parent = nullptr);
    ~SetVEFCGasTypeTask();

    void start() override;
    void stop()  override;
    QString taskType() const override { return "SetVEFCGasTypeTask"; }

signals:
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     int gasType);

    // 设备指令超时重试信号（通知 UI 层显示重试状态）
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);
    void onTimeout();

private:
    QByteArray buildRegisterValue(quint16 value) const;
    void disconnectAll();
    void checkAllFinished();
    void forceFinish();
    void logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode);
    void writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    QString subFunctionName() const;
    ILogger& deviceDetailLogger();

private:
    QVector<QString> m_qrcodes;
    int              m_gasType;

    QHash<qint64, QString>         m_pendingMap;
    QList<QMetaObject::Connection> m_connections;
    int        m_totalCount = 0;
    QAtomicInt m_completedCount{0};
    bool       m_stopped = false;

    int         m_successCount = 0;
    QStringList m_failedQrCodes;

    QTimer *m_timeoutTimer       = nullptr;
    bool    m_allFinishedEmitted = false;

    ILogger deviceLogger;
    bool m_loggerInitialized = false;
};

#endif // SET_VEFC_GAS_TYPE_TASK_H
