#include "firmwareupgradetestsettingwidget.h"

#include "../settingwidget/settingitemwidget.h"
#include "firmwareupdatewidget.h"
#include "scheduler.h"
#include "tasks/firmware_upgrade_test_task/firmware_upgrade_stress_task.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>

FirmwareUpgradeTestSettingWidget::FirmwareUpgradeTestSettingWidget(
    FirmwareUpdateWidget *monitorWidget,
    QWidget *parent)
    : SettingWidget(parent)
    , m_monitorWidget(monitorWidget)
{
    setTitle(QStringLiteral("Firmware Upgrade Test"));
    initUI();
    setTotalRoundProgressText(0, m_roundCountSpinBox ? m_roundCountSpinBox->value() : 0);
    setCurrentRoundProgressText(0, 0);
    refreshActionState();
}

FirmwareUpgradeTestSettingWidget::~FirmwareUpgradeTestSettingWidget() = default;

void FirmwareUpgradeTestSettingWidget::setFirmwareFilePath(const QString &filePath)
{
    m_binFilePath = filePath;
}

void FirmwareUpgradeTestSettingWidget::onStartClicked()
{
    if (m_stressTask && m_lastTaskState == SchedulerTask::Paused) {
        resumeTask();
        return;
    }

    if (m_stressTask) {
        return;
    }

    startNewTask();
}

void FirmwareUpgradeTestSettingWidget::onPauseClicked()
{
    if (!m_stressTask || m_lastTaskState != SchedulerTask::Running || m_pauseRequested) {
        return;
    }

    m_pauseRequested = true;
    if (m_actionItem) {
        m_actionItem->setStatusWaiting(QStringLiteral("Pause Requested"));
    }
    refreshActionState();

    QMetaObject::invokeMethod(m_stressTask.data(), "pause", Qt::QueuedConnection);
}

void FirmwareUpgradeTestSettingWidget::onRestartClicked()
{
    if (!m_stressTask) {
        startNewTask();
        return;
    }

    if (m_lastTaskState != SchedulerTask::Paused || m_restartRequested) {
        return;
    }

    FirmwareUpgradeStressTask *task = m_stressTask.data();
    if (!task) {
        return;
    }

    m_restartRequested = true;
    m_pauseRequested = false;
    if (m_actionItem) {
        m_actionItem->setStatusWaiting(QStringLiteral("Restarting"));
    }
    refreshActionState();

    QMetaObject::invokeMethod(task,
                              [task]() {
                                  task->stop();
                              },
                              Qt::QueuedConnection);
}

void FirmwareUpgradeTestSettingWidget::onTaskStateChanged(SchedulerTask::State state)
{
    m_lastTaskState = state;

    if (state == SchedulerTask::Running) {
        m_pauseRequested = false;
        if (m_actionItem) {
            m_actionItem->setStatusWaiting(QStringLiteral("Running"));
        }
    } else if (state == SchedulerTask::Paused) {
        m_pauseRequested = false;
        if (m_actionItem) {
            m_actionItem->setStatusWaiting(QStringLiteral("Paused"));
        }
    }

    refreshActionState();
}

void FirmwareUpgradeTestSettingWidget::onTaskFinished(bool success, const QString &message)
{
    const bool restartRequested = m_restartRequested;

    if (restartRequested && m_actionItem) {
        m_actionItem->setStatusWaiting(QStringLiteral("Restarting"));
    } else if (m_lastTaskState == SchedulerTask::Finished && m_actionItem) {
        m_actionItem->setStatusOK(QStringLiteral("Finished"));
    } else if (m_lastTaskState == SchedulerTask::Cancelled && m_actionItem) {
        m_actionItem->setStatusFailed(QStringLiteral("Cancelled"));
    } else if (!success && m_actionItem) {
        m_actionItem->setStatusFailed(QStringLiteral("Failed"));
    }

    if (!success && !message.trimmed().isEmpty() && !restartRequested) {
        QMessageBox::warning(this,
                             QStringLiteral("Firmware Upgrade Test"),
                             message);
    }

    m_stressTask = nullptr;
    m_pauseRequested = false;
    m_restartRequested = false;
    m_completedRounds = 0;
    refreshActionState();

    if (restartRequested) {
        startNewTask();
    }
}

