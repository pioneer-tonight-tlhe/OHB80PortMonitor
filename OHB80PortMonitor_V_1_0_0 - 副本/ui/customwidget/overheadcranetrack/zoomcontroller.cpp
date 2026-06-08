#include "zoomcontroller.h"
#include "framedevice.h"
#include <QtMath>
#include <QDebug>

ZoomController::ZoomController(QObject* parent)
    : QObject(parent)
    , m_zoomLevel(0)
{
}

ZoomController::~ZoomController()
{
}

void ZoomController::zoomIn()
{
    if (m_zoomLevel < 5) {
        m_zoomLevel++;
        emit zoomLevelChanged(m_zoomLevel);
        emit scaleFactorChanged(scaleFactor());
    }
}

void ZoomController::zoomOut()
{
    if (m_zoomLevel > -13) {
        m_zoomLevel--;
        emit zoomLevelChanged(m_zoomLevel);
        emit scaleFactorChanged(scaleFactor());
    }
}

void ZoomController::setZoomLevel(int level)
{
    if (level >= -13 && level <= 5 && level != m_zoomLevel) {
        m_zoomLevel = level;
        emit zoomLevelChanged(m_zoomLevel);
        emit scaleFactorChanged(scaleFactor());
    }
}

double ZoomController::scaleFactor() const
{
    return qPow(1.2, m_zoomLevel);
}

void ZoomController::saveBaseGeometry(const QMap<int, QSharedPointer<FrameDevice>>& devices)
{
    m_baseDeviceGeometry.clear();
    
    for (auto it = devices.constBegin(); it != devices.constEnd(); ++it) {
        if (it.value()) {
            BaseGeometry geom;
            geom.pos = it.value()->pos();
            geom.size = it.value()->size();
            m_baseDeviceGeometry[it.key()] = geom;
        }
    }
    
    qDebug() << "[ZoomController] Saved base geometry for" << m_baseDeviceGeometry.size() << "devices";
}

void ZoomController::applyZoom(const QMap<int, QSharedPointer<FrameDevice>>& devices, const QPoint& scrollOffset)
{
    double scale = scaleFactor();
    
    for (auto it = devices.constBegin(); it != devices.constEnd(); ++it) {
        int deviceKey = it.key();
        QSharedPointer<FrameDevice> device = it.value();
        
        if (!device || !m_baseDeviceGeometry.contains(deviceKey)) {
            continue;
        }
        
        const BaseGeometry& baseGeom = m_baseDeviceGeometry[deviceKey];
        
        // 缩放位置和尺寸（位置减去滚动偏移）
        device->move(baseGeom.pos.x() * scale - scrollOffset.x(), 
                     baseGeom.pos.y() * scale - scrollOffset.y());
        device->setFixedSize(baseGeom.size.width() * scale, baseGeom.size.height() * scale);
    }
}

void ZoomController::clearBaseGeometry()
{
    m_baseDeviceGeometry.clear();
}

void ZoomController::setBaseContentSize(const QSize& size)
{
    m_baseContentSize = size;
}

QSize ZoomController::getScaledContentSize() const
{
    double scale = scaleFactor();
    return QSize(m_baseContentSize.width() * scale, m_baseContentSize.height() * scale);
}
