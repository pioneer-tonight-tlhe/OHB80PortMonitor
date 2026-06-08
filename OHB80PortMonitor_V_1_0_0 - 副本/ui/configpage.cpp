#include "configpage.h"
#include "ui_configpage.h"
#include "customwidget/configsettingwidget/idlepurgesettingwidget.h"
#include "customwidget/configsettingwidget/pneumaticvalvepressuresettingwidget.h"
#include "customwidget/configsettingwidget/sh85periodicselfchecksettingwidget.h"
#include "customwidget/configsettingwidget/sh85selfchecksettingwidget.h"
#include "customwidget/configsettingwidget/humidityoffsetsettingwidget.h"
#include "customwidget/configsettingwidget/purgeflowsettingwidget.h"
#include "customwidget/configsettingwidget/deviceenablesettingwidget.h"
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>

ConfigPage::ConfigPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ConfigPage)
    , m_idlePurgeWidget(nullptr)
    , m_pneumaticValvePressureWidget(nullptr)
    , m_sh85PeriodicSelfCheckWidget(nullptr)
    , m_sh85SelfCheckWidget(nullptr)
    , m_humidityOffsetWidget(nullptr)
    , m_deviceEnableWidget(nullptr)
    , m_purgeFlowWidget(nullptr)
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

    // 启用触摸/鼠标拖动滚动手势（支持触屏滑动滚动区域）
    if (ui->scrollArea && ui->scrollArea->viewport()) {
        QScroller::grabGesture(ui->scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
        QScroller* scroller = QScroller::scroller(ui->scrollArea->viewport());
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

    m_humidityOffsetWidget = new HumidityOffsetSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_humidityOffsetWidget);

    m_purgeFlowWidget = new PurgeFlowSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_purgeFlowWidget);

    m_deviceEnableWidget = new DeviceEnableSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_deviceEnableWidget);

    ui->scrollAreaWidgetContents->layout()->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding)
        );
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
    QToolButton *btn = (QToolButton *)sender();
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
    } else if (objName == "btnHumidityOffset") {
        targetWidget = m_humidityOffsetWidget;
    } else if (objName == "btnPurgeFlow") {
        targetWidget = m_purgeFlowWidget;
    } else if (objName == "btnDeviceEnable") {
        targetWidget = m_deviceEnableWidget;
    }

    if (targetWidget) {
        QPoint pos = targetWidget->mapTo(ui->scrollAreaWidgetContents, QPoint(0, 0));
        ui->scrollArea->verticalScrollBar()->setValue(pos.y());
    }
}

// ============================================================
// 协调 SH85 自检控件
// ============================================================

void ConfigPage::onSelfCheckRunningStateChanged(bool running)
{
    // 手动自检状态变化时，禁用/启用定期自检控件
    if (m_sh85PeriodicSelfCheckWidget) {
        m_sh85PeriodicSelfCheckWidget->setPeriodicActionEnabled(!running);
    }
}

void ConfigPage::onPeriodicSelfCheckRunningStateChanged(bool running)
{
    // 定期自检状态变化时，禁用/启用手动自检控件
    if (m_sh85SelfCheckWidget) {
        m_sh85SelfCheckWidget->setCheckActionEnabled(!running);
    }
}
