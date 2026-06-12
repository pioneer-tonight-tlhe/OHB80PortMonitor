#include "firmwareupdateconfigsettingwidget.h"
#include "../settingwidget/settingitemwidget.h"
#include "loggermanager.h"
#include "applogger.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_firmware_config_task/set_firmware_config_task.h"
#include "firmwareconfig.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

FirmwareUpdateConfigSettingWidget::FirmwareUpdateConfigSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_binFileLineEdit(nullptr)
    , m_prepareTimeoutSpinBox(nullptr)
    , m_waitingTimeSpinBox(nullptr)
    , m_sendIntervalSpinBox(nullptr)
    , m_transferTimeoutSpinBox(nullptr)
    , m_postTransferWaitTimeSpinBox(nullptr)
    , m_prepareTimeoutItem(nullptr)
    , m_waitingTimeItem(nullptr)
    , m_sendIntervalItem(nullptr)
    , m_transferTimeoutItem(nullptr)
    , m_postTransferWaitTimeItem(nullptr)
{
    setTitle("Firmware Config");
    initUI();

    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget][FirmwareUpdateConfigSettingWidget]：固件更新配置控件已创建";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[ui][FirmwareUpdateConfigSettingWidget][FirmwareUpdateConfigSettingWidget]：固件更新配置控件已创建");
}

FirmwareUpdateConfigSettingWidget::~FirmwareUpdateConfigSettingWidget()
{
    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget][~FirmwareUpdateConfigSettingWidget]：固件更新配置控件已销毁";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[ui][FirmwareUpdateConfigSettingWidget][~FirmwareUpdateConfigSettingWidget]：固件更新配置控件已销毁");
}

QString FirmwareUpdateConfigSettingWidget::binFilePath() const
{
    return m_binFileLineEdit ? m_binFileLineEdit->text() : QString();
}

void FirmwareUpdateConfigSettingWidget::initUI()
{
    // bin 文件加载项
    initLoadBinFileItem();
    // 准备指令超时项
    initPrepareTimeoutItem();
    // 等待设备就绪项
    initWaitingTimeItem();
    // 发送间隔项
    initSendIntervalItem();
    // 传输响应超时项
    initTransferTimeoutItem();
    // 数据传输后等待时间项
    initPostTransferWaitTimeItem();

    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget][initUI]：固件更新配置UI初始化完成";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[ui][FirmwareUpdateConfigSettingWidget][initUI]：固件更新配置UI初始化完成");
}

void FirmwareUpdateConfigSettingWidget::initLoadBinFileItem()
{
    auto item = new SettingItemWidget(this);
    item->setTitle("Load Firmware Bin File");
    item->setTip("Select firmware bin file for upgrade");
    
    m_binFileLineEdit = new QLineEdit(item);
    m_binFileLineEdit->setPlaceholderText("No bin file selected");
    m_binFileLineEdit->setReadOnly(true);
    item->addWidget("bin_file_edit", m_binFileLineEdit);
    m_binFileLineEdit->setFixedWidth(400);
    
    auto browseBtn = new QPushButton("Browse", item);
    item->addWidget("browse_btn", browseBtn);
    connect(browseBtn, &QPushButton::clicked, this, &FirmwareUpdateConfigSettingWidget::onLoadBinFileBtnClicked);
    
    addItem(item);
}

void FirmwareUpdateConfigSettingWidget::initPrepareTimeoutItem()
{
    m_prepareTimeoutItem = new SettingItemWidget(this);
    m_prepareTimeoutItem->setTitle("Prepare Command Timeout");
    m_prepareTimeoutItem->setTip("Set prepare command timeout for firmware upgrade");
    
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    m_prepareTimeoutSpinBox = new QSpinBox(m_prepareTimeoutItem);
    m_prepareTimeoutSpinBox->setRange(1000, 30000);
    m_prepareTimeoutSpinBox->setValue(fwConfig.prepareCmdTimeoutMs());
    m_prepareTimeoutSpinBox->setSuffix(" ms");
    m_prepareTimeoutSpinBox->setMaximumWidth(150);
    m_prepareTimeoutItem->addWidget("prepare_timeout_spin", m_prepareTimeoutSpinBox);
    
    auto setBtn = new QPushButton("Set", m_prepareTimeoutItem);
    m_prepareTimeoutItem->addWidget("prepare_timeout_set_btn", setBtn);
    connect(setBtn, &QPushButton::clicked, this, &FirmwareUpdateConfigSettingWidget::onPrepareTimeoutSetBtnClicked);
    
    addItem(m_prepareTimeoutItem);
}

