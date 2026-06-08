#include "cranemapwidget.h"
#include "setlevelgraphbuilder.h"
#include "fouplevelgraphbuilder.h"
#include <QDebug>
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QStyle>
#include <QtMath>
#include <QTimer>
#include <QHash>
#include <QtConcurrent>
#include <QCoreApplication>
#include <limits>

namespace {

enum HorizontalDirection {
    RightDirection,
    LeftDirection
};

void drawArcCurve(QPainter &painter,
                  const QPointF &startPoint,
                  const QPointF &endPoint,
                  HorizontalDirection direction,
                  qreal radius,
                  qreal angleDegrees)
{
    if (radius <= 0.0 || angleDegrees <= 0.0 || angleDegrees >= 180.0) {
        painter.drawLine(startPoint, endPoint);
        return;
    }

    if (!qFuzzyCompare(startPoint.x() + 1.0, endPoint.x() + 1.0) || startPoint.y() >= endPoint.y()) {
        painter.drawLine(startPoint, endPoint);
        return;
    }

    const qreal angleRadians = qDegreesToRadians(angleDegrees);
    const qreal horizontalOffset = radius * qSin(angleRadians);
    const qreal verticalOffset = radius * (1.0 - qCos(angleRadians));
    const qreal directionSign = direction == RightDirection ? 1.0 : -1.0;
    const qreal totalVerticalDistance = endPoint.y() - startPoint.y();
    const qreal straightSegmentLength = totalVerticalDistance - 2.0 * verticalOffset;

    if (straightSegmentLength < 0.0) {
        painter.drawLine(startPoint, endPoint);
        return;
    }

    QPainterPath path(startPoint);

    const QRectF firstArcRect(startPoint.x() - radius,
                              startPoint.y(),
                              radius * 2.0,
                              radius * 2.0);
    const qreal firstStartAngle = 90.0;
    const qreal firstSweepAngle = direction == RightDirection ? -angleDegrees : angleDegrees;
    path.arcTo(firstArcRect, firstStartAngle, firstSweepAngle);

    const QPointF firstArcEnd(startPoint.x() + directionSign * horizontalOffset,
                              startPoint.y() + verticalOffset);
    const QPointF secondArcStart(firstArcEnd.x(), firstArcEnd.y() + straightSegmentLength);
    path.lineTo(secondArcStart);

    const QRectF secondArcRect(endPoint.x() - radius,
                               endPoint.y() - 2.0 * radius,
                               radius * 2.0,
                               radius * 2.0);
    const qreal secondStartAngle = direction == RightDirection ? angleDegrees - 90.0 : 270.0 - angleDegrees;
    const qreal secondSweepAngle = direction == RightDirection ? -angleDegrees : angleDegrees;
    path.arcTo(secondArcRect, secondStartAngle, secondSweepAngle);

    painter.drawPath(path);
}

}

CraneMapWidget::CraneMapWidget(QWidget *parent)
    : QWidget(parent),
      m_graphBuilder(nullptr),
      m_foupLevelBuilder(nullptr),
      m_contentSize(2000, 1200),
      m_isDragging(false),
      m_logger(*LoggerManager::getInstance()),
      m_loggerFileName("debug.log")
{
    m_fixSpacingValue = 6;
    m_frameDeviceTrackGap = 8;
    
    // 创建控制器
    m_zoomController = new ZoomController(this);
    m_scrollController = new ScrollController(this, this);
    
    // 创建 FrameDevice 控件池
    m_devicePool = new FrameDevicePool(this);
    
    // 连接滚动控制器信号
    connect(m_scrollController, &ScrollController::scrollOffsetChanged, this, [this](const QPoint& /*newOffset*/) {
        // 只同步移动控件，不重新计算缩放（性能优化）
        syncDevicePositions();
        update();
    });
    
    // 连接缩放控制器信号
    connect(m_zoomController, &ZoomController::zoomLevelChanged, this, [this]() {
        applyZoom();
        updateScrollBars();
        update();
    });
    
    // 创建 SetLevel 图构建器（传入控件池）
    m_graphBuilder = new Graph::SetLevelGraphBuilder(m_graph, m_frameDeviceMap, m_drawCommands, this, m_devicePool);
    m_graphBuilder->setFrameDeviceTrackGap(m_frameDeviceTrackGap);
    
    // 创建 FoupLevel 图构建器（传入控件池）
    m_foupLevelBuilder = new Graph::FoupLevelGraphBuilder(m_graph, m_frameDeviceMap, m_drawCommands, this, m_devicePool);
    m_foupLevelBuilder->setFrameDeviceTrackGap(m_frameDeviceTrackGap);
    m_foupLevelBuilder->setFoupDeviceWidth(m_foupDeviceWidth);
    m_foupLevelBuilder->setFoupSpacing(m_foupSpacing);
    
    // 创建异步加载监视器
    m_configLoadWatcher = new QFutureWatcher<bool>(this);
    connect(m_configLoadWatcher, &QFutureWatcher<bool>::finished,
            this, &CraneMapWidget::onConfigLoadFinished);
    
    // 创建设备数据刷新定时器
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &CraneMapWidget::refreshDeviceData);
    m_refreshTimer->start(1000);  // 每1秒刷新一次
    
    // 设置鼠标追踪
    setMouseTracking(true);

    // 设置背景透明（不使用WA_TranslucentBackground，避免传播到子控件）
    setAutoFillBackground(false);
}

