#include "alarmresetwidget.h"

#include "../settingwidget/settingitemwidget.h"
#include "logdatabases/alarmlogdb/alarmlogdbcon.h"
#include "logdatabases/databasemanager.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/alarm_reset_task/alarm_reset_task.h"

#include <QtGlobal>

AlarmResetWidget::AlarmResetWidget(QWidget* parent)
    : SettingWidget(parent)
{
    setTitle(QStringLiteral("Alarm Reset"));
    initUI();
    refreshUnresolvedCount();
}

AlarmResetWidget::~AlarmResetWidget() = default;

void AlarmResetWidget::initUI()
{
    m_controlItem = new SettingItemWidget(this);
    m_controlItem->setTitle(QStringLiteral("Unresolved Alarm Reset"));
    m_controlItem->setTip(QStringLiteral("Batch reset unresolved alarm log records in the background"));

    m_unresolvedCountLabel = new QLabel(QStringLiteral("Unresolved: --"), m_controlItem);
    m_unresolvedCountLabel->setMinimumWidth(130);
    m_controlItem->addWidget(QStringLiteral("unresolved_count"), m_unresolvedCountLabel);

    m_progressBar = new QProgressBar(m_controlItem);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setMinimumWidth(180);
    m_controlItem->addWidget(QStringLiteral("progress_bar"), m_progressBar);

    m_progressLabel = new QLabel(QStringLiteral("Resolved: 0 / 0"), m_controlItem);
    m_progressLabel->setMinimumWidth(170);
    m_controlItem->addWidget(QStringLiteral("progress_text"), m_progressLabel);

    m_refreshButton = new QPushButton(QStringLiteral("Refresh"), m_controlItem);
    connect(m_refreshButton, &QPushButton::clicked,
            this, &AlarmResetWidget::refreshUnresolvedCount);
    m_controlItem->addWidget(QStringLiteral("refresh_button"), m_refreshButton);

    m_startButton = new QPushButton(QStringLiteral("Start"), m_controlItem);
    connect(m_startButton, &QPushButton::clicked,
            this, &AlarmResetWidget::onStartClicked);
    m_controlItem->addWidget(QStringLiteral("start_button"), m_startButton);

    addItem(m_controlItem);
}

void AlarmResetWidget::refreshUnresolvedCount()
{
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) {
        m_unresolvedCountLabel->setText(QStringLiteral("Unresolved: --"));
        m_controlItem->setStatusFailed(QStringLiteral("Alarm log database unavailable"));
        return;
    }

    const int count = db->queryTotalCountWithConditions(/*alarmLevel*/ -1,
                                                        /*qrCode*/ QString(),
                                                        /*alarmType*/ QString(),
                                                        /*isResolved*/ 0,
                                                        /*startTime*/ QString(),
                                                        /*endTime*/ QString(),
                                                        /*resolveStartTime*/ QString(),
                                                        /*resolveEndTime*/ QString(),
                                                        /*maxUserPermission*/ -1);
    m_unresolvedCountLabel->setText(QStringLiteral("Unresolved: %1").arg(count));

    if (!m_taskRunning) {
        m_progressBar->setValue(count == 0 ? 100 : 0);
        m_progressLabel->setText(QStringLiteral("Resolved: 0 / %1").arg(count));
    }
}

void AlarmResetWidget::onStartClicked()
{
    if (m_taskRunning) {
        return;
    }

    auto* task = new AlarmResetTask();
    task->setBatchSize(100);
    task->setBatchIntervalMs(1000);

    m_taskRunning = true;
    setControlsEnabled(false);
    m_progressBar->setValue(0);
    m_controlItem->setStatusWaiting(QStringLiteral("Resetting..."));

    connect(task, &AlarmResetTask::resetStarted,
            this, [this](int totalCount) {
                m_unresolvedCountLabel->setText(QStringLiteral("Unresolved: %1").arg(totalCount));
                m_progressLabel->setText(QStringLiteral("Resolved: 0 / %1").arg(totalCount));
                m_progressBar->setValue(totalCount == 0 ? 100 : 0);
            }, Qt::QueuedConnection);

    connect(task, &AlarmResetTask::batchResolved,
            this, &AlarmResetWidget::updateProgress,
            Qt::QueuedConnection);

    connect(task, &AlarmResetTask::resetFinished,
            this, [this](bool success,
                         const QString& message,
                         int totalCount,
                         int resolvedCount,
                         int remainingCount) {
                Q_UNUSED(totalCount)
                Q_UNUSED(resolvedCount)

                m_taskRunning = false;
                setControlsEnabled(true);
                m_unresolvedCountLabel->setText(QStringLiteral("Unresolved: %1").arg(remainingCount));

                if (success) {
                    m_progressBar->setValue(100);
                    m_controlItem->setStatusOK(message);
                } else {
                    m_controlItem->setStatusFailed(message);
                }
            }, Qt::QueuedConnection);

    Scheduler::instance()->submitTask(task);
}

void AlarmResetWidget::updateProgress(int totalCount,
                                      int resolvedCount,
                                      int remainingCount,
                                      int batchCount)
{
    const int percent = totalCount > 0
        ? qBound(0, resolvedCount * 100 / totalCount, 100)
        : 100;
    m_progressBar->setValue(percent);
    m_unresolvedCountLabel->setText(QStringLiteral("Unresolved: %1").arg(remainingCount));
    m_progressLabel->setText(QStringLiteral("Resolved: %1 / %2 (+%3)")
                                 .arg(resolvedCount)
                                 .arg(totalCount)
                                 .arg(batchCount));
}

void AlarmResetWidget::setControlsEnabled(bool enabled)
{
    if (m_refreshButton) {
        m_refreshButton->setEnabled(enabled);
    }
    if (m_startButton) {
        m_startButton->setEnabled(enabled);
    }
}
