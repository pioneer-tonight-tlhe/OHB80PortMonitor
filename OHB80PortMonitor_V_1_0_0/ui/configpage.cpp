#include "configpage.h"

#include "ui_configpage.h"
#include "customwidget/configsettingwidget/deviceenablesettingwidget.h"
#include "customwidget/configsettingwidget/deviceinfosettingwidget.h"
#include "customwidget/configsettingwidget/idlepurgesettingwidget.h"
#include "customwidget/configsettingwidget/pneumaticvalvepressuresettingwidget.h"
#include "customwidget/configsettingwidget/purgeflowsettingwidget.h"
#include "customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.h"
#include "customwidget/configsettingwidget/sh85selfchecksettingwidget.h"

#include <QScroller>
#include <QScrollerProperties>
#include <QScrollBar>

ConfigPage::ConfigPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConfigPage)
    , m_idlePurgeWidget(nullptr)
    , m_pneumaticValvePressureWidget(nullptr)
    , m_sh85PeriodicSelfCheckWidget(nullptr)
    , m_sh85SelfCheckWidget(nullptr)
    , m_deviceEnableWidget(nullptr)
    , m_purgeFlowWidget(nullptr)
    , m_deviceInfoWidget(nullptr)
{
    ui->setupUi(this);
    initUI();
}

ConfigPage::~ConfigPage()
{
    delete ui;
}

void ConfigPage::initUI()
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

    m_idlePurgeWidget = new IdlePurgeSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_idlePurgeWidget);

    m_pneumaticValvePressureWidget = new PneumaticValvePressureSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_pneumaticValvePressureWidget);

    m_sh85PeriodicSelfCheckWidget = new SH85PeriodicSelfCheckSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_sh85PeriodicSelfCheckWidget);
    connect(m_sh85PeriodicSelfCheckWidget, &SH85PeriodicSelfCheckSettingWidget::runningStateChanged,
            this, &ConfigPage::onPeriodicSelfCheckRunningStateChanged);

    m_sh85SelfCheckWidget = new SH85SelfCheckSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_sh85SelfCheckWidget);
    connect(m_sh85SelfCheckWidget, &SH85SelfCheckSettingWidget::runningStateChanged,
            this, &ConfigPage::onSelfCheckRunningStateChanged);
    onPeriodicSelfCheckRunningStateChanged(m_sh85PeriodicSelfCheckWidget->isRunning());
    onSelfCheckRunningStateChanged(m_sh85SelfCheckWidget->isRunning());

    m_purgeFlowWidget = new PurgeFlowSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_purgeFlowWidget);

    m_deviceEnableWidget = new DeviceEnableSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_deviceEnableWidget);

    m_deviceInfoWidget = new DeviceInfoSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_deviceInfoWidget);

    ui->scrollAreaWidgetContents->layout()->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void ConfigPage::initNav()
{
    ui->widgetTop->setProperty("nav", "top");
    QList<QToolButton *> btns = ui->widgetTop->findChildren<QToolButton *>();
    foreach (QToolButton *btn, btns) {
        btn->setCheckable(true);
        connect(btn, &QPushButton::clicked, this, &ConfigPage::navBtnClicked);
    }
}

void ConfigPage::navBtnClicked()
{
    QToolButton *btn = static_cast<QToolButton *>(sender());
    QString objName = btn->objectName();

    QList<QToolButton *> btns = ui->widgetTop->findChildren<QToolButton *>();
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
        QPoint pos = targetWidget->mapTo(ui->scrollAreaWidgetContents, QPoint(0, 0));
        ui->scrollArea->verticalScrollBar()->setValue(pos.y());
    }
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
