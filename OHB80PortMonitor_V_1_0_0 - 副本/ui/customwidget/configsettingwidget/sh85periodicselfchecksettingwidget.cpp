#include "sh85periodicselfchecksettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "sh85selfcheckreportdialog.h"

#include "scheduler/scheduler.h"
#include "scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "app/ohbdeviceconfig.h"
#include "loggermanager.h"

#include <QDebug>
#include <QSignalBlocker>

// ============================================================
// 鏋勯€?/ 鏋愭瀯
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
    // 甯搁┗浠诲姟鐢?SharedData 鎸佹湁锛孶I 涓嶈礋璐ｉ攢姣?    // 浠呴攢姣佹姤鍛婂璇濇
    if (m_reportDialog) {
        m_reportDialog->deleteLater();
        m_reportDialog = nullptr;
    }
}

// ============================================================
// UI 鍒濆鍖?// ============================================================

void SH85PeriodicSelfCheckSettingWidget::initUI()
{
    // 鍒濆鍖栨墍鏈夌晫闈㈤」
    initEnableItem();
    initPeriodItem();
    initStatusItem();
    initReportItem();
    refreshActionControlsState();
}

void SH85PeriodicSelfCheckSettingWidget::initEnableItem()
{
    // 鍒濆鍖栧惎鐢ㄩ」锛氬垱寤哄惎鐢?绂佺敤鍛ㄦ湡鎬ц嚜妫€鐨勪笅鎷夋
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
    // 鍒濆鍖栧懆鏈熼」锛氬垱寤哄懆鏈熸暟鍊笺€佸崟浣嶅拰璁剧疆鎸夐挳
    m_periodItem = new SettingItemWidget(this);
    m_periodItem->setTitle("Self-check Period");
    m_periodItem->setTip("Configure the interval between two self-check rounds");

    m_periodSpinBox = new QSpinBox(m_periodItem);
    m_periodSpinBox->setRange(0, 999999);
    m_periodSpinBox->setValue(5);
    m_periodSpinBox->setFixedWidth(80);

    m_unitCombo = new QComboBox(m_periodItem);
    m_unitCombo->addItem("s");
    m_unitCombo->addItem("min");
    m_unitCombo->addItem("hour");
    m_unitCombo->setCurrentIndex(1);     // 榛樿 min
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
    // 鍒濆鍖栫姸鎬侀」锛氬垱寤哄彧璇荤姸鎬佹樉绀烘
    m_statusItem = new SettingItemWidget(this);
    m_statusItem->setTitle("Self-check Status");
    m_statusItem->setTip("Current periodic self-check status");

    m_statusEdit = new QLineEdit(m_statusItem);
    m_statusEdit->setReadOnly(true);
    m_statusEdit->setStyleSheet("QLineEdit { color: #386487; font-weight: bold; }");

    m_statusItem->addWidget("status_edit", m_statusEdit);
    addItem(m_statusItem);
}

