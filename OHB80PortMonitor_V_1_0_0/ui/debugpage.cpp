#include "debugpage.h"
#include "ui_debugpage.h"
#include "customwidget/debugsettingwidget/firmwareupdateconfigsettingwidget.h"
#include "customwidget/debugsettingwidget/firmwareupdatesettingwidget.h"
#include "customwidget/debugsettingwidget/vefcgastypesettingwidget.h"
#include "customwidget/debugsettingwidget/uirefreshtimesettingwidget.h"
#include "customwidget/debugsettingwidget/vefcflowunitmediumstatuswidget.h"
#include "customwidget/debugsettingwidget/boardenablestatuswidget.h"
#include <QScrollBar>
#include <QScroller>
#include <QScrollerProperties>

DebugPage::DebugPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DebugPage)
    , m_firmwareConfigWidget(nullptr)
    , m_firmwareUpdateWidget(nullptr)
    , m_vefcGasTypeWidget(nullptr)
    , m_uiRefreshTimeWidget(nullptr)
    , m_vefcFlowUnitMediumStatusWidget(nullptr)
    , m_boardEnableStatusWidget(nullptr)
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

    // 固件更新配置 SettingWidget
    m_firmwareConfigWidget = new FirmwareUpdateConfigSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_firmwareConfigWidget);

    // 固件升级 SettingWidget（独立的 SettingWidget，与配置界面平级）
    m_firmwareUpdateWidget = new FirmwareUpdateSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_firmwareUpdateWidget);

    // 配置界面选择 bin 文件后 → 同步到升级界面
    connect(m_firmwareConfigWidget, &FirmwareUpdateConfigSettingWidget::binFilePathChanged,
            m_firmwareUpdateWidget, &FirmwareUpdateSettingWidget::setFirmwareFilePath);

    // VEFC 气体介质类型 SettingWidget
    m_vefcGasTypeWidget = new VEFCGasTypeSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_vefcGasTypeWidget);

    // UI 页面刷新时间 SettingWidget
    m_uiRefreshTimeWidget = new UIRefreshTimeSettingWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_uiRefreshTimeWidget);

    // VEFC 流量单位 / 介质状态读取 SettingWidget
    m_vefcFlowUnitMediumStatusWidget = new VEFCFlowUnitMediumStatusWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_vefcFlowUnitMediumStatusWidget);

    // 板卡禁用状态读取 SettingWidget
    m_boardEnableStatusWidget = new BoardEnableStatusWidget(this);
    ui->scrollAreaWidgetContents->layout()->addWidget(m_boardEnableStatusWidget);
    
    ui->scrollAreaWidgetContents->layout()->addItem(
        new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding)
    );
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
    QToolButton *btn = (QToolButton *)sender();
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
    } else if (objName == "btnVEFCStatus") {
        targetWidget = m_vefcFlowUnitMediumStatusWidget;
    } else if (objName == "btnBoardEnable") {
        targetWidget = m_boardEnableStatusWidget;
    }

    if (targetWidget) {
        QPoint pos = targetWidget->mapTo(ui->scrollAreaWidgetContents, QPoint(0, 0));
        ui->scrollArea->verticalScrollBar()->setValue(pos.y());
    }
}