void FirmwareUpgradeTestSettingWidget::onTaskDestroyed()
{
    m_stressTask = nullptr;
    m_pauseRequested = false;
    m_restartRequested = false;
    m_completedRounds = 0;
    refreshActionState();
}

void FirmwareUpgradeTestSettingWidget::onRoundStarted(int roundIndex,
                                                      int targetRounds,
                                                      const QStringList &deviceIds)
{
    Q_UNUSED(roundIndex)
    Q_UNUSED(targetRounds)

    setCurrentRoundProgressText(0, deviceIds.size());

    if (!m_monitorWidget) {
        return;
    }

    m_monitorWidget->prepareMonitoredRound(deviceIds);
}

void FirmwareUpgradeTestSettingWidget::onTotalRoundProgressChanged(int completedRounds, int targetRounds)
{
    m_completedRounds = completedRounds;
    setTotalRoundProgressText(completedRounds, targetRounds);
    refreshActionState();
}

void FirmwareUpgradeTestSettingWidget::onCurrentRoundProgressChanged(int completedDevices, int totalDevices)
{
    setCurrentRoundProgressText(completedDevices, totalDevices);

    if (m_monitorWidget) {
        m_monitorWidget->updateMonitoredRoundProgress(completedDevices, totalDevices);
    }
}

void FirmwareUpgradeTestSettingWidget::onRoundSummaryReady(
    const FirmwareUpgradeTestRoundSummaryRecord &record)
{
    if (!m_monitorWidget || record.screenshotPath.trimmed().isEmpty()) {
        return;
    }

    m_monitorWidget->saveCurrentScreenshot(record.screenshotPath);
}

void FirmwareUpgradeTestSettingWidget::onFailureDetailReady(
    const FirmwareUpgradeTestFailureDetailRecord &record)
{
    Q_UNUSED(record)
}

void FirmwareUpgradeTestSettingWidget::onSessionSummaryUpdated(
    const FirmwareUpgradeTestSessionSummaryRecord &record)
{
    m_completedRounds = record.completedRounds;
    setTotalRoundProgressText(record.completedRounds, record.targetRounds);
    refreshActionState();
}

void FirmwareUpgradeTestSettingWidget::onStressTaskFinished(
    const FirmwareUpgradeTestSessionSummaryRecord &record)
{
    m_completedRounds = record.completedRounds;
    setTotalRoundProgressText(record.completedRounds, record.targetRounds);
}

void FirmwareUpgradeTestSettingWidget::initUI()
{
    initActionItem();
    initDeviceNumberItem();
    initRoundCountItem();
    initIntervalItem();
    initTotalProgressItem();
    initCurrentRoundProgressItem();
}

void FirmwareUpgradeTestSettingWidget::initDeviceNumberItem()
{
    m_deviceNumberItem = new SettingItemWidget(this);
    m_deviceNumberItem->setTitle(QStringLiteral("Target Device ID"));
    m_deviceNumberItem->setTip(
        QStringLiteral("Set the single device QRCode/device ID for the firmware upgrade test."));

    m_deviceNumberSpinBox = new QSpinBox(m_deviceNumberItem);
    m_deviceNumberSpinBox->setRange(1, 999999);
    m_deviceNumberSpinBox->setValue(12001);
    m_deviceNumberSpinBox->setFixedWidth(160);
    m_deviceNumberItem->addWidget(QStringLiteral("target_device_id"), m_deviceNumberSpinBox);

    addItem(m_deviceNumberItem);
}

void FirmwareUpgradeTestSettingWidget::initRoundCountItem()
{
    m_roundCountItem = new SettingItemWidget(this);
    m_roundCountItem->setTitle(QStringLiteral("Target Rounds"));
    m_roundCountItem->setTip(QStringLiteral("Set how many automatic firmware upgrade rounds should run."));

    m_roundCountSpinBox = new QSpinBox(m_roundCountItem);
    m_roundCountSpinBox->setRange(1, 100000);
    m_roundCountSpinBox->setValue(10000);
    m_roundCountSpinBox->setFixedWidth(160);
    m_roundCountItem->addWidget(QStringLiteral("round_count"), m_roundCountSpinBox);
    connect(m_roundCountSpinBox,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            [this](int value) {
                if (!m_stressTask || m_lastTaskState == SchedulerTask::Paused) {
                    setTotalRoundProgressText(m_completedRounds, value);
                }
            });

    addItem(m_roundCountItem);
}

