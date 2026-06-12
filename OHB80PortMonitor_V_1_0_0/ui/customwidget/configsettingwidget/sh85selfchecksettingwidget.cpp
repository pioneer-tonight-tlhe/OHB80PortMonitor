#include "sh85selfchecksettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "app/applogger.h"
#include "loggermanager.h"
#include "app/shareddata.h"

#include <QDebug>
#include <QMessageBox>

namespace {
constexpr int kPhase2BoundarySec = 10;   // 阶段 2 起点对应的剩余秒数
constexpr int kButtonInitialSec  = 60;   // 按钮 "Processing (60)" 初值
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

SH85SelfCheckSettingWidget::SH85SelfCheckSettingWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("SH85 Self-check Configuration");
    initUI();
}

SH85SelfCheckSettingWidget::~SH85SelfCheckSettingWidget() = default;

// ============================================================
// UI 初始化
// ============================================================

void SH85SelfCheckSettingWidget::initUI()
{
    initDeviceIdItem();
    initSelfCheckItem();
}

void SH85SelfCheckSettingWidget::initDeviceIdItem()
{
    m_deviceIdItem = new SettingItemWidget(this);
    m_deviceIdItem->setTitle("Target Device ID");
    m_deviceIdItem->setTip("Enter target device ID (0 ~ 99999) for SH85 self-check");

    m_deviceIdSpinBox = new QSpinBox(m_deviceIdItem);
    m_deviceIdSpinBox->setRange(0, 99999);
    m_deviceIdSpinBox->setFixedWidth(160);

    // 设置默认值为第一个 qrcode
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int firstDeviceId = qrcodes.first().toInt(&ok);
        if (ok) {
            m_deviceIdSpinBox->setValue(firstDeviceId);
        }
    }

    m_deviceIdItem->addWidget("device_id_spinbox", m_deviceIdSpinBox);

    addItem(m_deviceIdItem);
}

void SH85SelfCheckSettingWidget::initSelfCheckItem()
{
    m_selfCheckItem = new SettingItemWidget(this);
    m_selfCheckItem->setTitle("SH85 Self-check");
    m_selfCheckItem->setTip("Trigger SH85 humidity sensor self-check (about 70 seconds)");

    m_checkBtn = new QPushButton("Check", m_selfCheckItem);
    m_checkBtn->setFixedWidth(140);
    m_selfCheckItem->addWidget("self_check_btn", m_checkBtn);

    connect(m_checkBtn, &QPushButton::clicked,
            this, &SH85SelfCheckSettingWidget::onCheckBtnClicked);

    refreshActionState();
    addItem(m_selfCheckItem);
}


// ============================================================
// 槽 - 按钮点击
// ============================================================

void SH85SelfCheckSettingWidget::onCheckBtnClicked()
{
    const int deviceId = m_deviceIdSpinBox->value();
    const QString qrcode = QString::number(deviceId);
    if (qrcode.isEmpty()) {
        QMessageBox::warning(this, "Self-check Failed", "Please enter a valid device ID");
        return;
    }

    submitSelfCheckTask(qrcode);
}

// ============================================================
// 提交任务并连接反馈信号
// ============================================================

void SH85SelfCheckSettingWidget::submitSelfCheckTask(const QString &qrcode)
{
    m_runningQrcode = qrcode;

    // 锁定 UI：按钮禁用 + 初始倒计时文案
    m_checkBtn->setText(QString("Processing (%1)").arg(kButtonInitialSec));
    refreshActionState();
    m_selfCheckItem->setStatusWaiting();

    emit runningStateChanged(true);

    auto *task = new SH85SelfCheckTask(qrcode);

    connect(task, &SH85SelfCheckTask::countdownTick,
            this, &SH85SelfCheckSettingWidget::onCountdownTick);
    connect(task, &SH85SelfCheckTask::statusChanged,
            this, &SH85SelfCheckSettingWidget::onStatusChanged);
    connect(task, &SH85SelfCheckTask::allFinished,
            this, &SH85SelfCheckSettingWidget::onAllFinished);

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][SH85SelfCheckSettingWidget][submitSelfCheckTask]：提交任务 qrcode=" << qrcode;
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][SH85SelfCheckSettingWidget][submitSelfCheckTask]：提交任务 qrcode=%1")
            .arg(qrcode).toStdString());
}

// ============================================================
// 槽 - 任务反馈
// ============================================================

