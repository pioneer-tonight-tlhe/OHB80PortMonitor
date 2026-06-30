#include "firmwareupdateconfigsettingwidget.h"

#include "../settingwidget/settingitemwidget.h"
#include "applogger.h"
#include "firmwareconfig.h"
#include "loggermanager.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_firmware_config_task/set_firmware_config_task.h"

#include <QDebug>
#include <QFileDialog>

FirmwareUpdateConfigSettingWidget::FirmwareUpdateConfigSettingWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle("Firmware Config");
    initUI();
    loadConfigValues();

    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget] created";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(),
                                      Level::INFO,
                                      "[ui][FirmwareUpdateConfigSettingWidget] created");
}

FirmwareUpdateConfigSettingWidget::~FirmwareUpdateConfigSettingWidget()
{
    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget] destroyed";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(),
                                      Level::INFO,
                                      "[ui][FirmwareUpdateConfigSettingWidget] destroyed");
}

QString FirmwareUpdateConfigSettingWidget::binFilePath() const
{
    return m_binFileLineEdit ? m_binFileLineEdit->text() : QString();
}

void FirmwareUpdateConfigSettingWidget::initUI()
{
    initLoadBinFileItem();
    initPrepareTimeoutItem();
    initWaitingTimeItem();
    initSendIntervalItem();
    initTransferTimeoutItem();
    initPostTransferWaitTimeItem();
}

void FirmwareUpdateConfigSettingWidget::initLoadBinFileItem()
{
    SettingItemWidget *item = new SettingItemWidget(this);
    item->setTitle("Load Firmware Bin File");
    item->setTip("Select firmware bin file for upgrade");

    m_binFileLineEdit = new QLineEdit(item);
    m_binFileLineEdit->setPlaceholderText("No bin file selected");
    m_binFileLineEdit->setReadOnly(true);
    m_binFileLineEdit->setFixedWidth(400);
    item->addWidget("bin_file_edit", m_binFileLineEdit);

    QPushButton *browseButton = new QPushButton("Browse", item);
    item->addWidget("browse_btn", browseButton);
    connect(browseButton,
            &QPushButton::clicked,
            this,
            &FirmwareUpdateConfigSettingWidget::onLoadBinFileBtnClicked);

    addItem(item);
}

void FirmwareUpdateConfigSettingWidget::initPrepareTimeoutItem()
{
    m_prepareTimeoutItem = new SettingItemWidget(this);
    m_prepareTimeoutItem->setTitle("Prepare Command Timeout");
    m_prepareTimeoutItem->setTip("Set prepare command timeout for firmware upgrade");

    m_prepareTimeoutSpinBox = new QSpinBox(m_prepareTimeoutItem);
    m_prepareTimeoutSpinBox->setRange(1000, 30000);
    m_prepareTimeoutSpinBox->setSuffix(" ms");
    m_prepareTimeoutSpinBox->setMaximumWidth(150);
    m_prepareTimeoutItem->addWidget("prepare_timeout_spin", m_prepareTimeoutSpinBox);

    QPushButton *setButton = new QPushButton("Set", m_prepareTimeoutItem);
    m_prepareTimeoutItem->addWidget("prepare_timeout_set_btn", setButton);
    connect(setButton,
            &QPushButton::clicked,
            this,
            &FirmwareUpdateConfigSettingWidget::onPrepareTimeoutSetBtnClicked);

    addItem(m_prepareTimeoutItem);
}

void FirmwareUpdateConfigSettingWidget::initWaitingTimeItem()
{
    m_waitingTimeItem = new SettingItemWidget(this);
    m_waitingTimeItem->setTitle("Waiting for Equipment Ready");
    m_waitingTimeItem->setTip("Set waiting time for equipment to be ready before firmware update");

    m_waitingTimeSpinBox = new QSpinBox(m_waitingTimeItem);
    m_waitingTimeSpinBox->setRange(0, 60000);
    m_waitingTimeSpinBox->setSuffix(" ms");
    m_waitingTimeSpinBox->setMaximumWidth(150);
    m_waitingTimeItem->addWidget("waiting_time_spin", m_waitingTimeSpinBox);

    QPushButton *setButton = new QPushButton("Set", m_waitingTimeItem);
    m_waitingTimeItem->addWidget("waiting_time_set_btn", setButton);
    connect(setButton,
            &QPushButton::clicked,
            this,
            &FirmwareUpdateConfigSettingWidget::onWaitingTimeSetBtnClicked);

    addItem(m_waitingTimeItem);
}

