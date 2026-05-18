#include "sh85periodicselfchecksettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "sh85selfcheckreportdialog.h"

#include "scheduler/scheduler.h"
#include "scheduler/tasks/sh85_periodic_self_check_task.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "loggermanager.h"

#include <QDebug>

// ============================================================
// 构造 / 析构
// ============================================================

SH85PeriodicSelfCheckSettingWidget::SH85PeriodicSelfCheckSettingWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("SH85 Periodic Self-check Configuration");
    initUI();
    bindTask();
    refreshStatusText();
}

SH85PeriodicSelfCheckSettingWidget::~SH85PeriodicSelfCheckSettingWidget()
{
    // 常驻任务由 SharedData 持有，UI 不负责销毁
    if (m_reportDialog) {
        m_reportDialog->deleteLater();
        m_reportDialog = nullptr;
    }
}

// ============================================================
// UI 初始化
// ============================================================

void SH85PeriodicSelfCheckSettingWidget::initUI()
{
    initEnableItem();
    initPeriodItem();
    initStatusItem();
    initReportItem();
}

void SH85PeriodicSelfCheckSettingWidget::initEnableItem()
{
    m_enableItem = new SettingItemWidget(this);
    m_enableItem->setTitle("Enable Periodic Self-check");
    m_enableItem->setTip("Enable or disable periodic SH85 self-check");

    m_enableCombo = new QComboBox(m_enableItem);
    m_enableCombo->addItem("false");
    m_enableCombo->addItem("true");
    m_enableCombo->setCurrentIndex(0);
    m_enableCombo->setFixedWidth(120);
    connect(m_enableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SH85PeriodicSelfCheckSettingWidget::onEnableComboChanged);

    m_enableItem->addWidget("enable_combo", m_enableCombo);
    addItem(m_enableItem);
}

void SH85PeriodicSelfCheckSettingWidget::initPeriodItem()
{
    m_periodItem = new SettingItemWidget(this);
    m_periodItem->setTitle("Self-check Period");
    m_periodItem->setTip("Configure the interval between two self-check rounds");

    m_periodSpinBox = new QSpinBox(m_periodItem);
    m_periodSpinBox->setRange(1, 999);
    m_periodSpinBox->setValue(5);
    m_periodSpinBox->setFixedWidth(80);

    m_unitCombo = new QComboBox(m_periodItem);
    m_unitCombo->addItem("s");
    m_unitCombo->addItem("min");
    m_unitCombo->addItem("hour");
    m_unitCombo->setCurrentIndex(1);     // 默认 min
    m_unitCombo->setFixedWidth(70);

    m_setBtn = new QPushButton("Set", m_periodItem);
    m_setBtn->setFixedWidth(80);
    connect(m_setBtn, &QPushButton::clicked, this, &SH85PeriodicSelfCheckSettingWidget::onSetBtnClicked);

    m_periodItem->addWidget("period_spinbox", m_periodSpinBox);
    m_periodItem->addWidget("unit_combo",     m_unitCombo);
    m_periodItem->addWidget("set_btn",        m_setBtn);
    addItem(m_periodItem);
}

void SH85PeriodicSelfCheckSettingWidget::initStatusItem()
{
    m_statusItem = new SettingItemWidget(this);
    m_statusItem->setTitle("Self-check Status");
    m_statusItem->setTip("Current periodic self-check status");

    m_statusEdit = new QLineEdit(m_statusItem);
    m_statusEdit->setReadOnly(true);
    m_statusEdit->setMinimumWidth(360);
    m_statusEdit->setStyleSheet("QLineEdit { color: #386487; font-weight: bold; }");

    m_statusItem->addWidget("status_edit", m_statusEdit);
    addItem(m_statusItem);
}

void SH85PeriodicSelfCheckSettingWidget::initReportItem()
{
    m_reportItem = new SettingItemWidget(this);
    m_reportItem->setTitle("Self-check Report");
    m_reportItem->setTip("Open the periodic self-check report dialog");

    m_reportBtn = new QPushButton("Open Report", m_reportItem);
    m_reportBtn->setFixedWidth(140);
    connect(m_reportBtn, &QPushButton::clicked, this,
            &SH85PeriodicSelfCheckSettingWidget::onReportBtnClicked);

    m_reportItem->addWidget("report_btn", m_reportBtn);
    addItem(m_reportItem);
}