void FirmwareUpgradeTestSettingWidget::initIntervalItem()
{
    m_intervalItem = new SettingItemWidget(this);
    m_intervalItem->setTitle(QStringLiteral("Round Interval"));
    m_intervalItem->setTip(QStringLiteral("Set the wait time between two upgrade rounds."));

    m_intervalSpinBox = new QSpinBox(m_intervalItem);
    m_intervalSpinBox->setRange(2, 3600);
    m_intervalSpinBox->setSuffix(QStringLiteral(" s"));
    m_intervalSpinBox->setValue(2);
    m_intervalSpinBox->setFixedWidth(160);
    m_intervalItem->addWidget(QStringLiteral("round_interval"), m_intervalSpinBox);

    addItem(m_intervalItem);
}

void FirmwareUpgradeTestSettingWidget::initActionItem()
{
    m_actionItem = new SettingItemWidget(this);
    m_actionItem->setTitle(QStringLiteral("Test Actions"));
    m_actionItem->setTip(
        QStringLiteral("Start or soft-pause the firmware upgrade stress test. "
                       "Manual firmware update controls are locked while the test task is active."));

    m_startButton = new QPushButton(QStringLiteral("Start Test"), m_actionItem);
    m_pauseButton = new QPushButton(QStringLiteral("Pause Test"), m_actionItem);
    m_restartButton = new QPushButton(QStringLiteral("Restart Test"), m_actionItem);

    m_startButton->setFixedWidth(120);
    m_pauseButton->setFixedWidth(120);
    m_restartButton->setFixedWidth(120);

    m_actionItem->addWidget(QStringLiteral("start_test"), m_startButton);
    m_actionItem->addWidget(QStringLiteral("pause_test"), m_pauseButton);
    m_actionItem->addWidget(QStringLiteral("restart_test"), m_restartButton);

    connect(m_startButton, &QPushButton::clicked,
            this, &FirmwareUpgradeTestSettingWidget::onStartClicked);
    connect(m_pauseButton, &QPushButton::clicked,
            this, &FirmwareUpgradeTestSettingWidget::onPauseClicked);
    connect(m_restartButton, &QPushButton::clicked,
            this, &FirmwareUpgradeTestSettingWidget::onRestartClicked);

    addItem(m_actionItem);
}

void FirmwareUpgradeTestSettingWidget::initTotalProgressItem()
{
    m_totalProgressItem = new SettingItemWidget(this);
    m_totalProgressItem->setTitle(QStringLiteral("Total Round Progress"));
    m_totalProgressItem->setTip(QStringLiteral("Shows the completed test rounds against the configured total."));

    m_totalProgressBar = new QProgressBar(m_totalProgressItem);
    m_totalProgressBar->setMinimumWidth(220);
    m_totalProgressBar->setRange(0, 1);
    m_totalProgressBar->setValue(0);
    m_totalProgressBar->setTextVisible(true);
    m_totalProgressBar->setFormat(QStringLiteral("0/0"));
    m_totalProgressItem->addWidget(QStringLiteral("total_progress"), m_totalProgressBar);

    addItem(m_totalProgressItem);
}

void FirmwareUpgradeTestSettingWidget::initCurrentRoundProgressItem()
{
    m_currentRoundProgressItem = new SettingItemWidget(this);
    m_currentRoundProgressItem->setTitle(QStringLiteral("Current Round Progress"));
    m_currentRoundProgressItem->setTip(QStringLiteral("Shows how many devices in the current round have finished upgrading."));

    m_currentRoundProgressBar = new QProgressBar(m_currentRoundProgressItem);
    m_currentRoundProgressBar->setMinimumWidth(220);
    m_currentRoundProgressBar->setRange(0, 1);
    m_currentRoundProgressBar->setValue(0);
    m_currentRoundProgressBar->setTextVisible(true);
    m_currentRoundProgressBar->setFormat(QStringLiteral("0/0"));
    m_currentRoundProgressItem->addWidget(QStringLiteral("current_round_progress"),
                                          m_currentRoundProgressBar);

    addItem(m_currentRoundProgressItem);
}

