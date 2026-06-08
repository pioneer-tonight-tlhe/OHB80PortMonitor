#include "scrollcontroller.h"
#include "framedevice.h"
#include <QDebug>

ScrollController::ScrollController(QWidget* parentWidget, QObject* parent)
    : QObject(parent)
    , m_scrollOffset(0, 0)
{
    // 创建滚动条
    m_horizontalScrollBar = new QScrollBar(Qt::Horizontal, parentWidget);
    m_verticalScrollBar = new QScrollBar(Qt::Vertical, parentWidget);
    
    // 连接滚动条信号
    connect(m_horizontalScrollBar, &QScrollBar::valueChanged,
            this, &ScrollController::onHorizontalScrollChanged);
    connect(m_verticalScrollBar, &QScrollBar::valueChanged,
            this, &ScrollController::onVerticalScrollChanged);
}

ScrollController::~ScrollController()
{
}

void ScrollController::updateScrollBars(const QSize& contentSize, const QSize& viewportSize)
{
    // 计算滚动条的范围
    int hMax = qMax(0, contentSize.width() - viewportSize.width());
    int vMax = qMax(0, contentSize.height() - viewportSize.height());
    
    // 设置水平滚动条
    m_horizontalScrollBar->setRange(0, hMax);
    m_horizontalScrollBar->setPageStep(viewportSize.width());
    m_horizontalScrollBar->setSingleStep(viewportSize.width() / 10);
    
    // 设置垂直滚动条
    m_verticalScrollBar->setRange(0, vMax);
    m_verticalScrollBar->setPageStep(viewportSize.height());
    m_verticalScrollBar->setSingleStep(viewportSize.height() / 10);
    
    // 显示或隐藏滚动条
    m_horizontalScrollBar->setVisible(hMax > 0);
    m_verticalScrollBar->setVisible(vMax > 0);
}

void ScrollController::scrollContentsBy(int dx, int dy)
{
    m_scrollOffset.setX(m_scrollOffset.x() + dx);
    m_scrollOffset.setY(m_scrollOffset.y() + dy);
    
    emit scrollOffsetChanged(m_scrollOffset);
}

void ScrollController::scrollToDevice(QSharedPointer<FrameDevice> device, const QSize& viewportSize, int /*offsetPixels*/)
{
    if (!device) {
        return;
    }
    
    // 计算目标控件的中心点
    int deviceCenterX = device->pos().x() + device->width() / 2;
    int deviceCenterY = device->pos().y() + device->height() / 2;
    
    // 计算滚动目标：让控件中心对齐到可视区域中心
    int viewportCenterX = viewportSize.width() / 2;
    int viewportCenterY = viewportSize.height() / 2;
    
    int targetX = deviceCenterX - viewportCenterX;
    int targetY = deviceCenterY - viewportCenterY;
    
    // 设置滚动条位置
    if (m_horizontalScrollBar->isVisible()) {
        m_horizontalScrollBar->setValue(targetX);
    }
    if (m_verticalScrollBar->isVisible()) {
        m_verticalScrollBar->setValue(targetY);
    }
}

void ScrollController::resetScroll()
{
    m_horizontalScrollBar->setValue(0);
    m_verticalScrollBar->setValue(0);
    m_scrollOffset = QPoint(0, 0);
}

void ScrollController::onHorizontalScrollChanged(int value)
{
    int dx = value - m_scrollOffset.x();
    scrollContentsBy(dx, 0);
}

void ScrollController::onVerticalScrollChanged(int value)
{
    int dy = value - m_scrollOffset.y();
    scrollContentsBy(0, dy);
}