void FirmwareUpdateConfigSettingWidget::initWaitingTimeItem()
{
    m_waitingTimeItem = new SettingItemWidget(this);
    m_waitingTimeItem->setTitle("Waiting for Equipment Ready");
    m_waitingTimeItem->setTip("Set waiting time for equipment to be ready before firmware update");
    
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    m_waitingTimeSpinBox = new QSpinBox(m_waitingTimeItem);
    m_waitingTimeSpinBox->setRange(0, 60000);
    m_waitingTimeSpinBox->setValue(fwConfig.waitingForEquipmentReadyMs());
    m_waitingTimeSpinBox->setSuffix(" ms");
    m_waitingTimeSpinBox->setMaximumWidth(150);
    m_waitingTimeItem->addWidget("waiting_time_spin", m_waitingTimeSpinBox);
    
    auto setBtn = new QPushButton("Set", m_waitingTimeItem);
    m_waitingTimeItem->addWidget("waiting_time_set_btn", setBtn);
    connect(setBtn, &QPushButton::clicked, this, &FirmwareUpdateConfigSettingWidget::onWaitingTimeSetBtnClicked);
    
    addItem(m_waitingTimeItem);
}

void FirmwareUpdateConfigSettingWidget::initSendIntervalItem()
{
    m_sendIntervalItem = new SettingItemWidget(this);
    m_sendIntervalItem->setTitle("Send Interval for Firmware Data");
    m_sendIntervalItem->setTip("Set send interval for firmware data packets");
    
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    m_sendIntervalSpinBox = new QSpinBox(m_sendIntervalItem);
    m_sendIntervalSpinBox->setRange(10, 1000);
    m_sendIntervalSpinBox->setValue(fwConfig.sendIntervalForDataMs());
    m_sendIntervalSpinBox->setSuffix(" ms");
    m_sendIntervalSpinBox->setMaximumWidth(150);
    m_sendIntervalItem->addWidget("send_interval_spin", m_sendIntervalSpinBox);
    
    auto setBtn = new QPushButton("Set", m_sendIntervalItem);
    m_sendIntervalItem->addWidget("send_interval_set_btn", setBtn);
    connect(setBtn, &QPushButton::clicked, this, &FirmwareUpdateConfigSettingWidget::onSendIntervalSetBtnClicked);
    
    addItem(m_sendIntervalItem);
}

void FirmwareUpdateConfigSettingWidget::initTransferTimeoutItem()
{
    m_transferTimeoutItem = new SettingItemWidget(this);
    m_transferTimeoutItem->setTitle("Transfer Response Timeout");
    m_transferTimeoutItem->setTip("Set transfer response timeout for firmware upgrade");

    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    m_transferTimeoutSpinBox = new QSpinBox(m_transferTimeoutItem);
    m_transferTimeoutSpinBox->setRange(1000, 30000);
    m_transferTimeoutSpinBox->setValue(fwConfig.transferResponseTimeoutMs());
    m_transferTimeoutSpinBox->setSuffix(" ms");
    m_transferTimeoutSpinBox->setMaximumWidth(150);
    m_transferTimeoutItem->addWidget("transfer_timeout_spin", m_transferTimeoutSpinBox);

    auto setBtn = new QPushButton("Set", m_transferTimeoutItem);
    m_transferTimeoutItem->addWidget("transfer_timeout_set_btn", setBtn);
    connect(setBtn, &QPushButton::clicked, this, &FirmwareUpdateConfigSettingWidget::onTransferTimeoutSetBtnClicked);

    addItem(m_transferTimeoutItem);
}

void FirmwareUpdateConfigSettingWidget::initPostTransferWaitTimeItem()
{
    m_postTransferWaitTimeItem = new SettingItemWidget(this);
    m_postTransferWaitTimeItem->setTitle("Post-Transfer Wait Time");
    m_postTransferWaitTimeItem->setTip("Set wait time for device reboot after firmware data transfer");

    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    m_postTransferWaitTimeSpinBox = new QSpinBox(m_postTransferWaitTimeItem);
    m_postTransferWaitTimeSpinBox->setRange(0, 60000);
    m_postTransferWaitTimeSpinBox->setValue(fwConfig.postTransferWaitMs());
    m_postTransferWaitTimeSpinBox->setSuffix(" ms");
    m_postTransferWaitTimeSpinBox->setMaximumWidth(150);
    m_postTransferWaitTimeItem->addWidget("post_transfer_wait_time_spin", m_postTransferWaitTimeSpinBox);

    auto setBtn = new QPushButton("Set", m_postTransferWaitTimeItem);
    m_postTransferWaitTimeItem->addWidget("post_transfer_wait_time_set_btn", setBtn);
    connect(setBtn, &QPushButton::clicked, this, &FirmwareUpdateConfigSettingWidget::onPostTransferWaitTimeSetBtnClicked);

    addItem(m_postTransferWaitTimeItem);
}

void FirmwareUpdateConfigSettingWidget::onLoadBinFileBtnClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Select Firmware Bin File", "", "Bin Files (*.bin);;All Files (*)");
    
    if (!filePath.isEmpty()) {
        m_binFileLineEdit->setText(filePath);
        
        // 通知外部（DebugPage）同步到 FirmwareUpdateSettingWidget
        emit binFilePathChanged(filePath);
        
        qDebug() << "[ui][FirmwareUpdateConfigSettingWidget][onLoadBinFileBtnClicked]：bin 文件已选择:" << filePath;
        LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[ui][FirmwareUpdateConfigSettingWidget][onLoadBinFileBtnClicked]：bin 文件已选择: %1").arg(filePath).toStdString());
    }
}

