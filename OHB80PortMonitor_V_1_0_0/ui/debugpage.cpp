#include "debugpage.h"

#include "app.h"
#include "ui_debugpage.h"
#include "customwidget/configsettingwidget/humidityoffsetsettingwidget.h"
#include "customwidget/debugsettingwidget/boardenablestatuswidget.h"
#include "customwidget/debugsettingwidget/configfilesettingwidget.h"
#include "customwidget/debugsettingwidget/firmwareupdateconfigsettingwidget.h"
#include "customwidget/debugsettingwidget/firmwareupdatesettingwidget.h"
#include "customwidget/debugsettingwidget/freertostaskstackmonitorwidget.h"
#include "customwidget/debugsettingwidget/foupinautopurgeenablewidget.h"
#include "customwidget/debugsettingwidget/foupinvacuumextractionenablewidget.h"
#include "customwidget/debugsettingwidget/uirefreshtimesettingwidget.h"
#include "customwidget/debugsettingwidget/vefcflowunitmediumstatuswidget.h"
#include "customwidget/debugsettingwidget/vefcgastypesettingwidget.h"
#include "customwidget/debugsettingwidget/vefcsensormonitorwidget.h"
#include "ohbdeviceconfig.h"

#include <QScroller>
#include <QScrollerProperties>
#include <QScrollBar>

DebugPage::DebugPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DebugPage)
    , m_firmwareConfigWidget(nullptr)
    , m_firmwareUpdateWidget(nullptr)
    , m_vefcGasTypeWidget(nullptr)
    , m_uiRefreshTimeWidget(nullptr)
    , m_humidityOffsetWidget(nullptr)
    , m_vefcFlowUnitMediumStatusWidget(nullptr)
    , m_vefcSensorMonitorWidget(nullptr)
    , m_freeRTOSTaskStackMonitorWidget(nullptr)
    , m_boardEnableStatusWidget(nullptr)
    , m_foupInVacuumExtractionEnableWidget(nullptr)
    , m_foupInAutoPurgeEnableWidget(nullptr)
    , m_configFileSettingWidget(nullptr)
{
    ui->setupUi(this);
    initUI();
}

DebugPage::~DebugPage()
{
    delete ui;
}

void DebugPage::initUI()
{
    initNav();

    if (ui->scrollArea && ui->scrollArea->viewport()) {
        QScroller::grabGesture(ui->scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
        QScroller *scroller = QScroller::scroller(ui->scrollArea->viewport());
        QScrollerProperties props = scroller->scrollerProperties();
        props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.005);
        props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.3);
        props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
        scroller->setScrollerProperties(props);
    }

    m_firmwareConfigWidget = new FirmwareUpdateConfigSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_firmwareConfigWidget);

    m_firmwareUpdateWidget = new FirmwareUpdateSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_firmwareUpdateWidget);

    connect(m_firmwareConfigWidget, &FirmwareUpdateConfigSettingWidget::binFilePathChanged,
            m_firmwareUpdateWidget, &FirmwareUpdateSettingWidget::setFirmwareFilePath);

    m_vefcGasTypeWidget = new VEFCGasTypeSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_vefcGasTypeWidget);

    m_uiRefreshTimeWidget = new UIRefreshTimeSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_uiRefreshTimeWidget);

    m_humidityOffsetWidget = new HumidityOffsetSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_humidityOffsetWidget);

    m_vefcFlowUnitMediumStatusWidget = new VEFCFlowUnitMediumStatusWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_vefcFlowUnitMediumStatusWidget);

    m_vefcSensorMonitorWidget = new VEFCSensorMonitorWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_vefcSensorMonitorWidget);

    m_freeRTOSTaskStackMonitorWidget = new FreeRTOSTaskStackMonitorWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_freeRTOSTaskStackMonitorWidget);

    m_boardEnableStatusWidget = new BoardEnableStatusWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_boardEnableStatusWidget);

    m_foupInVacuumExtractionEnableWidget = new FoupInVacuumExtractionEnableWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_foupInVacuumExtractionEnableWidget);

    m_foupInAutoPurgeEnableWidget = new FoupInAutoPurgeEnableWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_foupInAutoPurgeEnableWidget);

    m_configFileSettingWidget = new ConfigFileSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_configFileSettingWidget);

    const QVector<OHBDeviceConfigInfo> devices = OHBDeviceConfig::getInstance().readDevices();
    if (!devices.isEmpty()) {
        const OHBDeviceConfigInfo& firstDeviceConfig = devices.first();
        m_uiRefreshTimeWidget->setInitialConfigValues(firstDeviceConfig.getLogoTimeSeconds(),
                                                      firstDeviceConfig.getPageTotalTimeSeconds(),
                                                      firstDeviceConfig.getPageSwitchIntervalSeconds());
        m_humidityOffsetWidget->setInitialConfigValues(firstDeviceConfig.getHumidityLowerLimitPercent(),
                                                       firstDeviceConfig.getHumidityOffsetPercent());
        m_foupInAutoPurgeEnableWidget->setInitialConfigValue(
            firstDeviceConfig.getFoupInAutoPurgeEnable());
    }

    registerModulePermissions();

    ui->scrollAreaWidgetContents->layout()->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void DebugPage::initNav()
{
    ui->widgetTop->setProperty("nav", "top");
    QList<QToolButton *> btns = ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *btn, btns) {
        btn->setCheckable(true);
        connect(btn, &QPushButton::clicked, this, &DebugPage::navBtnClicked);
    }
}