void SH85SelfCheckSettingWidget::onCountdownTick(int remainingSeconds, const QString &qrcode)
{
    if (qrcode != m_runningQrcode) return;

    // 仅在第一阶段（剩余 > 10s）使用 countdownTick 驱动按钮
    // 阶段 2（剩余 ≤ 10s）由 statusChanged "Checking (N)" 接管
    if (remainingSeconds > kPhase2BoundarySec) {
        const int procSec = remainingSeconds - kPhase2BoundarySec;   // 60 → 1
        m_checkBtn->setText(QString("Processing (%1)").arg(procSec));
    }
}

void SH85SelfCheckSettingWidget::onStatusChanged(const QString &text, const QString &qrcode)
{
    if (qrcode != m_runningQrcode) return;

    // 阶段 2 倒计时文本（"Checking (N)"）直接覆盖按钮
    if (text.startsWith(QStringLiteral("Checking"))) {
        m_checkBtn->setText(text);
    }
    // 其他状态文本（终态文案）此处不处理，由 onAllFinished 收尾
}

void SH85SelfCheckSettingWidget::onAllFinished(bool success,
                                               SH85SelfChecker::Result result,
                                               const QString &qrcode)
{
    if (qrcode != m_runningQrcode) return;
    m_runningQrcode.clear();

    emit runningStateChanged(false);

    const QString friendly = resultToFriendlyText(result);

    if (success) {
        m_selfCheckItem->setStatusOK();
        QMessageBox::information(
            this,
            "Self-check Succeeded",
            QString("SH85 self-check completed successfully on device [%1]").arg(qrcode));
    } else {
        m_selfCheckItem->setStatusFailed();
        QMessageBox::warning(
            this,
            "Self-check Failed",
            QString("SH85 self-check failed on device [%1]:\n%2").arg(qrcode).arg(friendly));
    }

    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(),
        success ? Level::INFO : Level::WARN,
        QString("[ui][SH85SelfCheckSettingWidget][onAllFinished]：qrcode=%1 result=%2")
            .arg(qrcode).arg(friendly).toStdString());

    resetButton();
}

// ============================================================
// 内部辅助
// ============================================================

void SH85SelfCheckSettingWidget::resetButton()
{
    m_checkBtn->setText("Check");
    refreshActionState();
}

void SH85SelfCheckSettingWidget::refreshActionState()
{
    const bool running = isRunning();
    if (m_checkBtn) {
        m_checkBtn->setEnabled(m_checkActionEnabled && !running);
    }
    if (m_deviceIdSpinBox) {
        // 互斥只限制动作入口按钮，参数输入仅在任务执行中锁定
        m_deviceIdSpinBox->setEnabled(!running);
    }
}

QString SH85SelfCheckSettingWidget::resultToFriendlyText(SH85SelfChecker::Result r)
{
    switch (r) {
    case SH85SelfChecker::Result::Success:                  return "Self-check OK";
    case SH85SelfChecker::Result::StartCommandFailed:
    case SH85SelfChecker::Result::ReadEarlyCommandFailed:
    case SH85SelfChecker::Result::ReadPollCommandFailed:
        return "Network Error";
    case SH85SelfChecker::Result::DeviceNotEntered:          return "Device not in self-check";
    case SH85SelfChecker::Result::FirmwareAbnormal:          return "Firmware abnormal";
    case SH85SelfChecker::Result::HumidityExceeded:          return "Humidity exceeded threshold";
    case SH85SelfChecker::Result::SensorCommError:           return "SH85 sensor comm error";
    case SH85SelfChecker::Result::ThresholdParamError:       return "Threshold parameter error";
    case SH85SelfChecker::Result::Timeout:                   return "Self-check timeout";
    case SH85SelfChecker::Result::Cancelled:                 return "Cancelled";
    }
    return "Unknown";
}

// ============================================================
// enable 方法
// ============================================================

void SH85SelfCheckSettingWidget::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);

    // 禁用/启用所有子控件
    if (m_deviceIdItem) m_deviceIdItem->setEnabled(enabled);
    if (m_selfCheckItem) m_selfCheckItem->setEnabled(enabled);
    refreshActionState();
}

bool SH85SelfCheckSettingWidget::isRunning() const
{
    return !m_runningQrcode.isEmpty();
}

void SH85SelfCheckSettingWidget::setCheckActionEnabled(bool enabled)
{
    if (m_checkActionEnabled == enabled) return;
    m_checkActionEnabled = enabled;
    refreshActionState();
}
