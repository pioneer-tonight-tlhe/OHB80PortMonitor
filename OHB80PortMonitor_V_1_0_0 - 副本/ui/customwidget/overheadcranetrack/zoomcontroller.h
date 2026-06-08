#ifndef ZOOMCONTROLLER_H
#define ZOOMCONTROLLER_H

#include <QObject>
#include <QMap>
#include <QPointF>
#include <QSizeF>
#include <QSharedPointer>

class FrameDevice;

/**
 * @brief 缩放控制器，负责管理地图的缩放功能
 */
class ZoomController : public QObject
{
    Q_OBJECT

public:
    explicit ZoomController(QObject* parent = nullptr);
    ~ZoomController();

    // 缩放操作
    void zoomIn();
    void zoomOut();
    void setZoomLevel(int level);
    int zoomLevel() const { return m_zoomLevel; }
    
    // 获取当前缩放比例
    double scaleFactor() const;
    
    // 保存控件基础几何信息
    void saveBaseGeometry(const QMap<int, QSharedPointer<FrameDevice>>& devices);
    
    // 应用缩放到所有控件
    void applyZoom(const QMap<int, QSharedPointer<FrameDevice>>& devices, const QPoint& scrollOffset);
    
    // 清除基础几何信息
    void clearBaseGeometry();
    
    // 保存和获取基础内容尺寸
    void setBaseContentSize(const QSize& size);
    QSize getScaledContentSize() const;
    
    // 基础几何信息结构
    struct BaseGeometry {
        QPointF pos;
        QSizeF size;
    };
    
    // 获取基础几何信息（用于性能优化的滚动）
    const QMap<int, BaseGeometry>& baseDeviceGeometry() const { return m_baseDeviceGeometry; }

signals:
    void zoomLevelChanged(int level);
    void scaleFactorChanged(double factor);

private:
    int m_zoomLevel;
    QSize m_baseContentSize;
    QMap<int, BaseGeometry> m_baseDeviceGeometry;
};

#endif // ZOOMCONTROLLER_H