void FirmwareUpgradeTestSettingWidget::startNewTask()
{
    if (m_binFilePath.trimmed().isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Firmware Upgrade Test"),
                             QStringLiteral("Please select a firmware .bin file first."));
        return;
    }

    if (!QFileInfo::exists(m_binFilePath)) {
        QMessageBox::warning(this,
                             QStringLiteral("Firmware Upgrade Test"),
                             QStringLiteral("The configured firmware file does not exist anymore."));
        return;
    }

    const QString targetDeviceId =
        QString::number(m_deviceNumberSpinBox ? m_deviceNumberSpinBox->value() : 12001);

    auto *task = new FirmwareUpgradeStressTask(
        m_binFilePath,
        m_roundCountSpinBox->value(),
        m_intervalSpinBox->value() * 1000,
        QStringList{targetDeviceId});
    connectTaskSignals(task);

    m_stressTask = task;
    m_lastTaskState = SchedulerTask::Pending;
    m_pauseRequested = false;
    m_restartRequested = false;
    m_completedRounds = 0;
    setTotalRoundProgressText(0, m_roundCountSpinBox->value());
    setCurrentRoundProgressText(0, 0);

    if (m_actionItem) {
        m_actionItem->setStatusWaiting(QStringLiteral("Running"));
    }

    Scheduler::instance()->submitTask(task);
    refreshActionState();
}

void FirmwareUpgradeTestSettingWidget::resumeTask()
{
    if (!m_stressTask || m_lastTaskState != SchedulerTask::Paused) {
        return;
    }

    FirmwareUpgradeStressTask *task = m_stressTask.data();
    if (!task) {
        return;
    }

    const int intervalMs = (m_intervalSpinBox ? m_intervalSpinBox->value() : 0) * 1000;
    const int targetRounds = m_roundCountSpinBox
        ? m_roundCountSpinBox->value()
        : (m_completedRounds + 1);

    m_pauseRequested = false;
    m_restartRequested = false;
    m_lastTaskState = SchedulerTask::Running;
    if (m_actionItem) {
        m_actionItem->setStatusWaiting(QStringLiteral("Running"));
    }
    refreshActionState();

    QMetaObject::invokeMethod(task,
                              [task, intervalMs, targetRounds]() {
                                  task->setTargetRounds(targetRounds);
                                  task->setIntervalMs(intervalMs);
                                  task->resume();
                              },
                              Qt::QueuedConnection);
}

void FirmwareUpgradeTestSettingWidget::connectTaskSignals(FirmwareUpgradeStressTask *task)
{
    connect(task, &SchedulerTask::stateChanged,
            this, &FirmwareUpgradeTestSettingWidget::onTaskStateChanged,
            Qt::QueuedConnection);
    connect(task, &SchedulerTask::finished,
            this, &FirmwareUpgradeTestSettingWidget::onTaskFinished,
            Qt::QueuedConnection);
    connect(task, &QObject::destroyed,
            this, &FirmwareUpgradeTestSettingWidget::onTaskDestroyed,
            Qt::QueuedConnection);

    connect(task, &FirmwareUpgradeStressTask::roundStarted,
            this, &FirmwareUpgradeTestSettingWidget::onRoundStarted,
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::totalRoundProgressChanged,
            this, &FirmwareUpgradeTestSettingWidget::onTotalRoundProgressChanged,
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::currentRoundProgressChanged,
            this, &FirmwareUpgradeTestSettingWidget::onCurrentRoundProgressChanged,
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::roundSummaryReady,
            this, &FirmwareUpgradeTestSettingWidget::onRoundSummaryReady,
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::failureDetailReady,
            this, &FirmwareUpgradeTestSettingWidget::onFailureDetailReady,
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::sessionSummaryUpdated,
            this, &FirmwareUpgradeTestSettingWidget::onSessionSummaryUpdated,
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::stressTaskFinished,
            this, &FirmwareUpgradeTestSettingWidget::onStressTaskFinished,
            Qt::QueuedConnection);

    connect(task, &FirmwareUpgradeStressTask::deviceProgress,
            this,
            [this](const QString &deviceId, int percent) {
                if (m_monitorWidget) {
                    m_monitorWidget->applyMonitoredDeviceProgress(deviceId, percent);
                }
            },
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::deviceStateLog,
            this,
            [this](const QString &deviceId,
                   FirmwareUpgrader::UpgradeState state,
                   const QString &logMessage,
                   const QByteArray &frame) {
                if (m_monitorWidget) {
                    m_monitorWidget->applyMonitoredDeviceStateLog(deviceId, state, logMessage, frame);
                }
            },
            Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeStressTask::deviceFinished,
            this,
            [this](const QString &deviceId, bool success, const QString &message) {
                if (m_monitorWidget) {
                    m_monitorWidget->applyMonitoredDeviceFinished(deviceId, success, message);
                }
            },
            Qt::QueuedConnection);
}