void FirmwareUpdateConfigSettingWidget::initSendIntervalItem()
{
    m_sendIntervalItem = new SettingItemWidget(this);
    m_sendIntervalItem->setTitle("Send Interval for Firmware Data");
    m_sendIntervalItem->setTip("Set send interval for firmware data packets");

    m_sendIntervalSpinBox = new QSpinBox(m_sendIntervalItem);
    m_sendIntervalSpinBox->setRange(10, 1000);
    m_sendIntervalSpinBox->setSuffix(" ms");
    m_sendIntervalSpinBox->setMaximumWidth(150);
    m_sendIntervalItem->addWidget("send_interval_spin", m_sendIntervalSpinBox);

    QPushButton *setButton = new QPushButton("Set", m_sendIntervalItem);
    m_sendIntervalItem->addWidget("send_interval_set_btn", setButton);
    connect(setButton,
            &QPushButton::clicked,
            this,
            &FirmwareUpdateConfigSettingWidget::onSendIntervalSetBtnClicked);

    addItem(m_sendIntervalItem);
}

void FirmwareUpdateConfigSettingWidget::initTransferTimeoutItem()
{
    m_transferTimeoutItem = new SettingItemWidget(this);
    m_transferTimeoutItem->setTitle("Transfer Response Timeout");
    m_transferTimeoutItem->setTip("Set transfer response timeout for firmware upgrade");

    m_transferTimeoutSpinBox = new QSpinBox(m_transferTimeoutItem);
    m_transferTimeoutSpinBox->setRange(1000, 30000);
    m_transferTimeoutSpinBox->setSuffix(" ms");
    m_transferTimeoutSpinBox->setMaximumWidth(150);
    m_transferTimeoutItem->addWidget("transfer_timeout_spin", m_transferTimeoutSpinBox);

    QPushButton *setButton = new QPushButton("Set", m_transferTimeoutItem);
    m_transferTimeoutItem->addWidget("transfer_timeout_set_btn", setButton);
    connect(setButton,
            &QPushButton::clicked,
            this,
            &FirmwareUpdateConfigSettingWidget::onTransferTimeoutSetBtnClicked);

    addItem(m_transferTimeoutItem);
}

void FirmwareUpdateConfigSettingWidget::initPostTransferWaitTimeItem()
{
    m_postTransferWaitTimeItem = new SettingItemWidget(this);
    m_postTransferWaitTimeItem->setTitle("Post-Transfer Wait Time");
    m_postTransferWaitTimeItem->setTip("Set wait time for device reboot after firmware data transfer");

    m_postTransferWaitTimeSpinBox = new QSpinBox(m_postTransferWaitTimeItem);
    m_postTransferWaitTimeSpinBox->setRange(0, 60000);
    m_postTransferWaitTimeSpinBox->setSuffix(" ms");
    m_postTransferWaitTimeSpinBox->setMaximumWidth(150);
    m_postTransferWaitTimeItem->addWidget("post_transfer_wait_time_spin", m_postTransferWaitTimeSpinBox);

    QPushButton *setButton = new QPushButton("Set", m_postTransferWaitTimeItem);
    m_postTransferWaitTimeItem->addWidget("post_transfer_wait_time_set_btn", setButton);
    connect(setButton,
            &QPushButton::clicked,
            this,
            &FirmwareUpdateConfigSettingWidget::onPostTransferWaitTimeSetBtnClicked);

    addItem(m_postTransferWaitTimeItem);
}

void FirmwareUpdateConfigSettingWidget::loadConfigValues()
{
    FirmwareConfig &config = FirmwareConfig::getInstance();
    config.reloadConfig();

    if (m_prepareTimeoutSpinBox) {
        m_prepareTimeoutSpinBox->setValue(config.prepareCmdTimeoutMs());
    }
    if (m_waitingTimeSpinBox) {
        m_waitingTimeSpinBox->setValue(config.waitingForEquipmentReadyMs());
    }
    if (m_sendIntervalSpinBox) {
        m_sendIntervalSpinBox->setValue(config.sendIntervalForDataMs());
    }
    if (m_transferTimeoutSpinBox) {
        m_transferTimeoutSpinBox->setValue(config.transferResponseTimeoutMs());
    }
    if (m_postTransferWaitTimeSpinBox) {
        m_postTransferWaitTimeSpinBox->setValue(config.postTransferWaitMs());
    }
}

