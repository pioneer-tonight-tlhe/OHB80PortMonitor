#ifndef FIRMWARE_UPGRADE_STRESS_TASK_H
#define FIRMWARE_UPGRADE_STRESS_TASK_H

#include "../../scheduler_task.h"
#include "tasks/firmware_upgrade_test_task/firmware_upgrade_test_report_repository.h"
#include "modbustcpmastermanager/modbustcpmaster/firmwareupgrader.h"

#include <QDateTime>
#include <QHash>
#include <QStringList>
#include <QPointer>
#include <QSet>
#include <QTimer>

class FirmwareUpgradeTask;

class FirmwareUpgradeStressTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit FirmwareUpgradeStressTask(const QString &binFilePath = QString(),
                                       int targetRounds = 0,
                                       int intervalMs = 0,
                                       const QStringList &deviceIds = QStringList(),
                                       QObject *parent = nullptr);
    ~FirmwareUpgradeStressTask() override;

    void start() override;
    void stop() override;
    Q_INVOKABLE void pause() override;
    Q_INVOKABLE void resume() override;
    QString taskType() const override { return "FirmwareUpgradeStressTask"; }

    void setBinFilePath(const QString &binFilePath);
    QString binFilePath() const;

    void setTargetRounds(int targetRounds);
    int targetRounds() const;

    void setIntervalMs(int intervalMs);
    int intervalMs() const;

    void setDeviceIds(const QStringList &deviceIds);
    QStringList deviceIds() const;

    QString sessionId() const;
    bool pauseRequested() const;

signals:
    void totalRoundProgressChanged(int completedRounds, int targetRounds);
    void currentRoundProgressChanged(int completedDevices, int totalDevices);
    void roundStarted(int roundIndex, int targetRounds, const QStringList &deviceIds);
    void roundSummaryReady(const FirmwareUpgradeTestRoundSummaryRecord &record);
    void failureDetailReady(const FirmwareUpgradeTestFailureDetailRecord &record);
    void sessionSummaryUpdated(const FirmwareUpgradeTestSessionSummaryRecord &record);
    void stressTaskFinished(const FirmwareUpgradeTestSessionSummaryRecord &record);

    void deviceProgress(const QString &deviceId, int percent);
    void deviceStateLog(const QString &deviceId,
                        FirmwareUpgrader::UpgradeState state,
                        const QString &logMessage,
                        const QByteArray &frame);
    void deviceFinished(const QString &deviceId, bool success, const QString &message);

private slots:
    void onRoundTaskFinished(bool success, const QString &message);
    void onRoundDeviceProgress(const QString &deviceId, int percent);
    void onRoundDeviceStateLog(const QString &deviceId,
                               FirmwareUpgrader::UpgradeState state,
                               const QString &logMessage,
                               const QByteArray &frame);
    void onRoundDeviceFinished(const QString &deviceId, bool success, const QString &message);
    void onRoundIntervalTimeout();

private:
    struct DeviceRoundResult
    {
        bool success = false;
        bool finished = false;
        QString phase;
        QString message;
        QString errorCode;
        QDateTime occurredTime;
    };

    void registerMetaTypes();
    bool beginSession();
    void resolveDeviceIds();
    void startNextRound();
    void scheduleNextRound();
    void teardownCurrentRoundTask();
    void resetCurrentRoundState();
    void finalizePause();
    void finalizeSession(const QString &status, bool success, const QString &message);
    void writeSessionSnapshot(const QString &status);
    FirmwareUpgradeTestRoundSummaryRecord buildRoundSummaryRecord(bool roundSuccess) const;
    FirmwareUpgradeTestFailureDetailRecord buildFailureDetailRecord(const QString &deviceId,
                                                                   const DeviceRoundResult &result) const;
    QString buildRoundScreenshotPath() const;
    QString classifyFailureCode(const QString &message, const QString &phase) const;
    QString phaseFromState(FirmwareUpgrader::UpgradeState state) const;
    static QStringList sortedDeviceIds(const QStringList &deviceIds);

private:
    QString m_binFilePath;
    int m_targetRounds = 0;
    int m_intervalMs = 0;
    QStringList m_configuredDeviceIds;
    QStringList m_deviceIds;

    QString m_sessionId;
    QDateTime m_sessionStartTime;

    int m_currentRoundIndex = 0;
    int m_completedRounds = 0;
    int m_successRounds = 0;
    int m_failedRounds = 0;

    int m_currentRoundFinishedDevices = 0;
    int m_currentRoundSuccessDevices = 0;
    int m_currentRoundFailedDevices = 0;
    QDateTime m_currentRoundStartTime;

    bool m_stopRequested = false;
    bool m_pauseRequested = false;
    bool m_sessionInitialized = false;

    QHash<QString, DeviceRoundResult> m_currentRoundResults;
    QPointer<FirmwareUpgradeTask> m_currentRoundTask;
    QTimer *m_roundIntervalTimer = nullptr;
};

#endif // FIRMWARE_UPGRADE_STRESS_TASK_H