void DebugPage::navBtnClicked()
{
    QToolButton *btn = static_cast<QToolButton *>(sender());
    QString objName = btn->objectName();

    QList<QToolButton *> btns = ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *b, btns) {
        b->setChecked(b == btn);
    }

    QWidget *targetWidget = nullptr;
    if (objName == "btnFirmwareConfig") {
        targetWidget = m_firmwareConfigWidget;
    } else if (objName == "btnFirmwareUpdate") {
        targetWidget = m_firmwareUpdateWidget;
    } else if (objName == "btnVEFCGasType") {
        targetWidget = m_vefcGasTypeWidget;
    } else if (objName == "btnUIRefreshTime") {
        targetWidget = m_uiRefreshTimeWidget;
    } else if (objName == "btnHumidityOffset") {
        targetWidget = m_humidityOffsetWidget;
    } else if (objName == "btnVEFCStatus") {
        targetWidget = m_vefcFlowUnitMediumStatusWidget;
    } else if (objName == "btnVEFCSensorMonitor") {
        targetWidget = m_vefcSensorMonitorWidget;
    } else if (objName == "btnFreeRTOSTaskStack") {
        targetWidget = m_freeRTOSTaskStackMonitorWidget;
    } else if (objName == "btnBoardEnable") {
        targetWidget = m_boardEnableStatusWidget;
    } else if (objName == "btnFoupInVacuumExtraction") {
        targetWidget = m_foupInVacuumExtractionEnableWidget;
    } else if (objName == "btnFoupInAutoPurge") {
        targetWidget = m_foupInAutoPurgeEnableWidget;
    } else if (objName == "btnConfigFiles") {
        targetWidget = m_configFileSettingWidget;
    }

    if (targetWidget) {
        QPoint pos = targetWidget->mapTo(ui->scrollAreaWidgetContents, QPoint(0, 0));
        ui->scrollArea->verticalScrollBar()->setValue(pos.y());
    }
}

void DebugPage::registerModulePermissions()
{
    App::registerModulePermission(m_firmwareConfigWidget,
                                  ui->btnFirmwareConfig,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("FirmwareConfig"));
    App::registerModulePermission(m_firmwareUpdateWidget,
                                  ui->btnFirmwareUpdate,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("FirmwareUpdate"));
    App::registerModulePermission(m_vefcGasTypeWidget,
                                  ui->btnVEFCGasType,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("VEFCGasTypeConfiguration"));
    App::registerModulePermission(m_uiRefreshTimeWidget,
                                  ui->btnUIRefreshTime,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("UIRefreshTimeConfiguration"));
    App::registerModulePermission(m_humidityOffsetWidget,
                                  ui->btnHumidityOffset,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("HumidityOffsetConfiguration"));
    App::registerModulePermission(m_vefcFlowUnitMediumStatusWidget,
                                  ui->btnVEFCStatus,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("VEFCFlowUnitMediumStatus"));
    App::registerModulePermission(m_vefcSensorMonitorWidget,
                                  ui->btnVEFCSensorMonitor,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("VEFCSensorMonitor"));
    App::registerModulePermission(m_freeRTOSTaskStackMonitorWidget,
                                  ui->btnFreeRTOSTaskStack,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("FreeRTOSTaskStackMonitor"));
    App::registerModulePermission(m_boardEnableStatusWidget,
                                  ui->btnBoardEnable,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("BoardEnableStatus"));
    App::registerModulePermission(m_foupInVacuumExtractionEnableWidget,
                                  ui->btnFoupInVacuumExtraction,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("FoupInVacuumExtractionEnable"));
    App::registerModulePermission(m_foupInAutoPurgeEnableWidget,
                                  ui->btnFoupInAutoPurge,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("FoupInAutoPurgeEnable"));
    App::registerModulePermission(m_configFileSettingWidget,
                                  ui->btnConfigFiles,
                                  QStringLiteral("DebugPage"),
                                  QStringLiteral("ConfigFiles"));
}
