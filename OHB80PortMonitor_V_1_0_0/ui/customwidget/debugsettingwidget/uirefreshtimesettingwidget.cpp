#include "uirefreshtimesettingwidget.h"

#include "../modaltabledialog/modaltabledialog.h"
#include "../settingwidget/settingitemwidget.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "loggermanager.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "ohbdeviceconfig.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_ui_refresh_time_task/set_ui_refresh_time_task.h"

#include <QColor>
#include <QMessageBox>
#include <QVector>

UIRefreshTimeSettingWidget::UIRefreshTimeSettingWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("UI Refresh Time Configuration");
    initUI();

    if (m_qrcodeSpinBox) {
        loadConfigValues(QString::number(m_qrcodeSpinBox->value()));
    }
}

UIRefreshTimeSettingWidget::~UIRefreshTimeSettingWidget() = default;

void UIRefreshTimeSettingWidget::setInitialConfigValues(int logoTimeSeconds,
                                                        int pageTotalTimeSeconds,
                                                        int pageSwitchIntervalSeconds)
{
    if (m_logoSecSpinBox) {
        m_logoSecSpinBox->setValue(logoTimeSeconds);
    }
    if (m_paramTotalSpinBox) {
        m_paramTotalSpinBox->setValue(pageTotalTimeSeconds);
    }
    if (m_paramSwitchSpinBox) {
        m_paramSwitchSpinBox->setValue(pageSwitchIntervalSeconds);
    }
}

void UIRefreshTimeSettingWidget::initUI()
{
    initQrcodeItem();
    initLogoItem();
    initParamTotalItem();
    initParamSwitchItem();
}

void UIRefreshTimeSettingWidget::initQrcodeItem()
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
        const int qrcodeValue = qrcodes.first().toInt(&ok);
        if (ok) {
            m_qrcodeSpinBox->setValue(qrcodeValue);
        }
    }

    m_qrcodeItem->addWidget("qrcode_spin", m_qrcodeSpinBox);
    connect(m_qrcodeSpinBox,
            QOverload<int>::of(&QSpinBox::valueChanged),
            this,
            [this](int value) { loadConfigValues(QString::number(value)); });
    addItem(m_qrcodeItem);
}

void UIRefreshTimeSettingWidget::initLogoItem()
{
    m_logoItem = new SettingItemWidget(this);
    m_logoItem->setTitle("Logo Screen Duration");
    m_logoItem->setTip("Logo screen display duration (seconds)");

    m_logoSecSpinBox = new QSpinBox(m_logoItem);
    m_logoSecSpinBox->setRange(0, 65535);
    m_logoSecSpinBox->setValue(5);
    m_logoSecSpinBox->setSuffix(" s");
    m_logoSecSpinBox->setFixedWidth(120);
    m_logoItem->addWidget("logo_sec_spin", m_logoSecSpinBox);

    addItem(m_logoItem);
}

void UIRefreshTimeSettingWidget::initParamTotalItem()
{
    m_paramTotalItem = new SettingItemWidget(this);
    m_paramTotalItem->setTitle("Param Screen Total Duration");
    m_paramTotalItem->setTip("Parameter screen total display duration (seconds)");

    m_paramTotalSpinBox = new QSpinBox(m_paramTotalItem);
    m_paramTotalSpinBox->setRange(0, 65535);
    m_paramTotalSpinBox->setValue(5);
    m_paramTotalSpinBox->setSuffix(" s");
    m_paramTotalSpinBox->setFixedWidth(120);
    m_paramTotalItem->addWidget("param_total_spin", m_paramTotalSpinBox);

    addItem(m_paramTotalItem);
}

void UIRefreshTimeSettingWidget::initParamSwitchItem()
{
    m_paramSwitchItem = new SettingItemWidget(this);
    m_paramSwitchItem->setTitle("Param Page Switch Interval");
    m_paramSwitchItem->setTip("Parameter page switch interval (seconds)");

    m_paramSwitchSpinBox = new QSpinBox(m_paramSwitchItem);
    m_paramSwitchSpinBox->setRange(0, 65535);
    m_paramSwitchSpinBox->setValue(5);
    m_paramSwitchSpinBox->setSuffix(" s");
    m_paramSwitchSpinBox->setFixedWidth(120);
    m_paramSwitchItem->addWidget("param_switch_spin", m_paramSwitchSpinBox);

    QPushButton *setButton = new QPushButton("Set", m_paramSwitchItem);
    m_paramSwitchItem->addWidget("ui_refresh_set_btn", setButton);
    connect(setButton,
            &QPushButton::clicked,
            this,
            &UIRefreshTimeSettingWidget::onSetBtnClicked);

    QPushButton *setAllButton = new QPushButton("Set All", m_paramSwitchItem);
    m_paramSwitchItem->addWidget("ui_refresh_set_all_btn", setAllButton);
    connect(setAllButton,
            &QPushButton::clicked,
            this,
            &UIRefreshTimeSettingWidget::onSetAllBtnClicked);

    addItem(m_paramSwitchItem);
}

