#include "idlepurgesettingwidget.h"

#include "../settingwidget/settingitemwidget.h"
#include "modaltabledialog.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "scheduler/scheduler.h"

#include <QColor>
#include <QList>
#include <QMessageBox>

namespace {
QList<QStringList> buildDeviceResultRows(const QStringList &targetQrCodes,
                                         const QStringList &failedQrCodes,
                                         bool allSuccess,
                                         const QString &propertyName,
                                         quint16 setValue)
{
    QStringList qrcodes = targetQrCodes;
    for (const QString &qrCode : failedQrCodes) {
        if (!qrcodes.contains(qrCode)) {
            qrcodes.append(qrCode);
        }
    }

    QString settingValueStr;
    if (propertyName == "Idle Purge Enable") {
        settingValueStr = setValue == 1 ? "Set enable True" : "Set enable False";
    } else if (propertyName == "Purge Duration") {
        settingValueStr = QString("Set Purgetime %1 s").arg(setValue);
    } else if (propertyName == "Purge Interval") {
        settingValueStr = QString("Set Interval %1 s").arg(setValue);
    } else {
        settingValueStr = QString("Set %1 %2").arg(propertyName).arg(setValue);
    }

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
    if (!dialog) {
        return;
    }

    dialog->setFieldTextColor(QStringLiteral("Result"), QStringLiteral("Fail"), QColor(210, 0, 0));
    dialog->setFieldTextColor(QStringLiteral("Result"), QStringLiteral("Success"), QColor(0, 150, 0));
}
} // namespace

IdlePurgeSettingWidget::IdlePurgeSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_preparationTimeLineEdit(nullptr)
    , m_enableComboBox(nullptr)
    , m_durationSpinBox(nullptr)
    , m_intervalSpinBox(nullptr)
    , m_enableSetBtn(nullptr)
    , m_durationSetBtn(nullptr)
    , m_intervalSetBtn(nullptr)
    , m_enableItem(nullptr)
    , m_durationItem(nullptr)
    , m_intervalItem(nullptr)
{
    setTitle("Idle Purge Configuration");
    initUI();
}

IdlePurgeSettingWidget::~IdlePurgeSettingWidget()
{
}

void IdlePurgeSettingWidget::initUI()
{
    initPreparationTimeItem();
    initEnableItem();
    initDurationItem();
    initIntervalItem();
}

void IdlePurgeSettingWidget::setConfigValues(bool enabled,
                                             int purgeDurationSeconds,
                                             int purgeIntervalSeconds)
{
    if (m_enableComboBox) {
        const int index = m_enableComboBox->findData(enabled ? 1 : 0);
        if (index >= 0) {
            m_enableComboBox->setCurrentIndex(index);
        }
    }

    if (m_durationSpinBox) {
        m_durationSpinBox->setValue(purgeDurationSeconds);
    }

    if (m_intervalSpinBox) {
        m_intervalSpinBox->setValue(purgeIntervalSeconds);
    }
}

void IdlePurgeSettingWidget::initPreparationTimeItem()
{
    auto *item = new SettingItemWidget(this);
    item->setTitle("Preparation Time");
    item->setTip("Fixed preparation phase duration before idle purge starts");

    m_preparationTimeLineEdit = new QLineEdit(item);
    m_preparationTimeLineEdit->setText("10 s");
    m_preparationTimeLineEdit->setReadOnly(true);
    m_preparationTimeLineEdit->setFixedWidth(120);
    item->addWidget("prep_time_edit", m_preparationTimeLineEdit);

    addItem(item);
}

void IdlePurgeSettingWidget::initEnableItem()
{
    m_enableItem = new SettingItemWidget(this);
    m_enableItem->setTitle("Idle Purge Enable");
    m_enableItem->setTip("Enable or disable idle purge function for all devices");

    m_enableComboBox = new QComboBox(m_enableItem);
    m_enableComboBox->addItem("Enable", 1);
    m_enableComboBox->addItem("Disable", 0);
    m_enableComboBox->setFixedWidth(120);
    m_enableItem->addWidget("enable_combo", m_enableComboBox);

    m_enableSetBtn = new QPushButton("Set", m_enableItem);
    m_enableItem->addWidget("enable_set_btn", m_enableSetBtn);
    connect(m_enableSetBtn, &QPushButton::clicked,
            this, &IdlePurgeSettingWidget::onEnableSetBtnClicked);

    addItem(m_enableItem);
}

void IdlePurgeSettingWidget::initDurationItem()
{
    m_durationItem = new SettingItemWidget(this);
    m_durationItem->setTitle("Purge Duration");
    m_durationItem->setTip("Set idle purge inflation duration (seconds) for all devices");

    m_durationSpinBox = new QSpinBox(m_durationItem);
    m_durationSpinBox->setRange(1, 9999);
    m_durationSpinBox->setValue(10);
    m_durationSpinBox->setSuffix(" s");
    m_durationSpinBox->setFixedWidth(120);
    m_durationItem->addWidget("duration_spin", m_durationSpinBox);

    m_durationSetBtn = new QPushButton("Set", m_durationItem);
    m_durationItem->addWidget("duration_set_btn", m_durationSetBtn);
    connect(m_durationSetBtn, &QPushButton::clicked,
            this, &IdlePurgeSettingWidget::onDurationSetBtnClicked);

    addItem(m_durationItem);
}