// ============================================================
// 任务绑定（常驻任务由 SharedData 持有）
// ============================================================

void SH85PeriodicSelfCheckSettingWidget::bindTask()
{
    auto *task = SharedData::getSH85PeriodicSelfCheckTask();
    if (!task) {
        qWarning() << "[SH85PeriodicSelfCheckSettingWidget] SharedData::getSH85PeriodicSelfCheckTask() 返回空，"
                      "请确认 SharedData::initScheduler() 已调用";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[ui][SH85PeriodicSelfCheckSettingWidget] SharedData::getSH85PeriodicSelfCheckTask() returns null").toStdString());
        return;
    }
    m_task = task;

    // 同步 UI 当前值到任务（不影响 enable 状态）
    QMetaObject::invokeMethod(task, "setPeriod",
                              Qt::QueuedConnection,
                              Q_ARG(int, m_periodSpinBox->value()),
                              Q_ARG(SH85PeriodicSelfCheckTask::TimeUnit,
                                    unitFromIndex(m_unitCombo->currentIndex())));

    // 同步当前任务状态到 UI（任务可能此时已经在某个状态）
    m_currentTaskState = task->currentState();

    // 连接 UI 关心的整体状态信号
    connect(task, &SH85PeriodicSelfCheckTask::taskStateChanged,
            this, &SH85PeriodicSelfCheckSettingWidget::onTaskStateChanged,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask::elapsedTick,
            this, &SH85PeriodicSelfCheckSettingWidget::onTaskElapsedTick,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask::intervalCountdown,
            this, &SH85PeriodicSelfCheckSettingWidget::onTaskIntervalCountdown,
            Qt::QueuedConnection);

    // —— Report Dialog：构造时即创建并订阅信号，让 History Log 从首轮开始累计 ——
    m_reportDialog = new SH85SelfCheckReportDialog(this);
    m_reportDialog->setWindowFlag(Qt::Window);   // 独立窗口，关闭时不销毁 widget
    m_reportDialog->hide();                       // 默认隐藏
    m_reportDialog->setQrcodes(SharedData::getAllQrcodes());

    connect(task, &SH85PeriodicSelfCheckTask::countdownTick,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onCheckerCountdown,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask::selfCheckerStateChanged,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onCheckerStateChanged,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask::oneFinished,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onOneFinished,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask::allFinished,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onAllFinished,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask::deviceParticipated,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onDeviceParticipated,
            Qt::QueuedConnection);
    // 进入 Checking 时，自动重置 Live Log
    connect(task, &SH85PeriodicSelfCheckTask::taskStateChanged,
            this, [this](SH85PeriodicSelfCheckTask::State s) {
                if (s == SH85PeriodicSelfCheckTask::State::Checking && m_reportDialog) {
                    m_reportDialog->onRoundStarted();
                }
            },
            Qt::QueuedConnection);

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][SH85PeriodicSelfCheckSettingWidget] bound to resident SH85PeriodicSelfCheckTask").toStdString());
}

// ============================================================
// UI 事件
// ============================================================

void SH85PeriodicSelfCheckSettingWidget::onEnableComboChanged(int index)
{
    const bool enable = (index == 1);
    if (enable == m_isEnabled) return;
    m_isEnabled = enable;

    qDebug() << "[SH85PeriodicSelfCheckSettingWidget] enable changed:" << enable;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][SH85PeriodicSelfCheckSettingWidget] enable=%1").arg(enable).toStdString());

    if (m_task) {
        QMetaObject::invokeMethod(m_task.data(), "setEnabled",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, enable));
    }

    // —— UI 立即反馈（真实状态会随 taskStateChanged 信号同步覆盖）——
    // 启用：立即提示「自检中（执行：0s）」；
    // 停用：若当前正在 Checking，不修改状态文案（继续显示自检中，直到本轮完成）。
    if (enable) {
        m_currentTaskState = SH85PeriodicSelfCheckTask::State::Checking;
        m_elapsedSec = 0;
        refreshStatusText();
    }
    // disable 情况不立刻刷新文案，让 task 自然结束后通过 taskStateChanged 反馈

    // 通知 dialog 重置 Live Log（开启新一轮时）
    if (enable && m_reportDialog) {
        QMetaObject::invokeMethod(m_reportDialog.data(), "onRoundStarted",
                                  Qt::QueuedConnection);
    }

    emit runningStateChanged(enable);
}