void SH85PeriodicSelfCheckSettingWidget::initReportItem()
{
    // 鍒濆鍖栨姤鍛婇」锛氬垱寤烘墦寮€鎶ュ憡瀵硅瘽妗嗙殑鎸夐挳
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
// 浠诲姟缁戝畾锛堝父椹讳换鍔＄敱 SharedData 鎸佹湁锛?// ============================================================

void SH85PeriodicSelfCheckSettingWidget::bindTask()
{
    // 缁戝畾 SharedData 鎸佹湁鐨勫父椹讳换鍔?    auto *task = SharedData::getSH85PeriodicSelfCheckTask();
    if (!task) {
        qWarning() << "[SH85PeriodicSelfCheckSettingWidget] SharedData::getSH85PeriodicSelfCheckTask() 杩斿洖绌猴紝"
                      "璇风‘璁?SharedData::initScheduler() 宸茶皟鐢?;
        LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[ui][SH85PeriodicSelfCheckSettingWidget] SharedData::getSH85PeriodicSelfCheckTask() returns null").toStdString());
        return;
    }
    m_task = task;

    // 鍚屾浠诲姟鍚敤鐘舵€佸埌 UI
    m_isEnabled = task->isEnabled();
    if (m_enableCombo) {
        const QSignalBlocker blocker(m_enableCombo);
        m_enableCombo->setCurrentIndex(m_isEnabled ? 1 : 0);
    }

    // 鍚屾閰嶇疆鍒?UI锛堜笉瑕嗙洊浠诲姟璁剧疆锛涗换鍔＄殑鍛ㄦ湡宸插湪 SharedData 鍒濆鍖栨椂鎸夐厤缃簲鐢級
    {
        const int periodSec = OHBDeviceConfig::getInstance().readSH85SelfCheckPeriodSeconds();
        int displayValue = periodSec;
        int unitIndex = 0; // 0=s, 1=min, 2=hour
        if (periodSec > 0 && (periodSec % 3600 == 0)) {
            unitIndex = 2;
            displayValue = periodSec / 3600;
        } else if (periodSec > 0 && (periodSec % 60 == 0)) {
            unitIndex = 1;
            displayValue = periodSec / 60;
        }
        if (m_unitCombo) {
            const QSignalBlocker blockerUnit(m_unitCombo);
            m_unitCombo->setCurrentIndex(unitIndex);
        }
        if (m_periodSpinBox) {
            const QSignalBlocker blockerSpin(m_periodSpinBox);
            m_periodSpinBox->setValue(displayValue);
        }
    }

    // 鍚屾褰撳墠浠诲姟鐘舵€佸埌 UI锛堜换鍔″彲鑳芥鏃跺凡缁忓湪鏌愪釜鐘舵€侊級
    m_currentTaskState = task->currentState();

    // 杩炴帴 UI 鍏冲績鐨勬暣浣撶姸鎬佷俊鍙?    connect(task, &SH85PeriodicSelfCheckTask2::taskStateChanged,
            this, &SH85PeriodicSelfCheckSettingWidget::onTaskStateChanged,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask2::elapsedTick,
            this, &SH85PeriodicSelfCheckSettingWidget::onTaskElapsedTick,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask2::intervalCountdown,
            this, &SH85PeriodicSelfCheckSettingWidget::onTaskIntervalCountdown,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask2::bootDelayCountdown,
            this, &SH85PeriodicSelfCheckSettingWidget::onTaskBootDelayCountdown,
            Qt::QueuedConnection);

    // 鈥斺€?Report Dialog锛氭瀯閫犳椂鍗冲垱寤哄苟璁㈤槄淇″彿锛岃 History Log 浠庨杞紑濮嬬疮璁?鈥斺€?    m_reportDialog = new SH85SelfCheckReportDialog(this);
    m_reportDialog->setWindowFlag(Qt::Window);   // 鐙珛绐楀彛锛屽叧闂椂涓嶉攢姣?widget
    m_reportDialog->hide();                       // 榛樿闅愯棌
    m_reportDialog->setQrcodes(SharedData::getAllQrcodes());

    // 杩炴帴浠诲姟淇″彿鍒版姤鍛婂璇濇
    connect(task, &SH85PeriodicSelfCheckTask2::countdownTick,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onCheckerCountdown,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask2::selfCheckerStateChanged,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onCheckerStateChanged,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask2::oneFinished,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onOneFinished,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask2::allFinished,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onAllFinished,
            Qt::QueuedConnection);
    connect(task, &SH85PeriodicSelfCheckTask2::deviceParticipated,
            m_reportDialog.data(), &SH85SelfCheckReportDialog::onDeviceParticipated,
            Qt::QueuedConnection);
    // 杩涘叆 Checking 鏃讹紝鑷姩閲嶇疆 Live Log
    connect(task, &SH85PeriodicSelfCheckTask2::taskStateChanged,
            this, [this](SH85PeriodicSelfCheckTask2::State s) {
                if (s == SH85PeriodicSelfCheckTask2::State::Checking && m_reportDialog) {
                    m_reportDialog->onRoundStarted();
                }
            },
            Qt::QueuedConnection);

    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][SH85PeriodicSelfCheckSettingWidget] bound to resident SH85PeriodicSelfCheckTask2").toStdString());
}

// ============================================================
// UI 浜嬩欢
// ============================================================

