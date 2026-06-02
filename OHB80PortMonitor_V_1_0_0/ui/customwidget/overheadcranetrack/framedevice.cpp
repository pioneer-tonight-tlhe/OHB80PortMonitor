#include "framedevice.h"
#include "ui_framedevice.h"
#include <QDebug>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QHash>
#include <cmath>

// 状态颜色映射表定义（仅针对 Foup 类型）
const QMap<FrameDevice::DeviceStatus, QColor> FrameDevice::StatusColorMap = {
    {DeviceStatus::Alarm, QColor(255, 0, 0)},           // 警报红色
    {DeviceStatus::FoupOut, QColor(255, 255, 255)},     // 白色
    {DeviceStatus::FoupIn, QColor(173, 216, 230)},      // 浅蓝色
    {DeviceStatus::PurgeTime5Min, QColor(135, 206, 250)}, // 比浅蓝色深一点
    {DeviceStatus::PurgeTime10Min, QColor(70, 130, 180)},  // 钢蓝色
    {DeviceStatus::PurgeTime20Min, QColor(25, 25, 112)},    // 深午夜蓝
    {DeviceStatus::PurgeTime30Min, QColor(50, 205, 50)},    // 浅绿色
    {DeviceStatus::Disable, QColor(128, 128, 128)}         // 禁用灰色
};

FrameDevice::FrameDevice(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FrameDevice)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_StyledBackground, true);
    m_fontSize = 9;
}

FrameDevice::~FrameDevice()
{
    delete ui;
}

void FrameDevice::initializeUI(DeviceType type)
{
    m_deviceType = type;
    
    // 根据设备类型控制 gboxDeviceTitle 的显示
    if (type == DeviceType::Foup) {
        // Foup 类型：隐藏标题栏
        ui->gboxDeviceTitle->setVisible(false);
    } else {
        // Set 或 Bay 类型：显示标题栏并设置文本
        ui->gboxDeviceTitle->setVisible(true);
        
        QString typeText;
        switch (type) {
            case DeviceType::Set:
                typeText = "Set";
                break;
            case DeviceType::Bay:
                typeText = "Bay";
                break;
            case DeviceType::Foup:
                typeText = "Foup";
                break;
        }
        ui->labType->setText(typeText);
    }
}

void FrameDevice::setDeviceStatus(DeviceStatus status)
{
    QColor statusColor = getBackgroundColor(status);
    
    // 根据状态决定字体颜色
    QString textColor;
    if (status == DeviceStatus::PurgeTime10Min || 
        status == DeviceStatus::PurgeTime20Min || 
        status == DeviceStatus::Alarm ||
        status == DeviceStatus::Disable) {
        textColor = "color: rgb(255, 255, 255);"; // 白色字体
    } else {
        textColor = ""; // 保持默认字体颜色
    }
    
    // 存储背景色样式，仅应用到子控件（QFrame 和 QLabel），不影响 FrameDevice 本身
    QString bgColor = QString("rgb(%1, %2, %3)")
                      .arg(statusColor.red())
                      .arg(statusColor.green())
                      .arg(statusColor.blue());
    m_bgStyleSheet = QString(
        "#gboxDeviceTitle, #gboxDevicePanel { background-color: %1; } "
        "QLabel { background-color: %1; %2 }"
    )
        .arg(bgColor)
        .arg(textColor);
    updateStyleSheet();
}

QColor FrameDevice::getBackgroundColor(DeviceStatus status)
{
    return StatusColorMap.value(status, QColor(128, 128, 128)); // 默认灰色
}