CraneMapWidget::~CraneMapWidget()
{
    // 必须在 ~QWidget() 之前清除所有 QSharedPointer，
    // 避免 Qt 父子机制和 QSharedPointer 双重删除 FrameDevice
    // 顺序：先清 widget 本地引用，再清池中引用
    m_frameDeviceMap.clear();
    
    if (m_devicePool) {
        m_devicePool->clearAll();
    }
    
    delete m_graphBuilder;
    delete m_foupLevelBuilder;
}

bool CraneMapWidget::loadConfig(const QString& configFilePath)
{
    // 如果已有异步任务在运行，先取消
    if (m_configLoadWatcher->isRunning()) {
        m_configLoadWatcher->cancel();
        m_configLoadWatcher->waitForFinished();
    }
    
    // 保存配置文件路径
    m_pendingConfigPath = configFilePath;
    
    // 发射加载开始信号
    emit configLoadStarted();
    emit configLoadProgress(0, "开始加载配置文件...");
    
    // 使用 QtConcurrent 异步执行加载任务
    QFuture<bool> future = QtConcurrent::run([this, configFilePath]() -> bool {
        // 1. 创建配置解析器（在工作线程中）
        Graph::GraphConfigParser* parser = new Graph::GraphConfigParser(configFilePath);
        parser->setFixSpacingValue(m_fixSpacingValue);
        
        // 2. 解析配置文件
        if (!parser->parse()) {
            delete parser;
            return false;
        }
        
        // 3. 获取配置数据
        Graph::GraphConfig config = parser->getConfig();
        delete parser;
        
        // 4. 在主线程中更新 m_config（通过 QMetaObject::invokeMethod）
        QMetaObject::invokeMethod(this, [this, config]() {
            m_config = config;
            emit configLoadProgress(50, "配置解析完成，开始构建图...");
        }, Qt::QueuedConnection);
        
        // 5. 根据当前地图级别选择构建器（在主线程中执行）
        bool buildResult = false;
        QMetaObject::invokeMethod(this, [this, &buildResult]() {
            switch (m_currentMapLevel) {
            case MapLevel::SetLevel:
                m_graphBuilder->setFrameDeviceTrackGap(m_frameDeviceTrackGap);
                buildResult = m_graphBuilder->buildGraph(m_config);
                break;
            case MapLevel::FoupLevel:
                m_foupLevelBuilder->setFrameDeviceTrackGap(m_frameDeviceTrackGap);
                m_foupLevelBuilder->setFoupDeviceWidth(m_foupDeviceWidth);
                m_foupLevelBuilder->setFoupSpacing(m_foupSpacing);
                buildResult = m_foupLevelBuilder->buildGraph(m_config);
                break;
            case MapLevel::BayLevel:
                // TODO: BayLevel 视图构建器
                break;
            }
            emit configLoadProgress(90, "图构建完成...");
        }, Qt::BlockingQueuedConnection);
        
        return buildResult;
    });
    
    // 设置 Future 到 Watcher
    m_configLoadWatcher->setFuture(future);
    
    // 立即返回 true，实际结果在 onConfigLoadFinished 中处理
    return true;
}

void CraneMapWidget::setFixSpacingValue(double value)
{
    if (value >= 0) {
        m_fixSpacingValue = value;
    }
}

void CraneMapWidget::setFrameDeviceTrackGap(double value)
{
    if (value >= 0.0) {
        m_frameDeviceTrackGap = value;
        if (m_graphBuilder) {
            m_graphBuilder->setFrameDeviceTrackGap(value);
        }
        if (m_foupLevelBuilder) {
            m_foupLevelBuilder->setFrameDeviceTrackGap(value);
        }
    }
}

void CraneMapWidget::setFrameDeviceWidth(int width)
{
    if (width > 0 && m_graphBuilder) {
        m_graphBuilder->setFrameDeviceWidth(width);
    }
}

