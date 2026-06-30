#include "purgeflowsettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "modaltabledialog.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_purge_flow_task/set_purge_flow_task.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "app/applogger.h"
#include "loggermanager.h"
#include "ohbdeviceconfig.h"

#include <QDebug>
#include <QMessageBox>
#include <QColor>
#include <QVector>

namespace {
QList<QStringList> buildDeviceResultRows(const QStringList &targetQrCodes,
                                         const QStringList &failedQrCodes,
                                         bool allSuccess,
                                         int flowValue)
{
    QStringList qrcodes = targetQrCodes;
    for (const QString &qrCode : failedQrCodes) {
        if (!qrcodes.contains(qrCode)) {
            qrcodes.append(qrCode);
        }
    }

    // 格式化设置值（带单位）
    QString settingValueStr = QString("Set Flow %1 L/Min").arg(flowValue);

    QList<QStringList> rows;
    for (const QString &qrCode : qrcodes) {
        const bool failed = !allSuccess
            && (failedQrCodes.isEmpty() || failedQrCodes.contains(qrCode));
        rows.append({
            qrCode,
            settingValueStr,
            failed ? QStringLiteral("Fail") : QStringLiteral("Success")
        });
    }
    return rows;
}

void applyResultColors(ModalTableDialog *dialog)
{
    if (!dialog) return;
    dialog->setFieldTextColor(QStringLiteral("Result"), QStringLiteral("Fail"), QColor(210, 0, 0));
    dialog->setFieldTextColor(QStringLiteral("Result"), QStringLiteral("Success"), QColor(0, 150, 0));
}
} // namespace

PurgeFlowSettingWidget::PurgeFlowSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_qrcodeSpinBox(nullptr)
    , m_flowSpinBox(nullptr)
    , m_flowSetBtn(nullptr)
    , m_flowSetAllBtn(nullptr)
    , m_qrcodeItem(nullptr)
    , m_flowItem(nullptr)
{
    setTitle("Purge Flow Configuration");
    initUI();
}

PurgeFlowSettingWidget::~PurgeFlowSettingWidget() = default;

void PurgeFlowSettingWidget::setInitialConfigValue(int purgeFlowLitersPerMinute)
{
    if (m_flowSpinBox) {
        m_flowSpinBox->setValue(purgeFlowLitersPerMinute);
    }
}

// ============================================================
// UI
// ============================================================

void PurgeFlowSettingWidget::initUI()
{
    initQrcodeItem();
    initFlowItem();
}

void PurgeFlowSettingWidget::initQrcodeItem()
{
    m_qrcodeItem = new SettingItemWidget(this);
    m_qrcodeItem->setTitle("Target Device");
    m_qrcodeItem->setTip("Target device QRCode (numeric, used by Set button)");

    m_qrcodeSpinBox = new QSpinBox(m_qrcodeItem);
    m_qrcodeSpinBox->setRange(0, 99999);
    m_qrcodeSpinBox->setFixedWidth(160);

    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int v = qrcodes.first().toInt(&ok);
        if (ok) m_qrcodeSpinBox->setValue(v);
        else qWarning() << "[ui][PurgeFlowSettingWidget] qrcode 转换 int 失败:" << qrcodes.first();
    }

    m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    connect(m_qrcodeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int value) {
                loadFlowFromConfig(QString::number(value));
            });
    addItem(m_qrcodeItem);
}

void PurgeFlowSettingWidget::initFlowItem()
{
    m_flowItem = new SettingItemWidget(this);
    m_flowItem->setTitle("Purge Flow");
    m_flowItem->setTip("Set VEFC purge flow (only valid in FOUP IN). Register value = flow x 10");

    m_flowSpinBox = new QSpinBox(m_flowItem);
    m_flowSpinBox->setRange(0, 650);
    m_flowSpinBox->setValue(35);
    m_flowSpinBox->setSuffix(" L/Min");
    m_flowSpinBox->setFixedWidth(120);
    m_flowItem->addWidget("flow_spin", m_flowSpinBox);

    m_flowSetBtn = new QPushButton("Set", m_flowItem);
    m_flowItem->addWidget("flow_set_btn", m_flowSetBtn);
    connect(m_flowSetBtn, &QPushButton::clicked,
            this, &PurgeFlowSettingWidget::onSetBtnClicked);

    m_flowSetAllBtn = new QPushButton("Set All", m_flowItem);
    m_flowItem->addWidget("flow_set_all_btn", m_flowSetAllBtn);
    connect(m_flowSetAllBtn, &QPushButton::clicked,
            this, &PurgeFlowSettingWidget::onSetAllBtnClicked);

    addItem(m_flowItem);
}

