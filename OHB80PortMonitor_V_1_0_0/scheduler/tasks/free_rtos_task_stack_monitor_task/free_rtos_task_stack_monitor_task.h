#ifndef FREE_RTOS_TASK_STACK_MONITOR_TASK_H
#define FREE_RTOS_TASK_STACK_MONITOR_TASK_H

#include "../../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QVector>

class QTimer;

class FreeRTOSTaskStackMonitorTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit FreeRTOSTaskStackMonitorTask(QObject *parent = nullptr);
    ~FreeRTOSTaskStackMonitorTask() override = default;

    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;

    QString taskType() const override { return QStringLiteral("FreeRTOSTaskStackMonitorTask"); }
    bool isPersistent() const override { return true; }

signals:
    void stackWaterLevelsUpdated(const QVector<int> &values);

private slots:
    void onPeriodTimeout();
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);

private:
    void pollAllDevices();
    void disconnectAll();
    void finishCurrentRound();
    static QVector<int> parseStackWaterLevels(const QByteArray &payload);
    static QString formatStackValues(const QVector<int> &values);

private:
    static constexpr int PollIntervalMs = 1000;

    QTimer *m_periodTimer = nullptr;
    QHash<qint64, QString> m_pendingMap;
    QList<QMetaObject::Connection> m_connections;
    int m_totalCount = 0;
    int m_completedCount = 0;
    bool m_roundRunning = false;
    bool m_stopped = false;
    ILogger m_logger;
};

#endif // FREE_RTOS_TASK_STACK_MONITOR_TASK_H