void CraneMapWidget::setFoupDeviceWidth(int width)
{
    if (width > 0) {
        m_foupDeviceWidth = width;
        if (m_foupLevelBuilder) {
            m_foupLevelBuilder->setFoupDeviceWidth(width);
        }
    }
}

void CraneMapWidget::setFoupSpacing(int spacing)
{
    if (spacing >= 0) {
        m_foupSpacing = spacing;
        if (m_foupLevelBuilder) {
            m_foupLevelBuilder->setFoupSpacing(spacing);
        }
    }
}

void CraneMapWidget::switchMapLevel(MapLevel level)
{
    qDebug() << "[DIAG] switchMapLevel ENTER, level=" << (int)level;
    
    if (level == m_currentMapLevel)
        return;
    
    // 清除选中状态
    m_selectedDeviceKey = -1;
    
    m_currentMapLevel = level;
    
    // 使用控件池切换视图（复用控件，不重新创建）
    qDebug() << "[DIAG] switchMapLevel: switching to level" << (int)level << "using device pool";
    
    // 将 MapLevel 转换为 FrameDevicePool::MapLevel
    FrameDevicePool::MapLevel poolLevel;
    switch (level) {
        case MapLevel::SetLevel:
            poolLevel = FrameDevicePool::SetLevel;
            break;
        case MapLevel::FoupLevel:
            poolLevel = FrameDevicePool::FoupLevel;
            break;
        case MapLevel::BayLevel:
            poolLevel = FrameDevicePool::BayLevel;
            break;
    }
    
    // 切换到目标层级（自动隐藏其他层级，显示目标层级）
    m_devicePool->switchToLevel(poolLevel);
    
    // 获取当前层级的设备映射表引用
    m_frameDeviceMap = m_devicePool->getDeviceMap(poolLevel);
    
    qDebug() << "[DIAG] switchMapLevel: current level has" << m_frameDeviceMap.size() << "devices";
    
    // 清除缩放控制器的基础几何信息
    m_zoomController->clearBaseGeometry();
    
    // 清除绘制指令和图数据
    m_drawCommands.clear();
    m_graph.clear();
    
    // 重置缩放级别
    m_zoomController->setZoomLevel(0);
    
    // 根据新的级别重建视图
    if (m_config.nodes.isEmpty())
        return;
    
    qDebug() << "[DIAG] switchMapLevel: building graph for level" << (int)level;
    switch (level) {
    case MapLevel::SetLevel:
        if (m_graphBuilder) {
            m_graphBuilder->setFrameDeviceTrackGap(m_frameDeviceTrackGap);
            m_graphBuilder->buildGraph(m_config);
        }
        break;
    case MapLevel::FoupLevel:
        if (m_foupLevelBuilder) {
            m_foupLevelBuilder->setFrameDeviceTrackGap(m_frameDeviceTrackGap);
            m_foupLevelBuilder->setFoupDeviceWidth(m_foupDeviceWidth);
            m_foupLevelBuilder->setFoupSpacing(m_foupSpacing);
            m_foupLevelBuilder->buildGraph(m_config);
        }
        break;
    case MapLevel::BayLevel:
        // TODO: BayLevel 视图构建器
        break;
    }
    qDebug() << "[DIAG] switchMapLevel: graph built, devices=" << m_frameDeviceMap.size();
    
    // 重新居中并保存基准几何
    qDebug() << "[DIAG] switchMapLevel: centerMap";
    centerMap();
    
    // 连接 FrameDevice 双击信号
    qDebug() << "[DIAG] switchMapLevel: connectFrameDeviceSignals";
    connectFrameDeviceSignals();
    
    qDebug() << "[DIAG] switchMapLevel: calling update";
    update();
    qDebug() << "[DIAG] switchMapLevel EXIT";
}

void CraneMapWidget::setStartNodePosition(const QPointF& position)
{
    if (m_graphBuilder) {
        m_graphBuilder->setStartNodePosition(position);
    }
    if (m_foupLevelBuilder) {
        m_foupLevelBuilder->setStartNodePosition(position);
    }
}

QSize CraneMapWidget::sizeHint() const
{
    return m_contentSize;
}

