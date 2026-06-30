#include "foupinautopurgeenablewidget.h"

#include "../modaltabledialog/modaltabledialog.h"
#include "../settingwidget/settingitemwidget.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "loggermanager.h"
#include "ohbdeviceconfig.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_foup_in_auto_purge_enable_task/set_foup_in_auto_purge_enable_task.h"

#include <QColor>
#include <QMessageBox>
#include <QtGlobal>
#include <QVector>

FoupInAutoPurgeEnableWidget::FoupInAutoPurgeEnableWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle(QStringLiteral("FOUPIN Automatic Purge Enable"));
    initUI();

    if (m_qrcodeSpinBox) {
        loadEnableFromConfig(QString::number(m_qrcodeSpinBox->value()));
    }
}

FoupInAutoPurgeEnableWidget::~FoupInAutoPurgeEnableWidget() = default;

void FoupInAutoPurgeEnableWidget::setInitialConfigValue(int enableValue)
{
    if (!m_enableComboBox) {
        return;
    }

    const int comboIndex = m_enableComboBox->findData(qBound(0, enableValue, 1));
    if (comboIndex >= 0) {
        m_enableComboBox->setCurrentIndex(comboIndex);
    }
}

void FoupInAutoPurgeEnableWidget::initUI()
{
    initTargetItem();
    initControlItem();
}

void FoupInAutoPurgeEnableWidget::initTargetItem()
{
    m_targetItem = new SettingItemWidget(this);
    m_targetItem->setTitle(QStringLiteral("Target Device"));
    m_targetItem->setTip(QStringLiteral("Choose the target device QRCode"));

    m_qrcodeSpinBox = new QSpinBox(m_targetItem);
    m_qrcodeSpinBox->setRange(0, 99999);
    m_qrcodeSpinBox->setFixedWidth(160);

    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int qrcodeValue = qrcodes.first().toInt(&ok);
        if (ok) {
            m_qrcodeSpinBox->setValue(qrcodeValue);
        }
    }

    m_targetItem->addWidget(QStringLiteral("qrcode_spin"), m_qrcodeSpinBox);
    connect(m_qrcodeSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) { loadEnableFromConfig(QString::number(value)); });
    addItem(m_targetItem);
}

void FoupInAutoPurgeEnableWidget::initControlItem()
{
    m_controlItem = new SettingItemWidget(this);
    m_controlItem->setTitle(QStringLiteral("Foup In Auto Purge Enable"));
    m_controlItem->setTip(QStringLiteral("0 disables auto purge; 1 enables auto purge and 30 min humidity check"));

    m_enableComboBox = new QComboBox(m_controlItem);
    m_enableComboBox->addItem(QStringLiteral("0 - Disabled"), 0);
    m_enableComboBox->addItem(QStringLiteral("1 - Enabled"), 1);
    m_enableComboBox->setFixedWidth(260);
    m_controlItem->addWidget(QStringLiteral("enable_combo"), m_enableComboBox);

    m_setSingleBtn = new QPushButton(QStringLiteral("Set Single"), m_controlItem);
    m_controlItem->addWidget(QStringLiteral("set_single_btn"), m_setSingleBtn);
    connect(m_setSingleBtn,
            &QPushButton::clicked,
            this,
            &FoupInAutoPurgeEnableWidget::onSetSingleBtnClicked);

    m_setAllBtn = new QPushButton(QStringLiteral("Set All"), m_controlItem);
    m_controlItem->addWidget(QStringLiteral("set_all_btn"), m_setAllBtn);
    connect(m_setAllBtn,
            &QPushButton::clicked,
            this,
            &FoupInAutoPurgeEnableWidget::onSetAllBtnClicked);

    addItem(m_controlItem);
}

void FoupInAutoPurgeEnableWidget::loadEnableFromConfig(const QString &qrCode)
{
    if (qrCode.isEmpty() || !m_enableComboBox) {
        return;
    }

    const OHBDeviceConfigInfo deviceConfig = OHBDeviceConfig::getInstance().getDeviceByQRCode(qrCode);
    if (deviceConfig.getQrCode().isEmpty()) {
        return;
    }

    setInitialConfigValue(deviceConfig.getFoupInAutoPurgeEnable());
}

void FoupInAutoPurgeEnableWidget::onSetSingleBtnClicked()
{
    submitWriteTask(QString::number(m_qrcodeSpinBox->value()), currentEnableValue());
}

void FoupInAutoPurgeEnableWidget::onSetAllBtnClicked()
{
    const QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this,
                             QStringLiteral("Set Failed"),
                             QStringLiteral("No target device available"));
        return;
    }

    submitWriteAllTask(qrcodes, currentEnableValue());
}