void FirmwareUpdateConfigSettingWidget::onLoadBinFileBtnClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select Firmware Bin File",
        "",
        "Bin Files (*.bin);;All Files (*)");

    if (filePath.isEmpty()) {
        return;
    }

    m_binFileLineEdit->setText(filePath);
    emit binFilePathChanged(filePath);

    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget] selected bin file:" << filePath;
}

void FirmwareUpdateConfigSettingWidget::submitConfigTask(
    SettingItemWidget *item,
    const std::function<void(SetFirmwareConfigTask *)> &configSetter,
    const QString &paramName,
    int value)
{
    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget] submit" << paramName << value;

    SetFirmwareConfigTask *task = new SetFirmwareConfigTask();
    configSetter(task);

    if (item) {
        item->setStatusWaiting();
    }

    connect(task,
            &SetFirmwareConfigTask::finished,
            this,
            [this, item, paramName, value](bool success, const QString &message) {
                loadConfigValues();

                if (item) {
                    if (success) {
                        item->setStatusOK();
                    } else {
                        item->setStatusFailed();
                    }
                }

                const QString logMessage = success
                    ? QString("[ui][FirmwareUpdateConfigSettingWidget] %1=%2 applied, %3")
                          .arg(paramName)
                          .arg(value)
                          .arg(message)
                    : QString("[ui][FirmwareUpdateConfigSettingWidget] %1=%2 failed, %3")
                          .arg(paramName)
                          .arg(value)
                          .arg(message);
                qDebug() << logMessage;
                LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(),
                                                  success ? Level::INFO : Level::WARN,
                                                  logMessage.toStdString());
            });

    Scheduler::instance()->submitTask(task);
}

void FirmwareUpdateConfigSettingWidget::onPrepareTimeoutSetBtnClicked()
{
    submitConfigTask(m_prepareTimeoutItem,
                     [this](SetFirmwareConfigTask *task) {
                         task->setPrepareTimeout(m_prepareTimeoutSpinBox->value());
                     },
                     "PrepareCmdTimeoutTimeMs",
                     m_prepareTimeoutSpinBox->value());
}

void FirmwareUpdateConfigSettingWidget::onWaitingTimeSetBtnClicked()
{
    submitConfigTask(m_waitingTimeItem,
                     [this](SetFirmwareConfigTask *task) {
                         task->setWaitingTime(m_waitingTimeSpinBox->value());
                     },
                     "WaitingForEquipmentReadyTimeMs",
                     m_waitingTimeSpinBox->value());
}

void FirmwareUpdateConfigSettingWidget::onSendIntervalSetBtnClicked()
{
    submitConfigTask(m_sendIntervalItem,
                     [this](SetFirmwareConfigTask *task) {
                         task->setSendInterval(m_sendIntervalSpinBox->value());
                     },
                     "SendIntervalForDataTimeMs",
                     m_sendIntervalSpinBox->value());
}

void FirmwareUpdateConfigSettingWidget::onTransferTimeoutSetBtnClicked()
{
    submitConfigTask(m_transferTimeoutItem,
                     [this](SetFirmwareConfigTask *task) {
                         task->setTransferTimeout(m_transferTimeoutSpinBox->value());
                     },
                     "TransferResponseTimeoutTimeMs",
                     m_transferTimeoutSpinBox->value());
}

void FirmwareUpdateConfigSettingWidget::onPostTransferWaitTimeSetBtnClicked()
{
    submitConfigTask(m_postTransferWaitTimeItem,
                     [this](SetFirmwareConfigTask *task) {
                         task->setPostTransferWaitTime(m_postTransferWaitTimeSpinBox->value());
                     },
                     "PostTransferWaitTimeMs",
                     m_postTransferWaitTimeSpinBox->value());
}