QRectF CraneMapWidget::calculateMapBounds() const
{
    if (m_drawCommands.isEmpty() && m_frameDeviceMap.isEmpty()) {
        return QRectF(0, 0, 100, 100);
    }
    
    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::lowest();
    qreal maxY = std::numeric_limits<qreal>::lowest();
    
    // 遍历所有绘制指令，找出边界
    for (const auto& cmd : m_drawCommands) {
        minX = qMin(minX, qMin(cmd.from.x(), cmd.to.x()));
        minY = qMin(minY, qMin(cmd.from.y(), cmd.to.y()));
        maxX = qMax(maxX, qMax(cmd.from.x(), cmd.to.x()));
        maxY = qMax(maxY, qMax(cmd.from.y(), cmd.to.y()));
        
        // 对于曲线，也考虑控制点
        if (cmd.type == Graph::DrawCommand::SCurve || cmd.type == Graph::DrawCommand::ReverseSCurve) {
            minX = qMin(minX, qMin(cmd.control1.x(), cmd.control2.x()));
            minY = qMin(minY, qMin(cmd.control1.y(), cmd.control2.y()));
            maxX = qMax(maxX, qMax(cmd.control1.x(), cmd.control2.x()));
            maxY = qMax(maxY, qMax(cmd.control1.y(), cmd.control2.y()));
        }
    }
    
    // 遍历所有FrameDevice控件，扩展边界
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        QSharedPointer<FrameDevice> device = it.value();
        if (device) {
            QRect deviceRect = device->geometry();
            minX = qMin(minX, (qreal)deviceRect.left());
            minY = qMin(minY, (qreal)deviceRect.top());
            maxX = qMax(maxX, (qreal)deviceRect.right());
            maxY = qMax(maxY, (qreal)deviceRect.bottom());
        }
    }
    
    return QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
}

void CraneMapWidget::centerMap(int marginTop, int marginBottom, int marginLeft, int marginRight)
{
    if (m_drawCommands.isEmpty()) {
        return;
    }
    
    // 计算当前地图的边界
    QRectF bounds = calculateMapBounds();
    
    // 内容实际宽高（含边距）
    qreal contentWidth = bounds.width() + marginLeft + marginRight;
    qreal contentHeight = bounds.height() + marginTop + marginBottom;
    
    // 获取可用空间：取自身尺寸与父控件尺寸中较大的值
    QSize selfSize = size();
    QSize parentSize = parentWidget() ? parentWidget()->size() : selfSize;
    QSize availableSize(qMax(selfSize.width(), parentSize.width()),
                        qMax(selfSize.height(), parentSize.height()));
    
    qDebug() << "[CraneMapWidget::centerMap] bounds:" << bounds
             << "contentSize:" << contentWidth << "x" << contentHeight
             << "selfSize:" << selfSize
             << "parentSize:" << parentSize
             << "availableSize:" << availableSize;
    
    // 基础偏移：将内容左上角对齐到边距位置
    qreal offsetX = marginLeft - bounds.left();
    qreal offsetY = marginTop - bounds.top();
    
    // 如果内容比可用空间小，增加额外偏移量实现居中
    if (contentWidth < availableSize.width()) {
        offsetX += (availableSize.width() - contentWidth) / 2.0;
    }
    if (contentHeight < availableSize.height()) {
        offsetY += (availableSize.height() - contentHeight) / 2.0;
    }
    
    // 计算需要通过滚动来居中的偏移量（内容 > 可视区域时）
    int scrollToX = 0;
    int scrollToY = 0;
    if (contentWidth >= availableSize.width()) {
        scrollToX = (contentWidth - availableSize.width()) / 2.0;
    }
    if (contentHeight >= availableSize.height()) {
        scrollToY = (contentHeight - availableSize.height()) / 2.0;
    }
    
    qDebug() << "[CraneMapWidget::centerMap] offsetX:" << offsetX << "offsetY:" << offsetY
             << "scrollToX:" << scrollToX << "scrollToY:" << scrollToY;
    
    // 更新所有绘制指令的坐标
    for (auto& cmd : m_drawCommands) {
        cmd.from.setX(cmd.from.x() + offsetX);
        cmd.from.setY(cmd.from.y() + offsetY);
        cmd.to.setX(cmd.to.x() + offsetX);
        cmd.to.setY(cmd.to.y() + offsetY);
        
        if (cmd.type == Graph::DrawCommand::SCurve || cmd.type == Graph::DrawCommand::ReverseSCurve) {
            cmd.control1.setX(cmd.control1.x() + offsetX);
            cmd.control1.setY(cmd.control1.y() + offsetY);
            cmd.control2.setX(cmd.control2.x() + offsetX);
            cmd.control2.setY(cmd.control2.y() + offsetY);
        }
    }
    
    // 移动所有FrameDevice控件
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        QSharedPointer<FrameDevice> device = it.value();
        if (device) {
            QPoint currentPos = device->pos();
            device->move(currentPos.x() + offsetX, currentPos.y() + offsetY);
        }
    }
    
    // 内容尺寸取内容区域和可用空间的较大值，确保控件填满父区域
    m_contentSize = QSize(qMax((int)contentWidth, availableSize.width()),
                          qMax((int)contentHeight, availableSize.height()));
    updateGeometry();
    
    // 保存基准几何信息（用于缩放）
    saveBaseGeometry();
    
    // 更新滚动条范围，然后设置初始滚动位置居中
    updateScrollBars();
    if (scrollToX > 0 || scrollToY > 0) {
        QScrollBar* hBar = m_scrollController->horizontalScrollBar();
        QScrollBar* vBar = m_scrollController->verticalScrollBar();
        if (hBar && scrollToX > 0) hBar->setValue(scrollToX);
        if (vBar && scrollToY > 0) vBar->setValue(scrollToY);
    }
    
    update();
}

