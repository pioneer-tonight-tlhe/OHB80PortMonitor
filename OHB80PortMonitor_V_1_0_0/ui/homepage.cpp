#include "homepage.h"
#include "ui_homepage.h"
#include "app.h"
#include "qthelper.h"
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QCoreApplication>
#include <QDir>
#include <QPointF>

HomePage::HomePage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::HomePage)
{
    ui->setupUi(this);

    // // 查询 D 盘磁盘情况
    // qint64 totalBytes = QtHelper::diskTotalBytes("D:/");
    // qint64 usedBytes  = QtHelper::diskUsedBytes("D:/");
    // if (totalBytes >= 0 && usedBytes >= 0) {
    //     double totalGB = totalBytes / 1024.0 / 1024.0 / 1024.0;
    //     double usedGB  = usedBytes  / 1024.0 / 1024.0 / 1024.0;
    //     QMessageBox::information(this, "D 盘磁盘情况",
    //         QString("D 盘总容量: %1 GB\n已用: %2 GB").arg(totalGB, 0, 'f', 2).arg(usedGB, 0, 'f', 2));
    // } else {
    //     QMessageBox::warning(this, "磁盘信息", "无法获取 D 盘磁盘信息");
    // }

    initUI();
}

HomePage::~HomePage()
{
    delete ui;
}

void HomePage::initUI()
{
    // 初始化 labDetailed
    ui->labDetailed->setFixedHeight(35);
    ui->labDetailed->setStyleSheet("background-color: #2c3e50; color: white;"
                                   "border: 1px solid #34495e; padding: 4px;"
                                   "font-size: 11pt; font-weight: bold;");
    ui->labDetailed->setText("Detailed Information");

    // 初始化设备监控器
    initDeviceMonitorWidget();

    // 初始化天车地图控件
    initCraneMapWidget();

    // 初始化 overheadCranesWidget（天车提示控件）
    initOverheadCranesWidget();
}

void HomePage::initDeviceMonitorWidget()
{
    // 初始化设备监控器，放入占位控件中
    m_deviceMonitor = new Graph::DeviceMonitorWidget(ui->deviceMonitorPlaceholder);
    QVBoxLayout* monitorLayout = new QVBoxLayout(ui->deviceMonitorPlaceholder);
    monitorLayout->setContentsMargins(0, 0, 0, 0);
    monitorLayout->addWidget(m_deviceMonitor);

    // 连接设备监控器到 CraneMapWidget
    m_deviceMonitor->bindCraneMapWidget(ui->widget);
    m_deviceMonitor->setRefreshInterval(1000);
    m_deviceMonitor->startRefresh();
}

void HomePage::initCraneMapWidget()
{
    // 初始化 CraneMapWidget
    ui->widget->setFixSpacingValue(6.0);
    ui->widget->setFrameDeviceTrackGap(8.0);
    ui->widget->setStartNodePosition(QPointF(20.0, 120.0));

    // 加载配置文件
    QString configPath = App::getConfig()->getGraphConfigPath();
    if (!ui->widget->loadConfig(configPath)) {
        qWarning() << "Failed to load graph config from:" << configPath;
    }

    // 自动计算地图边界并居中显示（上下左右各留50像素边距）
    ui->widget->centerMap(50, 50, 50, 50);

    // 设置控件尺寸以显示滚动条
    ui->widget->resize(ui->widget->sizeHint());

    // 连接视图切换按钮
    connect(ui->btnFoupView, &QPushButton::clicked, this, [this]() {
        if (ui->widget->currentMapLevel() == CraneMapWidget::MapLevel::FoupLevel) {
            return;
        }
        ui->widget->switchMapLevel(CraneMapWidget::MapLevel::FoupLevel);
        ui->widget->resize(ui->widget->sizeHint());
        // 切换后缩放级别已重置为0，设置合适的缩放级别
        ui->widget->setZoomLevel(-4);
    });
    connect(ui->btnSetView, &QPushButton::clicked, this, [this]() {
        if (ui->widget->currentMapLevel() == CraneMapWidget::MapLevel::SetLevel) {
            return;
        }
        ui->widget->switchMapLevel(CraneMapWidget::MapLevel::SetLevel);
        ui->widget->resize(ui->widget->sizeHint());
        // 切换后缩放级别已重置为0，设置合适的缩放级别
        ui->widget->setZoomLevel(-4);
    });

    // 连接缩放按钮
    connect(ui->btnZoomIn, &QPushButton::clicked, ui->widget, &CraneMapWidget::zoomIn);
    connect(ui->btnZoomOut, &QPushButton::clicked, ui->widget, &CraneMapWidget::zoomOut);

    // 初始界面完成，调整初始界面伸缩比例
    connect(ui->widget, &CraneMapWidget::configLoadFinished, this, [this](bool isOk)
    {
        if (isOk)
        {
            // 设置合适的初始缩放级别
            ui->widget->setZoomLevel(-4);

            // 初始时选中第一个 Set
            ui->widget->selectFirstSet();
        }
        else
            qDebug() << "配置文件加载失败！";
    });
}

void HomePage::initOverheadCranesWidget()
{
    // 初始化 overheadCranesWidget 的样式，参考 labDetailed 的样式设置
    ui->overheadCranesWidget->setFixedHeight(35);
    ui->overheadCranesWidget->setStyleSheet("#overheadCranesWidget { background-color: #2c3e50; color: white;"
                                             "border: 1px solid #34495e; padding: 4px;"
                                             "font-size: 12pt; font-weight: bold; }");

    // 设置颜色标签的背景色，使用 FrameDevice 的状态颜色映射
    QString commonStyle = "border: 1px solid white;";

    // 状态和对应的颜色标签映射
    struct StatusLabelPair {
        FrameDevice::DeviceStatus status;
        QLabel* colorLabel;
        QLabel* textLabel;
    };

    StatusLabelPair labels[] = {
        {FrameDevice::DeviceStatus::FoupIn, ui->labFoupInColor, ui->labFoupInText},
        {FrameDevice::DeviceStatus::PurgeTime5Min, ui->labPurgeTime5MinColor, ui->labPurgeTime5MinText},
        {FrameDevice::DeviceStatus::PurgeTime10Min, ui->labPurgeTime10MinColor, ui->labPurgeTime10MinText},
        {FrameDevice::DeviceStatus::PurgeTime20Min, ui->labPurgeTime20MinColor, ui->labPurgeTime20MinText},
        {FrameDevice::DeviceStatus::PurgeTime30Min, ui->labPurgeTime30MinColor, ui->labPurgeTime30MinText},
        {FrameDevice::DeviceStatus::FoupOut, ui->labFoupOutColor, ui->labFoupOutText},
        {FrameDevice::DeviceStatus::Alarm, ui->labAlarmColor, ui->labAlarmText},
        {FrameDevice::DeviceStatus::Disable, ui->labDisableColor, ui->labDisableText}
    };

    for (const auto& pair : labels) {
        pair.colorLabel->setStyleSheet(QString("background-color: %1; %2")
                                         .arg(FrameDevice::getBackgroundColor(pair.status).name())
                                         .arg(commonStyle));
        pair.textLabel->setStyleSheet("color: white;");
    }
}