void SH85PeriodicSelfCheckSettingWidget::onSetBtnClicked()
{
    const int value = m_periodSpinBox->value();
    const auto unit = unitFromIndex(m_unitCombo->currentIndex());
    const QString unitStr = SH85PeriodicSelfCheckTask::timeUnitToString(unit);

    qDebug() << "[SH85PeriodicSelfCheckSettingWidget] period set:" << value << unitStr;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][SH85PeriodicSelfCheckSettingWidget] setPeriod=%1 %2")
            .arg(value).arg(unitStr).toStdString());

    if (m_task) {
        QMetaObject::invokeMethod(m_task.data(), "setPeriod",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, value),
                                  Q_ARG(SH85PeriodicSelfCheckTask::TimeUnit, unit));
        if (m_periodItem) {
            m_periodItem->setStatusOK(QString("Set OK: %1 %2").arg(value).arg(unitStr));
        }
    } else {
        if (m_periodItem) {
            m_periodItem->setStatusFailed(QStringLiteral("Set Failed: task not available"));
        }
    }
}

void SH85PeriodicSelfCheckSettingWidget::onReportBtnClicked()
{
    if (!m_reportDialog) {
        qWarning() << "[SH85PeriodicSelfCheckSettingWidget] report dialog not initialized "
                      "(SharedData task may be null)";
        return;
    }
    m_reportDialog->show();
    m_reportDialog->raise();
    m_reportDialog->activateWindow();
}

// ============================================================
// task 回调
// ============================================================

void SH85PeriodicSelfCheckSettingWidget::onTaskStateChanged(SH85PeriodicSelfCheckTask::State state)
{
    m_currentTaskState = state;
    if (state == SH85PeriodicSelfCheckTask::State::Checking) {
        m_elapsedSec = 0;
    } else if (state == SH85PeriodicSelfCheckTask::State::WaitingNext) {
        m_intervalRemainSec = m_task ? m_task->periodSeconds() : 0;
    }
    refreshStatusText();
}

void SH85PeriodicSelfCheckSettingWidget::onTaskElapsedTick(int elapsedSeconds)
{
    m_elapsedSec = elapsedSeconds;
    if (m_currentTaskState == SH85PeriodicSelfCheckTask::State::Checking) {
        refreshStatusText();
    }
}

void SH85PeriodicSelfCheckSettingWidget::onTaskIntervalCountdown(int remainingSeconds)
{
    m_intervalRemainSec = remainingSeconds;
    if (m_currentTaskState == SH85PeriodicSelfCheckTask::State::WaitingNext) {
        refreshStatusText();
    }
}

void SH85PeriodicSelfCheckSettingWidget::refreshStatusText()
{
    if (!m_statusEdit) return;

    QString text;
    switch (m_currentTaskState) {
    case SH85PeriodicSelfCheckTask::State::Stopped:
        text = QStringLiteral("Self-check disabled");
        break;
    case SH85PeriodicSelfCheckTask::State::Checking:
        text = QStringLiteral("Self-checking (elapsed: %1s)").arg(m_elapsedSec);
        break;
    case SH85PeriodicSelfCheckTask::State::WaitingNext:
        text = QStringLiteral("Waiting for next check (countdown: %1s)").arg(m_intervalRemainSec);
        break;
    }
    m_statusEdit->setText(text);
}

// ============================================================
// 工具
// ============================================================

SH85PeriodicSelfCheckTask::TimeUnit SH85PeriodicSelfCheckSettingWidget::unitFromIndex(int idx)
{
    switch (idx) {
    case 0: return SH85PeriodicSelfCheckTask::TimeUnit::Second;
    case 1: return SH85PeriodicSelfCheckTask::TimeUnit::Minute;
    case 2: return SH85PeriodicSelfCheckTask::TimeUnit::Hour;
    default: return SH85PeriodicSelfCheckTask::TimeUnit::Minute;
    }
}

void SH85PeriodicSelfCheckSettingWidget::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);

    if (m_enableItem) m_enableItem->setEnabled(enabled);
    if (m_periodItem) m_periodItem->setEnabled(enabled);
    if (m_statusItem) m_statusItem->setEnabled(enabled);
    if (m_reportItem) m_reportItem->setEnabled(enabled);
}
