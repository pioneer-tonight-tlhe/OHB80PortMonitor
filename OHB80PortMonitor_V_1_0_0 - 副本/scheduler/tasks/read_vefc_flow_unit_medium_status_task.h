#ifndef READ_VEFC_FLOW_UNIT_MEDIUM_STATUS_TASK_H
#define READ_VEFC_FLOW_UNIT_MEDIUM_STATUS_TASK_H

#include "../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include <string>


class OperationDispatchTask;

// ====================================================================
// ReadVEFCFlowUnitAndMediumStatusTask — 读取 VEFC 流量单位 / 介质配置状态
//
//   底层指令：ReadVEFCFlowUnitAndMediumStatus（FC 0x04, addr 0x0011）
//   响应 2 字节寄存器：
//     hi_byte: 0=单位配置成功 / 1=单位配置失败（默认 L/Min）
//     lo_byte: 0=介质配置成功 / 1=介质配置失败（默认 CDA）
//
//   信号 allFinished 携带 QList<DeviceStatus>：
//     每台设备的读取结果（含通信失败标志、单位/介质各自的成功/失败）
// ====================================================================
class ReadVEFCFlowUnitAndMediumStatusTask : public SchedulerTask
{
    Q_OBJECT

public:
    // 单台设备的读取结果
    struct DeviceStatus {
        QString qrcode;
        bool    commFailed   = false; // true → 未收到响应 / 校验错 / 设备不可用
        bool    unitOk       = false; // hi_byte == 0
        bool    mediumOk     = false; // lo_byte == 0
        quint8  unitRaw      = 0;
        quint8  mediumRaw    = 0;

        bool allOk() const { return !commFailed && unitOk && mediumOk; }
    };

    explicit ReadVEFCFlowUnitAndMediumStatusTask(const QVector<QString> &qrcodes,
                                                 QObject *parent = nullptr);
    ~ReadVEFCFlowUnitAndMediumStatusTask();

    void start() override;
    void stop()  override;
    QString taskType() const override { return "ReadVEFCFlowUnitAndMediumStatusTask"; }

signals:
    void allFinished(bool allSuccess,
                     int successCount,
                     QList<ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus> results);

    // 设备指令超时重试信号（通知 UI 层显示重试状态）
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);

private:
    void disconnectAll();
    void checkAllFinished();
    void forceFinish();
    void logFailedDevice(OperationDispatchTask* opTask, const QString& id, const DeviceStatus& st);
    void writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    QString subFunctionName() const;
    ILogger& deviceDetailLogger();

private:
    QVector<QString> m_qrcodes;

    QHash<qint64, QString>         m_pendingMap;     // uuid → qrcode
    QList<QMetaObject::Connection> m_connections;
    QHash<QString, DeviceStatus>   m_resultMap;      // qrcode → 结果

    int        m_totalCount = 0;
    QAtomicInt m_completedCount{0};
    bool       m_stopped = false;

    bool    m_allFinishedEmitted = false;

    ILogger deviceLogger;
    bool m_loggerInitialized = false;
};

Q_DECLARE_METATYPE(ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus)
Q_DECLARE_METATYPE(QList<ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus>)

#endif // READ_VEFC_FLOW_UNIT_MEDIUM_STATUS_TASK_H