void SH85PeriodicSelfCheckSettingWidget::onEnableComboChanged(int index)
{
    // 鍚敤涓嬫媺妗嗘敼鍙樹簨浠跺鐞?    const bool enable = (index == 1);
    if (enable == m_isEnabled) return;
    m_isEnabled = enable;

    qDebug() << "[SH85PeriodicSelfCheckSettingWidget] enable changed:" << enable;
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][SH85PeriodicSelfCheckSettingWidget] enable=%1").arg(enable).toStdString());

    // 璋冪敤浠诲姟鐨?setEnabled 鏂规硶
    if (m_task) {
        QMetaObject::invokeMethod(m_task.data(), "setEnabled",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, enable));
    }

    // 鎸佷箙鍖栧埌閰嶇疆
    OHBDeviceConfig::getInstance().setSH85SelfCheckEnabled(enable);

    // 鈥斺€?UI 绔嬪嵆鍙嶉锛堢湡瀹炵姸鎬佷細闅?taskStateChanged 淇″彿鍚屾瑕嗙洊锛夆€斺€?    // 鍚敤锛氱珛鍗虫彁绀恒€岃嚜妫€涓紙鎵ц锛?s锛夈€嶏紱
    // 鍋滅敤锛氳嫢褰撳墠姝ｅ湪 Checking锛屼笉淇敼鐘舵€佹枃妗堬紙缁х画鏄剧ず鑷涓紝鐩村埌鏈疆瀹屾垚锛夈€?    if (enable) {
        m_currentTaskState = SH85PeriodicSelfCheckTask2::State::Checking;
        m_elapsedSec = 0;
        refreshStatusText();
    }
    // disable 鎯呭喌涓嶇珛鍒诲埛鏂版枃妗堬紝璁?task 鑷劧缁撴潫鍚庨€氳繃 taskStateChanged 鍙嶉

    // 閫氱煡 dialog 閲嶇疆 Live Log锛堝紑鍚柊涓€杞椂锛?    if (enable && m_reportDialog) {
        QMetaObject::invokeMethod(m_reportDialog.data(), "onRoundStarted",
                                  Qt::QueuedConnection);
    }

    emit runningStateChanged(enable);
}

void SH85PeriodicSelfCheckSettingWidget::onSetBtnClicked()
{
    // 璁剧疆鎸夐挳鐐瑰嚮浜嬩欢澶勭悊
    const int value = m_periodSpinBox->value();
    const auto unit = unitFromIndex(m_unitCombo->currentIndex());
    const QString unitStr = SH85PeriodicSelfCheckTask2::timeUnitToString(unit);

    qDebug() << "[SH85PeriodicSelfCheckSettingWidget] period set:" << value << unitStr;
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][SH85PeriodicSelfCheckSettingWidget] setPeriod=%1 %2")
            .arg(value).arg(unitStr).toStdString());

    // 璁＄畻鍛ㄦ湡绉掓暟骞跺啓鍏ラ厤缃?    int seconds = value;
    switch (unit) {
    case SH85PeriodicSelfCheckTask2::TimeUnit::Second: seconds = value; break;
    case SH85PeriodicSelfCheckTask2::TimeUnit::Minute: seconds = value * 60; break;
    case SH85PeriodicSelfCheckTask2::TimeUnit::Hour:   seconds = value * 3600; break;
    }
    OHBDeviceConfig::getInstance().setSH85SelfCheckPeriodSeconds(seconds);

    // 璋冪敤浠诲姟鐨?setPeriod 鏂规硶
    if (m_task) {
        QMetaObject::invokeMethod(m_task.data(), "setPeriod",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, value),
                                  Q_ARG(SH85PeriodicSelfCheckTask2::TimeUnit, unit));
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
    // 鎶ュ憡鎸夐挳鐐瑰嚮浜嬩欢澶勭悊锛氭樉绀烘姤鍛婂璇濇
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
// task 鍥炶皟
// ============================================================

void SH85PeriodicSelfCheckSettingWidget::onTaskStateChanged(SH85PeriodicSelfCheckTask2::State state)
{
    // 浠诲姟鐘舵€佹敼鍙樺洖璋?    m_currentTaskState = state;
    if (state == SH85PeriodicSelfCheckTask2::State::Checking) {
        m_elapsedSec = 0;
    } else if (state == SH85PeriodicSelfCheckTask2::State::WaitingNext) {
        m_intervalRemainSec = m_task ? m_task->periodSeconds() : 0;
    }
    refreshStatusText();
}

void SH85PeriodicSelfCheckSettingWidget::onTaskElapsedTick(int elapsedSeconds)
{
    // 鑷璁℃椂鍣ㄥ洖璋?    m_elapsedSec = elapsedSeconds;
    if (m_currentTaskState == SH85PeriodicSelfCheckTask2::State::Checking) {
        refreshStatusText();
    }
}

