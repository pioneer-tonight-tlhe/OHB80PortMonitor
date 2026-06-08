#ifndef SET_UI_REFRESH_TIME_TASK_H
#define SET_UI_REFRESH_TIME_TASK_H

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
// SetUIRefreshTimeTask — 设置 UI 页面刷新时间（下位机屏幕）
//
//   底层指令：WriteUIRefreshTime（FC 0x10, addr 0x0004，3 寄存器，6 字节）
//   数据布局（大端）：
//     [0..1] logoSec        — logo 界面展示时间（秒）
//     [2..3] paramTotalSec  — 参数界面展示总时间（秒）
//     [4..5] paramSwitchSec — 参数界面切换时间（秒）
//
//   信号 allFinished：
//     allSuccess      — 是否全部成功
//     successCount    — 成功设备数
//     failedQrCodes   — 失败设备 QRCode 列表
//     logoSec         — 设置的 logo 界面时长
//     paramTotalSec   — 设置的参数界面总时长
//     paramSwitchSec  — 设置的参数界面切换时间
// ====================================================================
class SetUIRefreshTimeTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SetUIRefreshTimeTask(const QVector<QString> &qrcodes,
                                  int logoSec,
                                  int paramTotalSec,
                                  int paramSwitchSec,
                                  QObject *parent = nullptr);
    ~SetUIRefreshTimeTask();

    void start() override;
    void stop()  override;
    QString taskType() const override { return "SetUIRefreshTimeTask"; }

signals:
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     int logoSec,
                     int paramTotalSec,
                     int paramSwitchSec);

    // 设备指令超时重试信号（通知 UI 层显示重试状态）
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);
    void onTimeout();

private:
    QByteArray buildPayload() const;
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
    int              m_logoSec;
    int              m_paramTotalSec;
    int              m_paramSwitchSec;

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

#endif // SET_UI_REFRESH_TIME_TASK_H