void FirmwareUpgradeTestSettingWidget::refreshActionState()
{
    const bool hasTask = !m_stressTask.isNull();
    const bool paused = hasTask && m_lastTaskState == SchedulerTask::Paused;
    const bool running = hasTask && m_lastTaskState == SchedulerTask::Running;
    const bool restarting = hasTask && m_restartRequested;

    if (m_roundCountSpinBox) {
        const int minimumRounds = (hasTask && paused) ? (m_completedRounds + 1) : 1;
        if (m_roundCountSpinBox->minimum() != minimumRounds) {
            m_roundCountSpinBox->setMinimum(minimumRounds);
        }
        m_roundCountSpinBox->setEnabled(!hasTask || (paused && !restarting));
    }
    if (m_deviceNumberSpinBox) {
        m_deviceNumberSpinBox->setEnabled(!hasTask);
    }
    if (m_intervalSpinBox) {
        m_intervalSpinBox->setEnabled(!hasTask || ((paused || m_pauseRequested) && !restarting));
    }
    if (m_startButton) {
        m_startButton->setText(paused ? QStringLiteral("Continue Test")
                                      : QStringLiteral("Start Test"));
        m_startButton->setEnabled((!hasTask || paused) && !restarting);
    }
    if (m_pauseButton) {
        m_pauseButton->setText((running && m_pauseRequested)
                                   ? QStringLiteral("Pausing...")
                                   : QStringLiteral("Pause Test"));
        m_pauseButton->setEnabled(running && !m_pauseRequested && !restarting);
    }
    if (m_restartButton) {
        m_restartButton->setEnabled(!hasTask || (paused && !restarting));
    }
    if (m_monitorWidget) {
        m_monitorWidget->setInteractiveEnabled(!hasTask);
    }
}

void FirmwareUpgradeTestSettingWidget::setTotalRoundProgressText(int completedRounds, int targetRounds)
{
    if (m_totalProgressBar) {
        const int displayTarget = targetRounds > 0 ? targetRounds : 0;
        const int barTarget = displayTarget > 0 ? displayTarget : 1;
        int barValue = completedRounds;
        if (barValue < 0) {
            barValue = 0;
        } else if (barValue > barTarget) {
            barValue = barTarget;
        }

        m_totalProgressBar->setRange(0, barTarget);
        m_totalProgressBar->setValue(barValue);
        m_totalProgressBar->setFormat(
            QStringLiteral("%1/%2").arg(completedRounds).arg(displayTarget));
    }
}

void FirmwareUpgradeTestSettingWidget::setCurrentRoundProgressText(int completedDevices, int totalDevices)
{
    if (m_currentRoundProgressBar) {
        const int displayTotal = totalDevices > 0 ? totalDevices : 0;
        const int barTotal = displayTotal > 0 ? displayTotal : 1;
        int barValue = completedDevices;
        if (barValue < 0) {
            barValue = 0;
        } else if (barValue > barTotal) {
            barValue = barTotal;
        }

        m_currentRoundProgressBar->setRange(0, barTotal);
        m_currentRoundProgressBar->setValue(barValue);
        m_currentRoundProgressBar->setFormat(
            QStringLiteral("%1/%2").arg(completedDevices).arg(displayTotal));
    }
}