FrameDevice::DeviceStatus FrameDevice::calculateStatusByPurgeTime(QSharedPointer<FoupOfOHBInfo> foupInfo)
{
    // 检查 foupInfo 是否有效
    if (!foupInfo) {
        return DeviceStatus::FoupOut;
    }

    // 如果有警报，返回 Alarm 状态
    if (foupInfo->hasAlarm()) {
        return DeviceStatus::Alarm;
    }

    // 如果 Foup 不在位，返回 FoupOut
    if (!foupInfo->foupIn()) {
        return DeviceStatus::FoupOut;
    }

    // Foup 在位，根据 purgeTimeSec 计算状态
    double purgeTimeMin = foupInfo->purgeTimeSec() / 60.0; // 秒转分钟

    if (purgeTimeMin >= 30.0) {
        return DeviceStatus::PurgeTime30Min;
    } else if (purgeTimeMin >= 20.0) {
        return DeviceStatus::PurgeTime20Min;
    } else if (purgeTimeMin >= 10.0) {
        return DeviceStatus::PurgeTime10Min;
    } else if (purgeTimeMin >= 5.0) {
        return DeviceStatus::PurgeTime5Min;
    } else {
        // purgeTimeMin < 5.0 分钟，Foup 在位但充气时间不足 5 分钟
        return DeviceStatus::FoupIn;
    }
}

void FrameDevice::setLabIDValueText(const QString& text)
{
    ui->labIDValue->setText(text);
}

void FrameDevice::setLabInletPressureValue(const QString& text)
{
    ui->labInletPressureValue->setText(text);
}

void FrameDevice::setLabInletFlowValue(const QString& text)
{
    ui->labInletFlowValue->setText(text);
}

void FrameDevice::setLabRHValue(const QString& text)
{
    ui->labRHValue->setText(text);
}

void FrameDevice::setLabInletPressureValue(float value)
{
    // 检查数据是否为浮点数
    if (std::isnan(value) || std::isinf(value)) {
        qWarning() << "setLabInletPressureValue: 输入值不是有效的浮点数";
        return;
    }
    
    // 显示值并添加单位 Mpa
    QString displayText = QString::number(value, 'f', 2) + " Mpa";
    ui->labInletPressureValue->setText(displayText);
}

void FrameDevice::setLabInletFlowValue(float value)
{
    // 检查数据是否为浮点数
    if (std::isnan(value) || std::isinf(value)) {
        qWarning() << "setLabInletFlowValue: 输入值不是有效的浮点数";
        return;
    }
    
    // 显示值并添加单位 L/Min
    QString displayText = QString::number(value, 'f', 2) + " L/Min";
    ui->labInletFlowValue->setText(displayText);
}

void FrameDevice::setLabRHValue(float value)
{
    // 检查数据是否为浮点数
    if (std::isnan(value) || std::isinf(value)) {
        qWarning() << "setLabRHValue: 输入值不是有效的浮点数";
        return;
    }
    
    // 显示值并添加单位 %
    QString displayText = QString::number(value, 'f', 2) + " %";
    ui->labRHValue->setText(displayText);
}

void FrameDevice::setZoomLevel(int level)
{
    int newSize;
    switch (level) {
        case -5: newSize = 3;  break;
        case -4: newSize = 4;  break;
        case -3:
            if (m_deviceType == DeviceType::Foup)
                newSize = 5;
            else
                newSize = 4;
            break;
        case -2: newSize = 6;  break;
        case -1: newSize = 7;  break;
        case  0: newSize = 8;  break;
        case  1:
        case  2:
        case  3:
        case  4:
        case  5: newSize = 10; break;
        default:
            if (level < -3)
                newSize = 5;
            else
                newSize = 15;
            break;
    }
    m_fontSize = newSize;
    updateStyleSheet();
}

void FrameDevice::updateStyleSheet()
{
    QString style = m_bgStyleSheet;
    style += QString("\n* { font-size: %1pt; }").arg(m_fontSize);
    setStyleSheet(style);
}

void FrameDevice::setSetOfOHBInfo(QSharedPointer<SetOfOHBInfo> setInfo)
{
    m_setInfo = setInfo;
    if (m_setInfo) {
        // 直接更新UI显示Set信息
        ui->labIDValue->setText(m_setInfo->getSetId());
        ui->labInletPressureValue->setText(m_setInfo->getInletPressureRange());
        ui->labInletFlowValue->setText(m_setInfo->getInletFlowAverage());
        ui->labRHValue->setText(m_setInfo->getRHRange());
    }
}

void FrameDevice::setFoupOfOHBInfo(QSharedPointer<FoupOfOHBInfo> foupInfo)
{
    m_foupInfo = foupInfo;
    if (m_foupInfo) {
        // 更新UI显示Foup信息
        setLabIDValueText(m_foupInfo->qrCode());
        setLabInletPressureValue(m_foupInfo->inletPressure());
        setLabInletFlowValue(m_foupInfo->inletFlow());
        setLabRHValue(m_foupInfo->RH());
    }
}