void IdlePurgeSettingWidget::initIntervalItem()
{
    m_intervalItem = new SettingItemWidget(this);
    m_intervalItem->setTitle("Purge Interval");
    m_intervalItem->setTip("Set idle purge interval between cycles (seconds) for all devices");

    m_intervalSpinBox = new QSpinBox(m_intervalItem);
    m_intervalSpinBox->setRange(1, 99999);
    m_intervalSpinBox->setValue(5);
    m_intervalSpinBox->setSuffix(" s");
    m_intervalSpinBox->setFixedWidth(120);
    m_intervalItem->addWidget("interval_spin", m_intervalSpinBox);

    m_intervalSetBtn = new QPushButton("Set", m_intervalItem);
    m_intervalItem->addWidget("interval_set_btn", m_intervalSetBtn);
    connect(m_intervalSetBtn, &QPushButton::clicked,
            this, &IdlePurgeSettingWidget::onIntervalSetBtnClicked);

    addItem(m_intervalItem);
}

void IdlePurgeSettingWidget::onEnableSetBtnClicked()
{
    const quint16 value = static_cast<quint16>(m_enableComboBox->currentData().toInt());
    submitCommand(m_enableItem, SetIdlePurgeTask::IdlePurgeProperty::Enable, value);
}

void IdlePurgeSettingWidget::onDurationSetBtnClicked()
{
    const quint16 value = static_cast<quint16>(m_durationSpinBox->value());
    submitCommand(m_durationItem, SetIdlePurgeTask::IdlePurgeProperty::PurgeTime, value);
}

void IdlePurgeSettingWidget::onIntervalSetBtnClicked()
{
    const quint16 value = static_cast<quint16>(m_intervalSpinBox->value());
    submitCommand(m_intervalItem, SetIdlePurgeTask::IdlePurgeProperty::PurgeInterval, value);
}

void IdlePurgeSettingWidget::submitCommand(SettingItemWidget *item,
                                           SetIdlePurgeTask::IdlePurgeProperty property,
                                           quint16 value)
{
    auto *task = new SetIdlePurgeTask(property, value);
    const QStringList targetQrCodes = ModbusTcpMasterManager::instance().masterIds();

    item->setStatusWaiting();
    setAllSetButtonsEnabled(false);

    connect(task, &SetIdlePurgeTask::deviceRetrying, this,
            [item](const QString &qrCode, int retryCount, int maxRetry) {
                item->setStatusWaiting(QString("Retrying %1 (%2/%3)")
                                           .arg(qrCode)
                                           .arg(retryCount)
                                           .arg(maxRetry));
            },
            Qt::QueuedConnection);

    connect(task, &SetIdlePurgeTask::allFinished, this,
            [this, item, targetQrCodes](bool allSuccess,
                                        int successCount,
                                        QStringList failedQrCodes,
                                        QString propertyName,
                                        quint16 setValue) {
                const bool persistFailed = !allSuccess
                    && failedQrCodes.isEmpty()
                    && successCount > 0;
                const bool hasFailure = !allSuccess || !failedQrCodes.isEmpty();
                const bool showDeviceTable = targetQrCodes.size() > 1 || failedQrCodes.size() > 1;

                if (!hasFailure) {
                    item->setStatusOK();
                    if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Idle Purge %1=%2 Result").arg(propertyName).arg(setValue),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(targetQrCodes, failedQrCodes, true, propertyName, setValue));
                        applyResultColors(dialog);
                    } else {
                        auto *mb = new QMessageBox(QMessageBox::Information,
                                                  "Set Succeeded",
                                                  QString("Successfully set [%1] to [%2] on %3 device(s)")
                                                      .arg(propertyName)
                                                      .arg(setValue)
                                                      .arg(successCount),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                } else {
                    item->setStatusFailed();
                    if (persistFailed) {
                        auto *mb = new QMessageBox(QMessageBox::Warning,
                                                  "Set Failed",
                                                  QString("[%1] was written to device(s), but local config persistence failed.")
                                                      .arg(propertyName),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    } else if (showDeviceTable) {
                        auto *dialog = ModalTableDialog::showAsync(
                            this,
                            QString("Idle Purge %1=%2 Result").arg(propertyName).arg(setValue),
                            QStringList{"QRCode", "Setting", "Result"},
                            buildDeviceResultRows(targetQrCodes, failedQrCodes, false, propertyName, setValue));
                        applyResultColors(dialog);
                    } else {
                        const QString failList = failedQrCodes.join(", ");
                        auto *mb = new QMessageBox(QMessageBox::Warning,
                                                  "Set Failed",
                                                  QString("Failed to set [%1] to [%2] on %3 device(s):\n%4")
                                                      .arg(propertyName)
                                                      .arg(setValue)
                                                      .arg(failedQrCodes.count())
                                                      .arg(failList),
                                                  QMessageBox::Ok,
                                                  this);
                        mb->setAttribute(Qt::WA_DeleteOnClose);
                        mb->setModal(false);
                        mb->setWindowModality(Qt::NonModal);
                        mb->show();
                    }
                }

                setAllSetButtonsEnabled(true);
            });

    Scheduler::instance()->submitTask(task);
}

void IdlePurgeSettingWidget::setAllSetButtonsEnabled(bool enabled)
{
    if (m_enableSetBtn) {
        m_enableSetBtn->setEnabled(enabled);
    }
    if (m_durationSetBtn) {
        m_durationSetBtn->setEnabled(enabled);
    }
    if (m_intervalSetBtn) {
        m_intervalSetBtn->setEnabled(enabled);
    }
}