// ============================================================
// 槽
// ============================================================

void PurgeFlowSettingWidget::onSetBtnClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    submitTask(QStringList{qrcode}, m_flowSpinBox->value());
}

void PurgeFlowSettingWidget::onSetAllBtnClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Set Failed", "No target device available");
        return;
    }
    submitTask(qrcodes, m_flowSpinBox->value());
}

// ============================================================
// 提交任务
// ============================================================

void PurgeFlowSettingWidget::submitTask(const QStringList &qrcodes, int flowValue)
{
    const QVector<QString> qrcodeVec(qrcodes.begin(), qrcodes.end());
    auto *task = new SetPurgeFlowTask(qrcodeVec, flowValue);

    m_flowItem->setStatusWaiting();
    // 任务运行期间禁用两个 Set 按钮，避免并发任务提交
    setAllSetButtonsEnabled(false);

    connect(task, &SetPurgeFlowTask::deviceRetrying, this,
            [this](const QString &qrCode, int retryCount, int maxRetry) {
                m_flowItem->setStatusWaiting(QString("Retrying %1 (%2/%3)")
                    .arg(qrCode)
                    .arg(retryCount)
                    .arg(maxRetry));
            },
            Qt::QueuedConnection);

    connect(task, &SetPurgeFlowTask::allFinished,
            this, [this, qrcodes](bool /*allSuccess*/, int successCount,
                         QStringList failedQrCodes, int flowFinal) {
                const bool hasFailure = !failedQrCodes.isEmpty();
                const bool showDeviceTable = qrcodes.size() > 1 || failedQrCodes.size() > 1;
                if (!hasFailure) {
                    m_flowItem->setStatusOK();
                    if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Purge Flow=%1 Result").arg(flowFinal),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(qrcodes, failedQrCodes, true, flowFinal));
                        applyResultColors(dialog);
                    } else {
                        auto *mb = new QMessageBox(QMessageBox::Information,
                                                  "Set Succeeded",
                                                  QString("Successfully set Purge Flow=[%1] on %2 device(s)")
                                                      .arg(flowFinal).arg(successCount),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                } else {
                    m_flowItem->setStatusFailed();
                    if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Purge Flow=%1 Result").arg(flowFinal),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(qrcodes, failedQrCodes, false, flowFinal));
                        applyResultColors(dialog);
                    } else {
                        const QString failList = failedQrCodes.join(", ");
                        auto *mb = new QMessageBox(QMessageBox::Warning,
                                                  "Set Failed",
                                                  QString("Failed to set Purge Flow=[%1] on %2 device(s):\n%3")
                                                  .arg(flowFinal).arg(failedQrCodes.count()).arg(failList),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                }
                loadFlowFromConfig(m_qrcodeSpinBox ? QString::number(m_qrcodeSpinBox->value()) : QString());
                setAllSetButtonsEnabled(true);
            });

    Scheduler::instance()->submitTask(task);

    qDebug() << "[ui][PurgeFlowSettingWidget][submitTask]：提交任务 设备数="
             << qrcodes.size() << "flow=" << flowValue;
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][PurgeFlowSettingWidget][submitTask]：提交任务 设备数=%1 flow=%2")
            .arg(qrcodes.size()).arg(flowValue).toStdString());
}

void PurgeFlowSettingWidget::loadFlowFromConfig(const QString &qrCode)
{
    if (!m_flowSpinBox || qrCode.isEmpty()) {
        return;
    }

    const OHBDeviceConfigInfo deviceInfo = OHBDeviceConfig::getInstance().getDeviceByQRCode(qrCode);
    if (deviceInfo.getQrCode().isEmpty()) {
        return;
    }

    m_flowSpinBox->setValue(deviceInfo.getPurgeFlowLitersPerMinute());
}

void PurgeFlowSettingWidget::setAllSetButtonsEnabled(bool enabled)
{
    if (m_flowSetBtn) {
        m_flowSetBtn->setEnabled(enabled);
    }
    if (m_flowSetAllBtn) {
        m_flowSetAllBtn->setEnabled(enabled);
    }
}
