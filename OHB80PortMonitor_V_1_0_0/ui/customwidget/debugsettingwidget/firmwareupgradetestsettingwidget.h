#ifndef FIRMWAREUPGRADETESTSETTINGWIDGET_H
#define FIRMWAREUPGRADETESTSETTINGWIDGET_H

#include "../settingwidget/settingwidget.h"
#include "scheduler_task.h"
#include "tasks/firmware_upgrade_test_task/firmware_upgrade_test_report_types.h"

#include <QPointer>
#include <QStringList>

class FirmwareUpdateWidget;
class FirmwareUpgradeStressTask;
class SettingItemWidget;
class QProgressBar;
class QPushButton;
class QSpinBox;

class FirmwareUpgradeTestSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit FirmwareUpgradeTestSettingWidget(FirmwareUpdateWidget *monitorWidget,
                                              QWidget *parent = nullptr);
    ~FirmwareUpgradeTestSettingWidget() override;

public slots:
    void setFirmwareFilePath(const QString &filePath);

private slots:
    void onStartClicked();
    void onPauseClicked();
    void onRestartClicked();
    void onTaskStateChanged(SchedulerTask::State state);
    void onTaskFinished(bool success, const QString &message);
    void onTaskDestroyed();
    void onRoundStarted(int roundIndex, int targetRounds, const QStringList &deviceIds);
    void onTotalRoundProgressChanged(int completedRounds, int targetRounds);
    void onCurrentRoundProgressChanged(int completedDevices, int totalDevices);
    void onRoundSummaryReady(const FirmwareUpgradeTestRoundSummaryRecord &record);
    void onFailureDetailReady(const FirmwareUpgradeTestFailureDetailRecord &record);
    void onSessionSummaryUpdated(const FirmwareUpgradeTestSessionSummaryRecord &record);
    void onStressTaskFinished(const FirmwareUpgradeTestSessionSummaryRecord &record);

private:
    void initUI();
    void initDeviceNumberItem();
    void initRoundCountItem();
    void initIntervalItem();
    void initActionItem();
    void initTotalProgressItem();
    void initCurrentRoundProgressItem();
    void startNewTask();
    void resumeTask();
    void connectTaskSignals(FirmwareUpgradeStressTask *task);
    void refreshActionState();
    void setTotalRoundProgressText(int completedRounds, int targetRounds);
    void setCurrentRoundProgressText(int completedDevices, int totalDevices);

private:
    FirmwareUpdateWidget *m_monitorWidget = nullptr;
    QPointer<FirmwareUpgradeStressTask> m_stressTask;

    SettingItemWidget *m_roundCountItem = nullptr;
    SettingItemWidget *m_deviceNumberItem = nullptr;
    SettingItemWidget *m_intervalItem = nullptr;
    SettingItemWidget *m_actionItem = nullptr;
    SettingItemWidget *m_totalProgressItem = nullptr;
    SettingItemWidget *m_currentRoundProgressItem = nullptr;

    QSpinBox *m_roundCountSpinBox = nullptr;
    QSpinBox *m_deviceNumberSpinBox = nullptr;
    QSpinBox *m_intervalSpinBox = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_restartButton = nullptr;
    QProgressBar *m_totalProgressBar = nullptr;
    QProgressBar *m_currentRoundProgressBar = nullptr;

    QString m_binFilePath;
    int m_completedRounds = 0;
    SchedulerTask::State m_lastTaskState = SchedulerTask::Pending;
    bool m_pauseRequested = false;
    bool m_restartRequested = false;
};

#endif // FIRMWAREUPGRADETESTSETTINGWIDGET_H