void FrameDevice::setBayOfOHBInfo(QSharedPointer<BayOfOHBInfo> bayInfo)
{
    m_bayInfo = bayInfo;
    if (m_bayInfo) {
        // 更新UI显示Bay信息
        setLabIDValueText(m_bayInfo->getBayId());
        // Bay级别的信息可能需要进一步定义
        setLabInletPressureValue(0.0);
        setLabInletFlowValue(0.0);
        setLabRHValue(0.0);
    }
}

void FrameDevice::updateFoupInfo()
{
    if (!m_foupInfo) {
        return;
    }

    // 更新数据显示
    setLabInletPressureValue(m_foupInfo->inletPressure());
    setLabInletFlowValue(m_foupInfo->inletFlow());
    setLabRHValue(m_foupInfo->RH());

    // 仅针对 Foup 类型：检测 enable 变量
    if (m_deviceType == DeviceType::Foup) {
        if (!m_foupInfo->enable()) {
            // enable 为 false，设置背景颜色为灰色
            setDeviceStatus(DeviceStatus::Disable);
        } else {
            // enable 为 true，根据 FoupOfOHBInfo 计算状态并设置背景颜色
            DeviceStatus status = calculateStatusByPurgeTime(m_foupInfo);
            setDeviceStatus(status);
        }
    }
}

void FrameDevice::updateSetInfo()
{
    if (!m_setInfo) {
        return;
    }
    
    // 从绑定的 SetOfOHBInfo 读取最新数据并更新 UI
    setLabIDValueText(m_setInfo->getSetId());
    setLabInletPressureValue(m_setInfo->getInletPressureRange());
    setLabInletFlowValue(m_setInfo->getInletFlowAverage());
    setLabRHValue(m_setInfo->getRHRange());

    // 检查 Set 下的所有 Foup 是否有 hasAlarm 为 true 的
    // Set 总览只统计启用设备的告警；禁用设备保持数据可见，但不影响 Set 告警状态。
    const QVector<FoupOfOHBInfo>& foups = m_setInfo->getFoups();
    bool hasAlarmFoup = false;
    for (const auto& foup : foups) {
        if (foup.enable() && foup.hasAlarm()) {
            hasAlarmFoup = true;
            break;
        }
    }

    if (hasAlarmFoup) {
        setDeviceStatus(DeviceStatus::Alarm);
    } else {
        m_bgStyleSheet.clear();
        updateStyleSheet();
    }
}

void FrameDevice::setSelected(bool selected)
{
    if (m_selected != selected) {
        m_selected = selected;
        update();  // 触发重绘以显示/隐藏边框
    }
}

void FrameDevice::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    
    if (m_selected) {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        
        // 外层发光效果（深棕色）
        QPen glowPen(QColor(101, 67, 33), 10, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        painter.setPen(glowPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(5, 5, width() - 10, height() - 10);
        
        // 内层主边框（亮绿色）
        QPen mainPen(QColor(50, 205, 50), 8, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        painter.setPen(mainPen);
        painter.drawRect(4, 4, width() - 8, height() - 8);
    }
}

void FrameDevice::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int uiId = -1;
        if (m_deviceType == DeviceType::Set && m_setInfo) {
            uiId = m_setInfo->getUiId();
        } else if (m_deviceType == DeviceType::Foup && m_foupInfo) {
            // Foup 使用 qrCode 的哈希值作为 uiId
            uiId = qHash(m_foupInfo->qrCode());
        }
        emit deviceClicked(m_deviceType, uiId);
    }
    QWidget::mousePressEvent(event);
}

void FrameDevice::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_deviceType == DeviceType::Set && m_setInfo) {
        const QVector<FoupOfOHBInfo>& foups = m_setInfo->getFoups();
        if (!foups.isEmpty()) {
            QString firstFoupQrCode = foups.first().qrCode();
            int uiId = m_setInfo->getUiId();
            emit setDeviceDoubleClicked(uiId, firstFoupQrCode);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}