void SH85PeriodicSelfCheckSettingWidget::onTaskIntervalCountdown(int remainingSeconds)
{
    // 闂撮殧鍊掕鏃跺洖璋?    m_intervalRemainSec = remainingSeconds;
    if (m_currentTaskState == SH85PeriodicSelfCheckTask2::State::WaitingNext) {
        refreshStatusText();
    }
}

void SH85PeriodicSelfCheckSettingWidget::onTaskBootDelayCountdown(int remainingSeconds)
{
    m_bootDelayRemainSec = remainingSeconds;
    if (m_currentTaskState == SH85PeriodicSelfCheckTask2::State::Stopped) {
        if (m_statusEdit) {
            m_statusEdit->setText(QString("Boot delay (countdown: %1s)").arg(m_bootDelayRemainSec));
        }
    }
}

void SH85PeriodicSelfCheckSettingWidget::refreshStatusText()
{
    // 鍒锋柊鐘舵€佹枃鏈細鏍规嵁褰撳墠浠诲姟鐘舵€佹樉绀轰笉鍚屾枃妗?    if (!m_statusEdit) return;

    QString text;
    switch (m_currentTaskState) {
    case SH85PeriodicSelfCheckTask2::State::Stopped:
        if (m_bootDelayRemainSec > 0) {
            text = QString("Boot delay (countdown: %1s)").arg(m_bootDelayRemainSec);
        } else {
            text = QStringLiteral("Self-check disabled");
        }
        break;
    case SH85PeriodicSelfCheckTask2::State::Checking:
        text = QStringLiteral("Self-checking (elapsed: %1s)").arg(m_elapsedSec);
        break;
    case SH85PeriodicSelfCheckTask2::State::WaitingNext:
        text = QStringLiteral("Waiting for next check (countdown: %1s)").arg(m_intervalRemainSec);
        break;
    }
    m_statusEdit->setText(text);
}

// ============================================================
// 宸ュ叿
// ============================================================

SH85PeriodicSelfCheckTask2::TimeUnit SH85PeriodicSelfCheckSettingWidget::unitFromIndex(int idx)
{
    // 浠庝笅鎷夋绱㈠紩杞崲涓烘椂闂村崟浣?    switch (idx) {
    case 0: return SH85PeriodicSelfCheckTask2::TimeUnit::Second;
    case 1: return SH85PeriodicSelfCheckTask2::TimeUnit::Minute;
    case 2: return SH85PeriodicSelfCheckTask2::TimeUnit::Hour;
    default: return SH85PeriodicSelfCheckTask2::TimeUnit::Minute;
    }
}

void SH85PeriodicSelfCheckSettingWidget::refreshActionControlsState()
{
    // 鍒锋柊鎿嶄綔鎺т欢鐨勫惎鐢ㄧ姸鎬?    if (m_enableCombo) {
        m_enableCombo->setEnabled(m_periodicActionEnabled);
    }
    if (m_periodSpinBox) {
        m_periodSpinBox->setEnabled(m_periodicActionEnabled);
    }
    if (m_unitCombo) {
        m_unitCombo->setEnabled(m_periodicActionEnabled);
    }
    if (m_setBtn) {
        m_setBtn->setEnabled(m_periodicActionEnabled);
    }
}

void SH85PeriodicSelfCheckSettingWidget::setEnabled(bool enabled)
{
    // 璁剧疆鎺т欢鏄惁鍚敤
    QWidget::setEnabled(enabled);

    if (m_enableItem) m_enableItem->setEnabled(enabled);
    if (m_periodItem) m_periodItem->setEnabled(enabled);
    if (m_statusItem) m_statusItem->setEnabled(enabled);
    if (m_reportItem) m_reportItem->setEnabled(enabled);
    refreshActionControlsState();
}

void SH85PeriodicSelfCheckSettingWidget::setPeriodicActionEnabled(bool enabled)
{
    // 璁剧疆鍛ㄦ湡鎬ф搷浣滄帶浠剁殑鍚敤鐘舵€侊紙鐢ㄤ簬椤甸潰绾т簰鏂ワ級
    if (m_periodicActionEnabled == enabled) return;
    m_periodicActionEnabled = enabled;
    refreshActionControlsState();
}

