#ifndef SCROLLCONTROLLER_H
#define SCROLLCONTROLLER_H

#include <QObject>
#include <QScrollBar>
#include <QPoint>
#include <QSize>
#include <QSharedPointer>

class FrameDevice;

/**
 * @brief 滚动控制器，负责管理地图的滚动条和滚动逻辑
 */
class ScrollController : public QObject
{
    Q_OBJECT

public:
    explicit ScrollController(QWidget* parentWidget, QObject* parent = nullptr);
    ~ScrollController();

    // 获取滚动条
    QScrollBar* horizontalScrollBar() const { return m_horizontalScrollBar; }
    QScrollBar* verticalScrollBar() const { return m_verticalScrollBar; }
    
    // 获取滚动偏移量
    QPoint scrollOffset() const { return m_scrollOffset; }
    
    // 更新滚动条范围和位置
    void updateScrollBars(const QSize& contentSize, const QSize& viewportSize);
    
    // 滚动内容
    void scrollContentsBy(int dx, int dy);
    
    // 滚动到指定设备
    void scrollToDevice(QSharedPointer<FrameDevice> device, const QSize& viewportSize, int offsetPixels = 10);
    
    // 重置滚动位置
    void resetScroll();

signals:
    void scrollOffsetChanged(const QPoint& offset);

private slots:
    void onHorizontalScrollChanged(int value);
    void onVerticalScrollChanged(int value);

private:
    QScrollBar* m_horizontalScrollBar;
    QScrollBar* m_verticalScrollBar;
    QPoint m_scrollOffset;
};

#endif // SCROLLCONTROLLER_H
