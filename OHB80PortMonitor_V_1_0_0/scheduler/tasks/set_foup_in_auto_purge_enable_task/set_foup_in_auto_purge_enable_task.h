#ifndef SET_FOUP_IN_AUTO_PURGE_ENABLE_TASK_H
#define SET_FOUP_IN_AUTO_PURGE_ENABLE_TASK_H

#include "../../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QTimer>
#include <QVector>

class OperationDispatchTask;

class SetFoupInAutoPurgeEnableTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SetFoupInAutoPurgeEnableTask(const QVector<QString> &qrcodes,
                                          int enableValue,
                                          QObject *parent = nullptr);
    ~SetFoupInAutoPurgeEnableTask() override;

    void start() override;
    void stop() override;
    QString taskType() const override { return "SetFoupInAutoPurgeEnableTask"; }

signals:
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     int enableValue);
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
    bool persistConfig(QString *errorMessage);
    void logFailedDevice(OperationDispatchTask *opTask, const QString &qrcode);
    void writeDeviceSkipLog(const QString &qrCode, const QString &commandId, const QString &reason);
    void writeDeviceCommandLog(const QString &qrCode, const ModbusCommand &cmd, bool success);
    QString commandFrameLogString(const ModbusCommand &cmd) const;
    QString deviceLogPath() const;
    QString subFunctionName() const;
    ILogger &deviceDetailLogger();

private:
    QVector<QString> m_qrcodes;
    int m_enableValue = 0;

    QHash<qint64, QString> m_pendingMap;
    QList<QMetaObject::Connection> m_connections;
    int m_totalCount = 0;
    QAtomicInt m_completedCount{0};
    bool m_stopped = false;

    int m_successCount = 0;
    QStringList m_failedQrCodes;
    QStringList m_targetQrCodes;

    QTimer *m_timeoutTimer = nullptr;
    bool m_allFinishedEmitted = false;

    ILogger deviceLogger;
    bool m_loggerInitialized = false;
};

#endif // SET_FOUP_IN_AUTO_PURGE_ENABLE_TASK_H
