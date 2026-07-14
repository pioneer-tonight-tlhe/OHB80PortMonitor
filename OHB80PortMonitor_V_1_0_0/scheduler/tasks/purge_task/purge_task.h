#ifndef PURGE_TASK_H
#define PURGE_TASK_H

#include "scheduler/scheduler_task.h"
#include "purge_task_logger.h"
#include "purge_task_types.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QDateTime>
#include <QList>
#include <QTimer>

class PurgeTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit PurgeTask(const PurgeTaskDefinition &definition, QObject *parent = nullptr);
    ~PurgeTask() override;

    void start() override;
    void stop() override;
    QString taskType() const override { return QStringLiteral("PurgeTask"); }

    QString outputDir() const;
    void setOutputDir(const QString &outputDir);

signals:
    void outputDirectoryReady(const QString &outputDir);
    void purgeStarted(const QString &qrCode);
    void purgeStageStarted(int stageNo, const QString &stageName, int durationSeconds);
    void purgeStageFinished(int stageNo, const QString &stageName);
    void purgeActionFinished(int stageNo,
                             int actionNo,
                             const QString &commandId,
                             bool success,
                             const QString &message);
    void purgeFinished(bool success, const QString &message, const QString &outputDir);

private slots:
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);
    void onStageTimeout();

private:
    void createTimersIfNeeded();
    void stopTimers();

    void startNextStage();
    void startNextAction();
    void startStageTiming();
    void finishCurrentStage();
    void finishTask(bool success,
                    const QString &message,
                    SchedulerTask::State finalState);

    bool submitActionCommand(const PurgeActionDefinition &action, QString *errorMessage);
    bool applyActionParams(ModbusCommand &cmd,
                           const PurgeActionDefinition &action,
                           QString *errorMessage) const;
    QByteArray buildRegisterValue(quint16 value) const;
    void applyRegisterValue(ModbusCommand &cmd, const QByteArray &registerValue) const;
    void disconnectCommandSignals();

private:
    PurgeTaskDefinition m_definition;
    PurgeTaskLogger m_logger;

    bool m_cancelRequested = false;
    bool m_finishEmitted = false;
    int m_currentStageIndex = -1;
    int m_currentActionIndex = -1;

    qint64 m_pendingCommandUuid = 0;
    QString m_pendingCommandId;
    QString m_lastError;
    QList<QMetaObject::Connection> m_commandConnections;

    QTimer *m_stageTimer = nullptr;
    QDateTime m_taskStartedAt;
    QDateTime m_currentStageTimingStartedAt;

    QString m_outputDir;
};

#endif // PURGE_TASK_H