void FoupInAutoPurgeEnableWidget::submitWriteTask(const QString &qrcode, quint16 enableValue)
{
    auto *task = new SetFoupInAutoPurgeEnableTask(QVector<QString>{qrcode},
                                                  static_cast<int>(enableValue));

    m_controlItem->setStatusWaiting(QStringLiteral("Setting..."));
    setButtonsEnabled(false);

    connect(task,
            &SetFoupInAutoPurgeEnableTask::deviceRetrying,
            this,
            [this](const QString &qrCode, int retryCount, int maxRetry) {
                m_controlItem->setStatusWaiting(
                    QString("Retrying %1 (%2/%3)").arg(qrCode).arg(retryCount).arg(maxRetry));
            },
            Qt::QueuedConnection);

    connect(task,
            &SetFoupInAutoPurgeEnableTask::allFinished,
            this,
            [this, qrcode, enableValue](bool allSuccess,
                                        int successCount,
                                        const QStringList &failedIds,
                                        int finalEnableValue) {
                Q_UNUSED(successCount)
                Q_UNUSED(failedIds)
                Q_UNUSED(finalEnableValue)

                loadEnableFromConfig(qrcode);

                if (allSuccess) {
                    m_controlItem->setStatusOK(QStringLiteral("Set succeeded"));
                    QMessageBox::information(this,
                                             QStringLiteral("Set Succeeded"),
                                             QStringLiteral("Device %1: %2")
                                                 .arg(qrcode, valueDisplayText(enableValue)));
                } else {
                    m_controlItem->setStatusFailed(QStringLiteral("Set failed"));
                    QMessageBox::warning(this,
                                         QStringLiteral("Set Failed"),
                                         QStringLiteral("Device %1 failed to set Foup In Auto Purge Enable")
                                             .arg(qrcode));
                }

                setButtonsEnabled(true);
            });

    Scheduler::instance()->submitTask(task);

    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(),
                                      Level::INFO,
                                      QString("[ui][FoupInAutoPurgeEnableWidget] submit single task: qrcode=%1 enableValue=%2")
                                          .arg(qrcode)
                                          .arg(enableValue)
                                          .toStdString());
}

void FoupInAutoPurgeEnableWidget::submitWriteAllTask(const QStringList &qrcodes,
                                                     quint16 enableValue)
{
    const QVector<QString> qrcodeVector(qrcodes.begin(), qrcodes.end());
    auto *task = new SetFoupInAutoPurgeEnableTask(qrcodeVector,
                                                  static_cast<int>(enableValue));

    m_controlItem->setStatusWaiting(QStringLiteral("Setting all..."));
    setButtonsEnabled(false);

    QStringList *targetQrcodes = new QStringList(qrcodes);

    connect(task,
            &SetFoupInAutoPurgeEnableTask::deviceRetrying,
            this,
            [this](const QString &qrCode, int retryCount, int maxRetry) {
                m_controlItem->setStatusWaiting(
                    QString("Retrying %1 (%2/%3)").arg(qrCode).arg(retryCount).arg(maxRetry));
            },
            Qt::QueuedConnection);

    connect(task,
            &SetFoupInAutoPurgeEnableTask::allFinished,
            this,
            [this, targetQrcodes, enableValue](bool allSuccess,
                                               int successCount,
                                               const QStringList &failedIds,
                                               int finalEnableValue) {
                Q_UNUSED(successCount)
                Q_UNUSED(finalEnableValue)

                loadEnableFromConfig(m_qrcodeSpinBox ? QString::number(m_qrcodeSpinBox->value()) : QString());

                if (allSuccess) {
                    m_controlItem->setStatusOK(QStringLiteral("All succeeded"));
                } else {
                    m_controlItem->setStatusFailed(QStringLiteral("Completed with failures"));
                }

                QList<QStringList> tableRows;
                for (const QString &qrcode : *targetQrcodes) {
                    const bool failed = failedIds.contains(qrcode);
                    tableRows.append({
                        qrcode,
                        failed ? QStringLiteral("Failed") : QStringLiteral("Success"),
                        FoupInAutoPurgeEnableWidget::valueDisplayText(enableValue)
                    });
                }

                ModalTableDialog *dialog = ModalTableDialog::showAsync(
                    this,
                    QStringLiteral("Foup In Auto Purge Enable Result"),
                    QStringList{QStringLiteral("QRCode"),
                                QStringLiteral("Status"),
                                QStringLiteral("Value")},
                    tableRows);

                if (dialog) {
                    dialog->setFieldTextColor(QStringLiteral("Status"),
                                              QStringLiteral("Success"),
                                              QColor(0, 150, 0));
                    dialog->setFieldTextColor(QStringLiteral("Status"),
                                              QStringLiteral("Failed"),
                                              QColor(210, 0, 0));
                }

                setButtonsEnabled(true);
                delete targetQrcodes;
            });

    Scheduler::instance()->submitTask(task);

    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(),
                                      Level::INFO,
                                      QString("[ui][FoupInAutoPurgeEnableWidget] submit all task: deviceCount=%1 enableValue=%2")
                                          .arg(qrcodes.size())
                                          .arg(enableValue)
                                          .toStdString());
}

void FoupInAutoPurgeEnableWidget::setButtonsEnabled(bool enabled)
{
    if (m_setSingleBtn) {
        m_setSingleBtn->setEnabled(enabled);
    }
    if (m_setAllBtn) {
        m_setAllBtn->setEnabled(enabled);
    }
}

quint16 FoupInAutoPurgeEnableWidget::currentEnableValue() const
{
    return static_cast<quint16>(m_enableComboBox->currentData().toUInt());
}

QString FoupInAutoPurgeEnableWidget::valueDisplayText(quint16 value)
{
    const QString hexText = QStringLiteral("0x%1")
                                .arg(QString::number(value, 16).toUpper().rightJustified(4, QLatin1Char('0')));

    if (value == 0) {
        return QStringLiteral("0 - Disabled (%1)").arg(hexText);
    }
    if (value == 1) {
        return QStringLiteral("1 - Enabled (%1)").arg(hexText);
    }
    return QStringLiteral("Unknown (%1)").arg(hexText);
}
