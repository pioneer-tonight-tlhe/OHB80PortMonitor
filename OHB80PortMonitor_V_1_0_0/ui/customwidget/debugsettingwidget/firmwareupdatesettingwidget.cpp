#include "firmwareupdatesettingwidget.h"
#include "firmwareupdatewidget.h"
#include "loggermanager.h"
#include "app/applogger.h"

#include <QDebug>

FirmwareUpdateSettingWidget::FirmwareUpdateSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_firmwareUpdateWidget(nullptr)
{
    setTitle("Firmware Update");

    m_firmwareUpdateWidget = new FirmwareUpdateWidget(this);
    addCustomWidget(m_firmwareUpdateWidget, VerticalLayout);

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

void FirmwareUpdateSettingWidget::setFirmwareFilePath(const QString &filePath)
{
    if (m_firmwareUpdateWidget) {
        m_firmwareUpdateWidget->setFirmwareFilePath(filePath);
    }
}