void UIRefreshTimeSettingWidget::loadConfigValues(const QString &qrCode)
{
    if (qrCode.isEmpty()) {
        return;
    }

    const OHBDeviceConfigInfo deviceConfig = OHBDeviceConfig::getInstance().getDeviceByQRCode(qrCode);
    if (deviceConfig.getQrCode().isEmpty()) {
        return;
    }

    if (m_logoSecSpinBox) {
        m_logoSecSpinBox->setValue(deviceConfig.getLogoTimeSeconds());
    }
    if (m_paramTotalSpinBox) {
        m_paramTotalSpinBox->setValue(deviceConfig.getPageTotalTimeSeconds());
    }
    if (m_paramSwitchSpinBox) {
        m_paramSwitchSpinBox->setValue(deviceConfig.getPageSwitchIntervalSeconds());
    }
}

void UIRefreshTimeSettingWidget::onSetBtnClicked()
{
    const QString qrcode = QString::number(m_qrcodeSpinBox->value());
    submitTask(QStringList{qrcode},
               m_logoSecSpinBox->value(),
               m_paramTotalSpinBox->value(),
               m_paramSwitchSpinBox->value());
}

void UIRefreshTimeSettingWidget::onSetAllBtnClicked()
{
    const QStringList qrcodes = ModbusTcpMasterManager::instance().masterIds();
    if (qrcodes.isEmpty()) {
        QMessageBox::warning(this, "Set Failed", "No target device available");
        return;
    }

    submitTask(qrcodes,
               m_logoSecSpinBox->value(),
               m_paramTotalSpinBox->value(),
               m_paramSwitchSpinBox->value());
}

void UIRefreshTimeSettingWidget::submitTask(const QStringList &qrcodes,
                                            int logoSec,
                                            int paramTotalSec,
                                            int paramSwitchSec)
{
    const QVector<QString> qrcodeVector(qrcodes.begin(), qrcodes.end());
    SetUIRefreshTimeTask *task = new SetUIRefreshTimeTask(qrcodeVector,
                                                          logoSec,
                                                          paramTotalSec,
                                                          paramSwitchSec);

    m_logoItem->setStatusWaiting();
    m_paramTotalItem->setStatusWaiting();
    m_paramSwitchItem->setStatusWaiting();

    QStringList *targetQrcodes = new QStringList(qrcodes);
    const bool isSetAll = qrcodes.size() > 1;

    connect(task,
            &SetUIRefreshTimeTask::deviceRetrying,
            this,
            [this](const QString &qrCode, int retryCount, int maxRetry) {
                const QString status = QString("Retrying %1 (%2/%3)")
                                           .arg(qrCode)
                                           .arg(retryCount)
                                           .arg(maxRetry);
                m_logoItem->setStatusWaiting(status);
                m_paramTotalItem->setStatusWaiting(status);
                m_paramSwitchItem->setStatusWaiting(status);
            },
            Qt::QueuedConnection);

    connect(task,
            &SetUIRefreshTimeTask::allFinished,
            this,
            [this, targetQrcodes, isSetAll](bool allSuccess,
                                            int successCount,
                                            QStringList failedQrCodes,
                                            int logoFinal,
                                            int totalFinal,
                                            int switchFinal) {
                loadConfigValues(m_qrcodeSpinBox ? QString::number(m_qrcodeSpinBox->value()) : QString());

                if (allSuccess) {
                    m_logoItem->setStatusOK();
                    m_paramTotalItem->setStatusOK();
                    m_paramSwitchItem->setStatusOK();
                } else {
                    m_logoItem->setStatusFailed();
                    m_paramTotalItem->setStatusFailed();
                    m_paramSwitchItem->setStatusFailed();
                }

                if (isSetAll) {
                    QList<QStringList> tableRows;
                    for (const QString &qrcode : *targetQrcodes) {
                        const bool success = !failedQrCodes.contains(qrcode);
                        tableRows.append({
                            qrcode,
                            success ? QStringLiteral("Success") : QStringLiteral("Failed"),
                            QString("%1 s").arg(logoFinal),
                            QString("%1 s").arg(totalFinal),
                            QString("%1 s").arg(switchFinal)
                        });
                    }

                    ModalTableDialog *dialog = ModalTableDialog::showAsync(
                        this,
                        QString("UI Refresh Time Set Result"),
                        QStringList{"QRCode", "Status", "Logo Duration", "Param Total", "Param Switch"},
                        tableRows);
                    if (dialog) {
                        dialog->setFieldTextColor("Status", "Success", QColor(0, 150, 0));
                        dialog->setFieldTextColor("Status", "Failed", QColor(210, 0, 0));
                    }
                } else if (allSuccess) {
                    QMessageBox::information(
                        this,
                        "Set Succeeded",
                        QString("Successfully set UI Refresh Time (logo=%1s, total=%2s, switch=%3s) on %4 device(s)")
                            .arg(logoFinal)
                            .arg(totalFinal)
                            .arg(switchFinal)
                            .arg(successCount));
                } else {
                    QMessageBox::warning(
                        this,
                        "Set Failed",
                        QString("Failed to set UI Refresh Time (logo=%1s, total=%2s, switch=%3s)")
                            .arg(logoFinal)
                            .arg(totalFinal)
                            .arg(switchFinal));
                }

                delete targetQrcodes;
            });

    Scheduler::instance()->submitTask(task);

    LoggerManager::getInstance()->log(
        AppLogger::SystemLoggerPath().toStdString(),
        Level::INFO,
        QString("[ui][UIRefreshTimeSettingWidget] submit task: devices=%1 logo=%2 total=%3 switch=%4")
            .arg(qrcodes.size())
            .arg(logoSec)
            .arg(paramTotalSec)
            .arg(paramSwitchSec)
            .toStdString());
}
