#include "configpage.h"

#include "app.h"
#include "ui_configpage.h"
#include "customwidget/configsettingwidget/deviceenablesettingwidget.h"
#include "customwidget/configsettingwidget/deviceinfosettingwidget.h"
#include "customwidget/configsettingwidget/idlepurgesettingwidget.h"
#include "customwidget/configsettingwidget/pneumaticvalvepressuresettingwidget.h"
#include "customwidget/configsettingwidget/purgeflowsettingwidget.h"
#include "customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.h"
#include "customwidget/configsettingwidget/sh85selfchecksettingwidget.h"
#include "ohbdeviceconfig.h"
#include "idlepurgeconfig.h"

#include <QScroller>
#include <QScrollerProperties>
#include <QScrollBar>

ConfigPage::ConfigPage(QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::ConfigPage)
    , m_idlePurgeWidget(nullptr)
    , m_pneumaticValvePressureWidget(nullptr)
    , m_sh85PeriodicSelfCheckWidget(nullptr)
    , m_sh85SelfCheckWidget(nullptr)
    , m_deviceEnableWidget(nullptr)
    , m_purgeFlowWidget(nullptr)
    , m_deviceInfoWidget(nullptr)
{
    m_ui->setupUi(this);
    initUI();
}

ConfigPage::~ConfigPage()
{
    delete m_ui;
}

void ConfigPage::initUI()
{
    initNav();

    if (m_ui->scrollArea && m_ui->scrollArea->viewport()) {
        QScroller::grabGesture(m_ui->scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
        QScroller *scroller = QScroller::scroller(m_ui->scrollArea->viewport());
        QScrollerProperties props = scroller->scrollerProperties();
        props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.005);
        props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.3);
        props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
        scroller->setScrollerProperties(props);
    }

    m_idlePurgeWidget = new IdlePurgeSettingWidget(this);
    IdlePurgeConfig &idlePurgeConfig = IdlePurgeConfig::getInstance();
    m_idlePurgeWidget->setConfigValues(idlePurgeConfig.isEnabled(),
                                       idlePurgeConfig.getPurgeDurationSeconds(),
                                       idlePurgeConfig.getPurgeIntervalSeconds());
    m_ui->scrollAreaWidgetContents->layout()->addWidget(m_idlePurgeWidget);

    m_pneumaticValvePressureWidget = new PneumaticValvePressureSettingWidget(this);
    m_ui->scrollAreaWidgetContents->layout()->addWidget(m_pneumaticValvePressureWidget);

    m_sh85PeriodicSelfCheckWidget = new SH85PeriodicSelfCheckSettingWidget(this);
    m_ui->scrollAreaWidgetContents->layout()->addWidget(m_sh85PeriodicSelfCheckWidget);
    connect(m_sh85PeriodicSelfCheckWidget, &SH85PeriodicSelfCheckSettingWidget::runningStateChanged,
            this, &ConfigPage::onPeriodicSelfCheckRunningStateChanged);

    m_sh85SelfCheckWidget = new SH85SelfCheckSettingWidget(this);
    m_ui->scrollAreaWidgetContents->layout()->addWidget(m_sh85SelfCheckWidget);
    connect(m_sh85SelfCheckWidget, &SH85SelfCheckSettingWidget::runningStateChanged,
            this, &ConfigPage::onSelfCheckRunningStateChanged);
    onPeriodicSelfCheckRunningStateChanged(m_sh85PeriodicSelfCheckWidget->isRunning());
    onSelfCheckRunningStateChanged(m_sh85SelfCheckWidget->isRunning());

    m_purgeFlowWidget = new PurgeFlowSettingWidget(this);
    m_ui->scrollAreaWidgetContents->layout()->addWidget(m_purgeFlowWidget);

    m_deviceEnableWidget = new DeviceEnableSettingWidget(this);
    m_ui->scrollAreaWidgetContents->layout()->addWidget(m_deviceEnableWidget);

    m_deviceInfoWidget = new DeviceInfoSettingWidget(this);
    m_ui->scrollAreaWidgetContents->layout()->addWidget(m_deviceInfoWidget);

    const QVector<OHBDeviceConfigInfo> devices = OHBDeviceConfig::getInstance().readDevices();
    if (!devices.isEmpty()) {
        const OHBDeviceConfigInfo& firstDeviceConfig = devices.first();
        m_purgeFlowWidget->setInitialConfigValue(firstDeviceConfig.getPurgeFlowLitersPerMinute());
        m_pneumaticValvePressureWidget->setInitialConfigValue(firstDeviceConfig.getVppePressureBar());
        m_deviceEnableWidget->setInitialConfigValue(firstDeviceConfig.isEnabled());
    }

    registerModulePermissions();

    m_ui->scrollAreaWidgetContents->layout()->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void ConfigPage::initNav()
{
    m_ui->widgetTop->setProperty("nav", "top");
    QList<QToolButton *> btns = m_ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *btn, btns) {
        btn->setCheckable(true);
        connect(btn, &QPushButton::clicked, this, &ConfigPage::onNavBtnClicked);
    }
}