void CraneMapWidget::saveBaseGeometry()
{
    m_zoomController->setBaseContentSize(m_contentSize);
    m_zoomController->saveBaseGeometry(m_frameDeviceMap);
}

void CraneMapWidget::zoomIn()
{
    m_zoomController->zoomIn();
}

void CraneMapWidget::zoomOut()
{
    m_zoomController->zoomOut();
}

void CraneMapWidget::setZoomLevel(int level)
{
    m_zoomController->setZoomLevel(level);
}

int CraneMapWidget::zoomLevel() const
{
    return m_zoomController->zoomLevel();
}

void CraneMapWidget::syncDevicePositions()
{
    // 高效地同步移动所有控件位置（只根据滚动偏移调整，不重新计算缩放）
    // 这个方法只在滚动时调用，避免重复计算提升性能
    
    // 安全检查：如果没有设备或基础几何信息为空，直接返回
    if (m_frameDeviceMap.isEmpty()) {
        return;
    }
    
    const auto& baseGeometry = m_zoomController->baseDeviceGeometry();
    if (baseGeometry.isEmpty()) {
        return;
    }
    
    QPoint scrollOffset = m_scrollController->scrollOffset();
    double scale = m_zoomController->scaleFactor();
    
    // 只更新控件位置，不改变尺寸
    for (auto it = m_frameDeviceMap.constBegin(); it != m_frameDeviceMap.constEnd(); ++it) {
        int deviceKey = it.key();
        QSharedPointer<FrameDevice> device = it.value();
        
        if (!device || !baseGeometry.contains(deviceKey)) {
            continue;
        }
        
        const auto& baseGeom = baseGeometry[deviceKey];
        
        // 只更新位置（减去滚动偏移 + 居中偏移）
        device->move(baseGeom.pos.x() * scale - scrollOffset.x() + m_centerOffset.x(), 
                     baseGeom.pos.y() * scale - scrollOffset.y() + m_centerOffset.y());
    }
}

void CraneMapWidget::applyZoom()
{
    // 获取当前滚动偏移
    QPoint scrollOffset = m_scrollController->scrollOffset();
    
    // 应用缩放到所有控件（传入滚动偏移）
    m_zoomController->applyZoom(m_frameDeviceMap, scrollOffset);
    
    // 更新控件的缩放级别（用于字体缩放）
    int zLevel = m_zoomController->zoomLevel();
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        if (it.value()) {
            it.value()->setZoomLevel(zLevel);
        }
    }
    
    // 更新内容尺寸
    QSize scaledSize = m_zoomController->getScaledContentSize();
    m_contentSize = scaledSize;
    updateGeometry();
    
    // 更新滚动条
    updateScrollBars();
    
    // 动态计算居中偏移并应用到设备控件
    updateCenterOffset();
    
    update();
}

void CraneMapWidget::updateCenterOffset()
{
    QSize scaledContent = m_zoomController->getScaledContentSize();
    QSize viewport = size();
    
    qreal cx = scaledContent.width() < viewport.width()
               ? (viewport.width() - scaledContent.width()) / 2.0 : 0;
    qreal cy = scaledContent.height() < viewport.height()
               ? (viewport.height() - scaledContent.height()) / 2.0 : 0;
    m_centerOffset = QPointF(cx, cy);
    
    // 将居中偏移应用到所有设备控件位置
    if (cx > 0 || cy > 0) {
        const auto& baseGeometry = m_zoomController->baseDeviceGeometry();
        QPoint scrollOffset = m_scrollController->scrollOffset();
        double scale = m_zoomController->scaleFactor();
        
        for (auto it = m_frameDeviceMap.constBegin(); it != m_frameDeviceMap.constEnd(); ++it) {
            int deviceKey = it.key();
            QSharedPointer<FrameDevice> device = it.value();
            if (!device || !baseGeometry.contains(deviceKey)) continue;
            
            const auto& baseGeom = baseGeometry[deviceKey];
            device->move(baseGeom.pos.x() * scale - scrollOffset.x() + cx,
                         baseGeom.pos.y() * scale - scrollOffset.y() + cy);
        }
    }
}

void CraneMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_lastMousePos = event->globalPos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void CraneMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        QPoint delta = event->globalPos() - m_lastMousePos;
        m_lastMousePos = event->globalPos();
        
        // 使用滚动控制器的滚动条进行滚动
        QScrollBar* hBar = m_scrollController->horizontalScrollBar();
        QScrollBar* vBar = m_scrollController->verticalScrollBar();
        
        if (hBar->isVisible()) {
            hBar->setValue(hBar->value() - delta.x());
        }
        if (vBar->isVisible()) {
            vBar->setValue(vBar->value() - delta.y());
        }
        
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void CraneMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void CraneMapWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    qDebug() << "[DIAG] paintEvent ENTER";

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 应用滚动偏移 + 居中偏移
    QPoint offset = m_scrollController->scrollOffset();
    painter.translate(-offset.x() + m_centerOffset.x(), -offset.y() + m_centerOffset.y());
    
    // 应用缩放
    double scale = m_zoomController->scaleFactor();
    painter.scale(scale, scale);
    
    // 设置画笔样式
    QPen pen(Qt::white, 2, Qt::SolidLine);
    painter.setPen(pen);
    
    // 遍历绘制指令
    for (const auto& cmd : m_drawCommands) {
        switch (cmd.type) {
        case Graph::DrawCommand::Line:
            painter.drawLine(cmd.from, cmd.to);
            break;
            
        case Graph::DrawCommand::SCurve:
        case Graph::DrawCommand::ReverseSCurve:
            {
                QPainterPath path;
                path.moveTo(cmd.from);
                path.cubicTo(cmd.control1, cmd.control2, cmd.to);
                painter.drawPath(path);
            }
            break;
            
        case Graph::DrawCommand::LeftArc:
        case Graph::DrawCommand::RightArc:
        case Graph::DrawCommand::LeftArc2:
        case Graph::DrawCommand::RightArc2:
            {
                if (cmd.type == Graph::DrawCommand::LeftArc) {
                    drawArcCurve(painter, cmd.from, cmd.to, LeftDirection, 10.0, 90.0);
                } else if (cmd.type == Graph::DrawCommand::RightArc) {
                    drawArcCurve(painter, cmd.from, cmd.to, RightDirection, 10.0, 90.0);
                } else if (cmd.type == Graph::DrawCommand::LeftArc2) {
                    drawArcCurve(painter, cmd.from, cmd.to, LeftDirection, 8.0, 90.0);
                } else {
                    drawArcCurve(painter, cmd.from, cmd.to, RightDirection, 8.0, 90.0);
                }
            }
            break;
        }
    }
    qDebug() << "[DIAG] paintEvent EXIT";
}

void CraneMapWidget::refreshDeviceData()
{
    std::string loggerPre = "[CraneMapWidget::refreshDeviceData]";
    std::string mapLevelStr = (m_currentMapLevel == MapLevel::FoupLevel) ? "FoupLevel" : "SetLevel";
    
    // 安全检查：如果设备映射为空，直接返回
    if (m_frameDeviceMap.isEmpty()) {
        m_logger.log(m_loggerFileName, Level::WARN, loggerPre + "No devices in map, returning");
        return;
    }

    int updatedCount = 0;

    // 刷新所有 FrameDevice 控件的显示
    int skippedNull = 0;
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        int key = it.key();
        QSharedPointer<FrameDevice> frameDevice = it.value();

        // 严格的空指针检查
        if (!frameDevice || frameDevice.isNull()) {
            skippedNull++;
            m_logger.log(m_loggerFileName, Level::WARN, loggerPre + "Skipped null device, key=" + std::to_string(key));
            continue;
        }

        // 根据当前地图级别调用相应的更新方法
        try {
            if (m_currentMapLevel == MapLevel::FoupLevel) {
                // FoupLevel 视图：更新 Foup 数据
                frameDevice->updateFoupInfo();
                updatedCount++;
            } else if (m_currentMapLevel == MapLevel::SetLevel) {
                // SetLevel 视图：更新 Set 数据
                frameDevice->updateSetInfo();
                updatedCount++;
            }
        } catch (const std::exception& e) {
            m_logger.log(m_loggerFileName, Level::ERROR, loggerPre + "Exception at key=" + std::to_string(key) + ", error=" + e.what());
        } catch (...) {
            m_logger.log(m_loggerFileName, Level::ERROR, loggerPre + "Unknown exception at key=" + std::to_string(key));
        }
    }
    if (skippedNull > 0) {
        m_logger.log(m_loggerFileName, Level::WARN, loggerPre + "Skipped " + std::to_string(skippedNull) + " null devices total");
    }

    m_logger.log(m_loggerFileName, Level::INFO, loggerPre + "Updated " + std::to_string(updatedCount) + " devices");
}