void FirmwareUpdateConfigSettingWidget::submitConfigTask(SettingItemWidget *item,
                                                         std::function<void(SetFirmwareConfigTask*)> configSetter, 
                                                         const QString &paramName, 
                                                         int value,
                                                         std::function<bool(int)> saveConfigCallback)
{
    qDebug() << "[ui][FirmwareUpdateConfigSettingWidget][submitConfigTask]：" << paramName << "设置为:" << value << "ms";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[ui][FirmwareUpdateConfigSettingWidget][submitConfigTask]：%1设置为: %2ms").arg(paramName).arg(value).toStdString());
    
    SetFirmwareConfigTask *task = new SetFirmwareConfigTask();
    configSetter(task);

    if (item) item->setStatusWaiting();

    connect(task, &SetFirmwareConfigTask::finished, this, [this, item, paramName, value, saveConfigCallback](bool success, const QString &msg) {
        QString logMsg = success 
            ? QString("[ui][FirmwareUpdateConfigSettingWidget][submitConfigTask]：%1设置成功: %2ms, %3").arg(paramName).arg(value).arg(msg)
            : QString("[ui][FirmwareUpdateConfigSettingWidget][submitConfigTask]：%1设置失败: %2").arg(paramName).arg(msg);
        qDebug() << logMsg;
        LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), success ? Level::INFO : Level::WARN, logMsg.toStdString());
        
        // 任务成功时保存到配置文件
        if (success && saveConfigCallback) {
            bool saved = saveConfigCallback(value);
            QString saveLogMsg = saved 
                ? QString("[ui][FirmwareUpdateConfigSettingWidget][submitConfigTask]：%1已保存到配置文件").arg(paramName)
                : QString("[ui][FirmwareUpdateConfigSettingWidget][submitConfigTask]：%1保存到配置文件失败").arg(paramName);
            qDebug() << saveLogMsg;
            LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), saved ? Level::INFO : Level::WARN, saveLogMsg.toStdString());
            
            // 显示状态
            if (item) {
                if (saved) item->setStatusOK();
                else       item->setStatusFailed();
            }
        } else {
            // 显示任务执行状态
            if (item) {
                if (success) item->setStatusOK();
                else         item->setStatusFailed();
            }
        }
    });
    
    Scheduler::instance()->submitTask(task);
}

void FirmwareUpdateConfigSettingWidget::onPrepareTimeoutSetBtnClicked()
{
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    submitConfigTask(m_prepareTimeoutItem,
                     [this](SetFirmwareConfigTask *task) { task->setPrepareTimeout(m_prepareTimeoutSpinBox->value()); }, 
                     "准备指令超时", 
                     m_prepareTimeoutSpinBox->value(),
                     [&fwConfig](int value) { return fwConfig.setPrepareCmdTimeoutMs(value); });
}

void FirmwareUpdateConfigSettingWidget::onWaitingTimeSetBtnClicked()
{
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    submitConfigTask(m_waitingTimeItem,
                     [this](SetFirmwareConfigTask *task) { task->setWaitingTime(m_waitingTimeSpinBox->value()); }, 
                     "等待设备就绪时间", 
                     m_waitingTimeSpinBox->value(),
                     [&fwConfig](int value) { return fwConfig.setWaitingForEquipmentReadyMs(value); });
}

void FirmwareUpdateConfigSettingWidget::onSendIntervalSetBtnClicked()
{
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    submitConfigTask(m_sendIntervalItem,
                     [this](SetFirmwareConfigTask *task) { task->setSendInterval(m_sendIntervalSpinBox->value()); }, 
                     "固件数据发送间隔", 
                     m_sendIntervalSpinBox->value(),
                     [&fwConfig](int value) { return fwConfig.setSendIntervalForDataMs(value); });
}

void FirmwareUpdateConfigSettingWidget::onTransferTimeoutSetBtnClicked()
{
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    submitConfigTask(m_transferTimeoutItem,
                     [this](SetFirmwareConfigTask *task) { task->setTransferTimeout(m_transferTimeoutSpinBox->value()); },
                     "传输响应超时",
                     m_transferTimeoutSpinBox->value(),
                     [&fwConfig](int value) { return fwConfig.setTransferResponseTimeoutMs(value); });
}

void FirmwareUpdateConfigSettingWidget::onPostTransferWaitTimeSetBtnClicked()
{
    FirmwareConfig &fwConfig = FirmwareConfig::getInstance();
    submitConfigTask(m_postTransferWaitTimeItem,
                     [this](SetFirmwareConfigTask *task) { task->setPostTransferWaitTime(m_postTransferWaitTimeSpinBox->value()); },
                     "数据传输后等待时间",
                     m_postTransferWaitTimeSpinBox->value(),
                     [&fwConfig](int value) { return fwConfig.setPostTransferWaitMs(value); });
}