void ConfigPage::onNavBtnClicked()
{
    QToolButton *btn = static_cast<QToolButton *>(sender());
    QString objName = btn->objectName();

    QList<QToolButton *> btns = m_ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *b, btns) {
        b->setChecked(b == btn);
    }

    QWidget *targetWidget = nullptr;
    if (objName == "btnIdelPurge") {
        targetWidget = m_idlePurgeWidget;
    } else if (objName == "btnPneumaticValvePressure") {
        targetWidget = m_pneumaticValvePressureWidget;
    } else if (objName == "btnSH85PeriodicSelfCheck") {
        targetWidget = m_sh85PeriodicSelfCheckWidget;
    } else if (objName == "btnSH85SelfCheck") {
        targetWidget = m_sh85SelfCheckWidget;
    } else if (objName == "btnPurgeFlow") {
        targetWidget = m_purgeFlowWidget;
    } else if (objName == "btnDeviceEnable") {
        targetWidget = m_deviceEnableWidget;
    } else if (objName == "btnDeviceInfo") {
        targetWidget = m_deviceInfoWidget;
    }

    if (targetWidget) {
        QPoint pos = targetWidget->mapTo(m_ui->scrollAreaWidgetContents, QPoint(0, 0));
        m_ui->scrollArea->verticalScrollBar()->setValue(pos.y());
    }
}

void ConfigPage::registerModulePermissions()
{
    App::registerModulePermission(m_idlePurgeWidget,
                                  m_ui->btnIdelPurge,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("IdlePurgeConfiguration"));
    App::registerModulePermission(m_idlePurgeWidget ? m_idlePurgeWidget->preparationTimeItem() : nullptr,
                                  nullptr,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("IdlePurgeConfiguration.PreparationTime"));
    App::registerModulePermission(m_pneumaticValvePressureWidget,
                                  m_ui->btnPneumaticValvePressure,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("PneumaticValvePressureConfiguration"));
    App::registerModulePermission(m_sh85PeriodicSelfCheckWidget,
                                  m_ui->btnSH85PeriodicSelfCheck,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("SH85PeriodicSelfCheckConfiguration"));
    App::registerModulePermission(m_sh85SelfCheckWidget,
                                  m_ui->btnSH85SelfCheck,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("SH85SelfCheckConfiguration"));
    App::registerModulePermission(m_purgeFlowWidget,
                                  m_ui->btnPurgeFlow,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("PurgeFlowConfiguration"));
    App::registerModulePermission(m_deviceEnableWidget,
                                  m_ui->btnDeviceEnable,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("DeviceEnableConfiguration"));
    App::registerModulePermission(m_deviceInfoWidget,
                                  m_ui->btnDeviceInfo,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("DeviceInfoConfiguration"));
    App::registerModulePermission(m_deviceInfoWidget ? m_deviceInfoWidget->viewItem() : nullptr,
                                  nullptr,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("DeviceInfoConfiguration.ViewDeviceInformation"));
    App::registerModulePermission(m_deviceInfoWidget ? m_deviceInfoWidget->currentQrCodeItem() : nullptr,
                                  nullptr,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("DeviceInfoConfiguration.CurrentQRCode"));
    App::registerModulePermission(m_deviceInfoWidget ? m_deviceInfoWidget->qrCodeModifyItem() : nullptr,
                                  nullptr,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("DeviceInfoConfiguration.NewQRCode"));
    App::registerModulePermission(m_deviceInfoWidget ? m_deviceInfoWidget->endpointModifyItem() : nullptr,
                                  nullptr,
                                  QStringLiteral("ConfigPage"),
                                  QStringLiteral("DeviceInfoConfiguration.IPPort"));
}

void ConfigPage::onSelfCheckRunningStateChanged(bool running)
{
    if (m_sh85PeriodicSelfCheckWidget) {
        m_sh85PeriodicSelfCheckWidget->setPeriodicActionEnabled(!running);
    }
}

void ConfigPage::onPeriodicSelfCheckRunningStateChanged(bool running)
{
    if (m_sh85SelfCheckWidget) {
        m_sh85SelfCheckWidget->setCheckActionEnabled(!running);
    }
}