void CraneMapWidget::selectFirstSet()
{
    // 遍历设备映射，找到第一个 Set 设备
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        QSharedPointer<FrameDevice> device = it.value();
        if (device && device->getDeviceType() == FrameDevice::DeviceType::Set) {
            auto setInfo = device->getSetOfOHBInfo();
            if (setInfo) {
                // 触发设备点击信号来选中第一个 Set
                emit device->deviceClicked(FrameDevice::DeviceType::Set, setInfo->getUiId());
                break;
            }
        }
    }
}

void CraneMapWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScrollBars();
    updateCenterOffset();
}

void CraneMapWidget::updateScrollBars()
{
    qDebug() << "[DIAG] updateScrollBars ENTER";
    
    // 使用缩放后的内容尺寸
    QSize scaledSize = m_zoomController->getScaledContentSize();
    
    // 委托给 ScrollController 更新滚动条
    m_scrollController->updateScrollBars(scaledSize, size());
    
    // 调整滚动条位置
    int scrollBarExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    QScrollBar* hBar = m_scrollController->horizontalScrollBar();
    QScrollBar* vBar = m_scrollController->verticalScrollBar();
    
    if (hBar->isVisible()) {
        int hBarWidth = width() - (vBar->isVisible() ? scrollBarExtent : 0);
        hBar->setGeometry(0, height() - scrollBarExtent, hBarWidth, scrollBarExtent);
    }
    
    if (vBar->isVisible()) {
        int vBarHeight = height() - (hBar->isVisible() ? scrollBarExtent : 0);
        vBar->setGeometry(width() - scrollBarExtent, 0, scrollBarExtent, vBarHeight);
    }
    
    qDebug() << "[DIAG] updateScrollBars EXIT";
}

void CraneMapWidget::wheelEvent(QWheelEvent *event)
{
    // 使用 Ctrl + 滚轮进行缩放
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        // 普通滚轮滚动
        QScrollBar* vBar = m_scrollController->verticalScrollBar();
        if (vBar->isVisible()) {
            int delta = -event->angleDelta().y() / 8;  // 转换为像素
            vBar->setValue(vBar->value() + delta);
        }
        event->accept();
    }
}

void CraneMapWidget::connectFrameDeviceSignals()
{
    int setCount = 0;
    int totalCount = 0;
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        QSharedPointer<FrameDevice> device = it.value();
        if (device) {
            // 先断开旧连接，避免信号累积（控件池复用时同一控件会被多次连接）
            disconnect(device.data(), &FrameDevice::deviceClicked,
                       this, &CraneMapWidget::handleDeviceClicked);
            
            // 连接所有设备的单击信号
            connect(device.data(), &FrameDevice::deviceClicked,
                    this, &CraneMapWidget::handleDeviceClicked);
            totalCount++;
            
            // 只连接 Set 设备的双击信号
            if (device->getDeviceType() == FrameDevice::DeviceType::Set) {
                disconnect(device.data(), &FrameDevice::setDeviceDoubleClicked,
                           this, &CraneMapWidget::handleSetDeviceDoubleClicked);
                connect(device.data(), &FrameDevice::setDeviceDoubleClicked,
                        this, &CraneMapWidget::handleSetDeviceDoubleClicked,
                        Qt::QueuedConnection);
                setCount++;
            }
        }
    }
    qDebug() << "[DIAG] connectFrameDeviceSignals: connected" << totalCount << "total devices," << setCount << "Set devices";
}

void CraneMapWidget::handleDeviceClicked(FrameDevice::DeviceType deviceType, int uiId)
{
    // 清除所有设备的选中状态
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        if (it.value()) {
            it.value()->setSelected(false);
        }
    }
    
    // 查找并设置新的选中状态
    int newSelectedKey = -1;
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        QSharedPointer<FrameDevice> device = it.value();
        if (!device) continue;
        
        int deviceUiId = -1;
        if (deviceType == FrameDevice::DeviceType::Set && device->getDeviceType() == FrameDevice::DeviceType::Set) {
            deviceUiId = device->getSetOfOHBInfo() ? device->getSetOfOHBInfo()->getUiId() : -1;
        } else if (deviceType == FrameDevice::DeviceType::Foup && device->getDeviceType() == FrameDevice::DeviceType::Foup) {
            // Foup 使用 qrCode 的哈希值作为 uiId
            auto foupInfo = device->getFoupOfOHBInfo();
            deviceUiId = foupInfo ? qHash(foupInfo->qrCode()) : -1;
        }
        
        if (deviceUiId == uiId) {
            device->setSelected(true);
            newSelectedKey = it.key();
            
            // 发射设备选中信号，携带设备数据
            QSharedPointer<SetOfOHBInfo> setInfo = device->getSetOfOHBInfo();
            QSharedPointer<FoupOfOHBInfo> foupInfo = device->getFoupOfOHBInfo();
            emit deviceSelected(deviceType, setInfo, foupInfo);
            break;
        }
    }
    
    m_selectedDeviceKey = newSelectedKey;
}

