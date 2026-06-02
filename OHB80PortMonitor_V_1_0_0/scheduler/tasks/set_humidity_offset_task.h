#ifndef SET_HUMIDITY_OFFSET_TASK_H
#define SET_HUMIDITY_OFFSET_TASK_H

#include "../scheduler_task.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "ilogger.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QTimer>
#include <QVector>


class OperationDispatchTask;

// ====================================================================
// SetHumidityOffsetTask — 设置湿度校准参数调度任务
//
//   构造函数仅传入目标设备列表，待执行的指令通过 setter 注入：
//     - setThreshold(thresholdPct)   下发 WriteHumidityOffsetThreshold (0x0019)
//     - setOffset(offsetPct)         下发 WriteHumidityOffset          (0x0018)
//
//   start() 时检查两个 setter 是否被调用，仅下发对应指令。
//   两个 setter 都未调用 → 任务直接以失败结束。
//
//   寄存器值 = 百分比 × 100（例：18% → 1800）
//
//   信号 allFinished：
//     allSuccess     — 是否全部成功
//     successCount   — 成功设备数
//     failedQrCodes  — 失败设备 QRCode 列表
//     thresholdSet   — 是否设置了 threshold
//     thresholdPct   — 设置的阈值（%，未设置则为 0）
//     offsetSet      — 是否设置了 offset
//     offsetPct      — 设置的 offset（%，未设置则为 0）
// ====================================================================
class SetHumidityOffsetTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SetHumidityOffsetTask(const QVector<QString> &qrcodes,
                                   QObject *parent = nullptr);
    ~SetHumidityOffsetTask();

    // 公开 setter — 仅在 start() 之前调用有效
    void setThreshold(double thresholdPct);
    void setOffset(double offsetPct);

    // SchedulerTask 接口
    void start() override;
    void stop()  override;
    QString taskType() const override { return "SetHumidityOffsetTask"; }

    // 寄存器值倍率：百分比 × kRegisterScale 写入设备
    static constexpr int kRegisterScale = 100;

signals:
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     bool thresholdSet, double thresholdPct,
                     bool offsetSet,    double offsetPct);

    // 设备指令超时重试信号（通知 UI 层显示重试状态）
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);
    void onTimeout();

private:
    enum class CmdKind { Threshold, Offset };

    struct Pending {
        QString  qrcode;
        CmdKind  kind;
    };

    QByteArray buildRegisterValue(quint16 value) const;
    void disconnectAll();
    void markDeviceFailed(const QString &qrcode);
    void tryMarkDeviceSuccess(const QString &qrcode);
    void checkAllFinished();
    void forceFinish();
    void logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode);

    void writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    static QString safeLogPathSegment(const QString& value);
    ILogger& deviceDetailLogger();
    QString subFunctionName() const;

private:
    QVector<QString>  m_qrcodes;

    // 待下发的子指令配置（通过 setter 注入）
    bool   m_thresholdSet = false;
    double m_thresholdPct = 0.0;
    bool   m_offsetSet    = false;
    double m_offsetPct    = 0.0;

    // 实际下发的子指令数（由 m_thresholdSet + m_offsetSet 推导）
    int    m_subCmdPerDevice = 0;

    // uuid → (qrcode, kind)
    QHash<qint64, Pending>         m_pendingMap;
    QHash<qint64, ModbusCommand>   m_pendingCommands;
    QList<QMetaObject::Connection> m_connections;

    QHash<QString, int>  m_deviceSuccessCount;   // qrcode → 已成功的子指令数
    QHash<QString, bool> m_deviceFailed;         // qrcode → 是否已标记失败

    int        m_totalDevices = 0;
    QAtomicInt m_completedDevices{0};
    bool       m_stopped = false;

    int         m_successCount = 0;
    QStringList m_failedQrCodes;

    QTimer *m_timeoutTimer       = nullptr;
    bool    m_allFinishedEmitted = false;

    QStringList m_targetQrCodes;  // 本次任务的目标设备列表（用于边界日志）

    ILogger deviceLogger;               ///< 设备详细日志记录器（subFunction 级共享）
    bool m_loggerInitialized = false;   ///< deviceLogger 路径是否已设置
};

#endif // SET_HUMIDITY_OFFSET_TASK_H
