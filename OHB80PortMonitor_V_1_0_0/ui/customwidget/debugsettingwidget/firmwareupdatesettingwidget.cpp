#include "firmwareupdatesettingwidget.h"
#include "firmwareupdatewidget.h"
#include "firmwareupgradetestsettingwidget.h"
#include "loggermanager.h"
#include "app/applogger.h"

#include <QDebug>

FirmwareUpdateSettingWidget::FirmwareUpdateSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_firmwareUpdateWidget(nullptr)
    , m_firmwareUpgradeTestWidget(nullptr)
{
    setTitle("Firmware Update");

    m_firmwareUpdateWidget = new FirmwareUpdateWidget(this);
    addCustomWidget(m_firmwareUpdateWidget, VerticalLayout);

    m_firmwareUpgradeTestWidget = new FirmwareUpgradeTestSettingWidget(m_firmwareUpdateWidget, this);
    addCustomWidget(m_firmwareUpgradeTestWidget, VerticalLayout);

    qDebug() << "[ui][FirmwareUpdateSettingWidget][FirmwareUpdateSettingWidget]：固件升级界面已创建";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[ui][FirmwareUpdateSettingWidget][FirmwareUpdateSettingWidget]：固件升级界面已创建");
}

FirmwareUpdateSettingWidget::~FirmwareUpdateSettingWidget()
{
    qDebug() << "[ui][FirmwareUpdateSettingWidget][~FirmwareUpdateSettingWidget]：固件升级界面已销毁";
    LoggerManager::getInstance()->log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[ui][FirmwareUpdateSettingWidget][~FirmwareUpdateSettingWidget]：固件升级界面已销毁");
}

FirmwareUpdateWidget *FirmwareUpdateSettingWidget::firmwareUpdateWidget() const
{
    return m_firmwareUpdateWidget;
}

FirmwareUpgradeTestSettingWidget *FirmwareUpdateSettingWidget::firmwareUpgradeTestWidget() const
{
    return m_firmwareUpgradeTestWidget;
}

void FirmwareUpdateSettingWidget::setFirmwareFilePath(const QString &filePath)
{
    if (m_firmwareUpdateWidget) {
        m_firmwareUpdateWidget->setFirmwareFilePath(filePath);
    }
    if (m_firmwareUpgradeTestWidget) {
        m_firmwareUpgradeTestWidget->setFirmwareFilePath(filePath);
    }
}