void CraneMapWidget::handleSetDeviceDoubleClicked(int uiId, const QString& firstFoupQrCode)
{
    std::string loggerPre = "[CraneMapWidget::handleSetDeviceDoubleClicked]";
    m_logger.log(m_loggerFileName, Level::INFO, loggerPre + "Set设备双击: uiId=" + std::to_string(uiId) + ", firstFoupQrCode=" + firstFoupQrCode.toStdString());
    
    // 重置拖动状态，避免切换视图后鼠标仍处于拖动模式
    m_isDragging = false;
    setCursor(Qt::ArrowCursor);
    
    // 切换到 FoupLevel（如果当前不是）
    if (m_currentMapLevel != MapLevel::FoupLevel) {
        m_currentMapLevel = MapLevel::SetLevel;
        switchMapLevel(MapLevel::FoupLevel);
        
        resize(sizeHint());
        // 使用 setZoomLevel 设置固定缩放级别，避免累积缩放
        setZoomLevel(-4);
    }
    
    // 清除所有设备的选中状态
    for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
        if (it.value()) {
            it.value()->setSelected(false);
        }
    }
    
    // 查找目标节点的 nodeKey，高亮该 Set 下所有 Foup
    int targetDeviceKey = -1;
    int targetNodeKey = -1;
    for (auto nodeIt = m_config.nodes.begin(); nodeIt != m_config.nodes.end(); ++nodeIt) {
        if (nodeIt.value() && nodeIt.value()->getId() == uiId) {
            targetNodeKey = nodeIt.key();
            targetDeviceKey = targetNodeKey * 100;
            break;
        }
    }
    
    if (targetDeviceKey >= 0) {
        m_logger.log(m_loggerFileName, Level::INFO, loggerPre + "找到目标设备键: " + std::to_string(targetDeviceKey));
        
        // 高亮该 Set 下所有 Foup 控件（键格式：nodeKey * 100 + foupIndex）
        for (auto it = m_frameDeviceMap.begin(); it != m_frameDeviceMap.end(); ++it) {
            int key = it.key();
            if (key / 100 == targetNodeKey && it.value()) {
                it.value()->setSelected(true);
            }
        }
        
        // 延迟到下一轮事件循环，等 UI 完全渲染后再滚动
        QTimer::singleShot(0, this, [this, targetDeviceKey]() {
            scrollToDevice(targetDeviceKey, 10);
        });
    } else {
        m_logger.log(m_loggerFileName, Level::WARN, loggerPre + "未找到 uiId=" + std::to_string(uiId) + " 对应的 Foup 设备");
    }
}

void CraneMapWidget::scrollToDevice(int deviceKey, int offsetPixels)
{
    std::string loggerPre = "[CraneMapWidget::scrollToDevice()]";
    
    QSharedPointer<FrameDevice> device = m_frameDeviceMap.value(deviceKey);
    if (!device) {
        return;
    }
    
    qDebug() << "[DIAG] scrollToDevice: key=" << deviceKey
             << "devicePos=(" << device->pos().x() << "," << device->pos().y() << ")";
    
    m_logger.log(m_loggerFileName, Level::INFO, loggerPre + "滚动到设备: key=" + std::to_string(deviceKey));
    
    // 委托给 ScrollController 处理滚动
    m_scrollController->scrollToDevice(device, size(), offsetPixels);
}

void CraneMapWidget::onConfigLoadFinished()
{
    // 获取异步任务的结果
    bool success = m_configLoadWatcher->result();
    
    if (success) {
        // 居中布局（计算偏移并保存基准几何信息）
        centerMap();
        
        // 连接 FrameDevice 信号
        connectFrameDeviceSignals();
        
        // 更新滚动条
        updateScrollBars();
        
        // 触发重绘
        update();
        
        emit configLoadProgress(100, "配置加载完成");
        emit configLoadFinished(true);
        
        qDebug() << "[CraneMapWidget] Config loaded successfully from:" << m_pendingConfigPath;
    } else {
        emit configLoadFinished(false);
        qWarning() << "[CraneMapWidget] Failed to load config from:" << m_pendingConfigPath;
    }
}
