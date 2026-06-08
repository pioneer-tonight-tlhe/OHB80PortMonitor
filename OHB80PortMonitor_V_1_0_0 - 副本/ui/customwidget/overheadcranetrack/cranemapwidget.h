#ifndef CRANEMAPWIDGET_H
#define CRANEMAPWIDGET_H

#include "graphadjacencymultilist.h"
#include "graphconfigparser.h"
#include "setlevelgraphbuilder.h"
#include "fouplevelgraphbuilder.h"
#include "framedevice.h"
#include "framedevicepool.h"
#include "drawcommand.h"
#include "loggermanager.h"
#include "zoomcontroller.h"
#include "scrollcontroller.h"

#include <QWidget>
#include <QSharedPointer>
#include <QString>
#include <QMap>
#include <QPointF>
#include <QPainterPath>
#include <QFuture>
#include <QFutureWatcher>
#include <QTimer>

// 图邻接多重表类型别名
using GraphMultilist = Graph::GraphAdjacencyMultilist<QSharedPointer<Graph::GraphNode>, QSharedPointer<Graph::GraphEdge>>;

class CraneMapWidget : public QWidget
{
    Q_OBJECT

public:
    // 地图级别状态枚举
    enum class MapLevel {
        FoupLevel,      // 第一级别：FOUP级别（真实环境，近距离查看FOUP）
        SetLevel,       // 第二级别：SET级别（高空视角，查看整个版图上的工作站）
        BayLevel        // 第三级别：BAY级别（宇宙视角，选择不同的区域）
    };
    
    explicit CraneMapWidget(QWidget *parent = nullptr);
    ~CraneMapWidget();
    
    // 加载配置文件并构建图
    bool loadConfig(const QString& configFilePath);

    // 设置配置中 FixSpcaing 的替换值
    void setFixSpacingValue(double value);

    // 设置设备控件到线的距离
    void setFrameDeviceTrackGap(double value);

    // 设置设备控件宽度
    void setFrameDeviceWidth(int width);
    
    // 设置 Foup 设备控件宽度
    void setFoupDeviceWidth(int width);
    
    // 设置同一 Set 内 Foup 之间的间距
    void setFoupSpacing(int spacing);
    
    // 切换地图级别并重建视图
    void switchMapLevel(MapLevel level);
    
    // 获取当前地图级别
    MapLevel currentMapLevel() const { return m_currentMapLevel; }
    
    // 设置起始节点坐标
    void setStartNodePosition(const QPointF& position);
    
    // 获取推荐的控件尺寸（用于滚动区域）
    QSize sizeHint() const override;
    
    // 自动计算地图边界并居中显示
    void centerMap(int marginTop = 50, int marginBottom = 50, int marginLeft = 50, int marginRight = 50);

    // 缩放控制
    void zoomIn();
    void zoomOut();
    void setZoomLevel(int level);
    int zoomLevel() const;

signals:
    // 设备被选中时发射，携带设备类型和对应的数据指针
    void deviceSelected(FrameDevice::DeviceType deviceType, 
                       QSharedPointer<SetOfOHBInfo> setInfo,
                       QSharedPointer<FoupOfOHBInfo> foupInfo);
    
    // 配置加载进度信号
    void configLoadStarted();
    void configLoadProgress(int percentage, const QString& message);
    void configLoadFinished(bool success);

public slots:
    // 刷新设备数据显示（由外部调用，如 homepage）
    void refreshDeviceData();

    // 选择第一个 Set 设备
    void selectFirstSet();

private slots:
    // 处理 Set 设备双击事件
    void handleSetDeviceDoubleClicked(int uiId, const QString& firstFoupQrCode);
    // 处理设备单击事件
    void handleDeviceClicked(FrameDevice::DeviceType deviceType, int uiId);
    
    // 异步加载完成后的处理
    void onConfigLoadFinished();

protected:
    // 重写绘制事件
    void paintEvent(QPaintEvent *event) override;
    
    // 重写尺寸变化事件
    void resizeEvent(QResizeEvent *event) override;
    
    // 鼠标事件处理
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    
    // 滚轮事件处理
    void wheelEvent(QWheelEvent *event) override;

private:
    // 同步设备控件位置（根据当前滚动偏移和缩放）
    void syncDevicePositions();
    
    // 图数据结构（邻接多重表）
    GraphMultilist m_graph;
    
    // SetLevel 图构建器
    Graph::SetLevelGraphBuilder* m_graphBuilder;
    
    // FoupLevel 图构建器
    Graph::FoupLevelGraphBuilder* m_foupLevelBuilder;
    
    // 配置数据
    Graph::GraphConfig m_config;

    // 配置中 FixSpcaing 的替换值，默认10
    double m_fixSpacingValue = 10.0;

    // 设备控件到线的距离，默认10
    double m_frameDeviceTrackGap = 10.0;
    
    // Foup 设备控件宽度，默认120
    int m_foupDeviceWidth = 120;
    
    // 同一 Set 内 Foup 之间的间距，默认5
    int m_foupSpacing = 5;
    
    // FrameDevice控件映射表：key=设备键，value=对应的FrameDevice控件
    QMap<int, QSharedPointer<FrameDevice>> m_frameDeviceMap;
    
    // 当前地图级别状态，默认为SET级别
    MapLevel m_currentMapLevel = MapLevel::SetLevel;
    
    // 当前选中的设备键（-1 表示无选中）
    int m_selectedDeviceKey = -1;
    
    // 绘制指令列表
    QList<Graph::DrawCommand> m_drawCommands;
    
    // 内容尺寸（用于滚动区域）
    QSize m_contentSize;
    
    // 当缩放后内容小于可视区域时的居中偏移
    QPointF m_centerOffset;
    
    // 鼠标拖动相关
    bool m_isDragging;
    QPoint m_lastMousePos;
    
    // 控制器
    ZoomController* m_zoomController;
    ScrollController* m_scrollController;
    
    // FrameDevice 控件池（负责控件复用）
    FrameDevicePool* m_devicePool;
    
    // 日志
    LoggerManager& m_logger;
    std::string m_loggerFileName;
    
    // 异步加载相关
    QFutureWatcher<bool>* m_configLoadWatcher;
    QString m_pendingConfigPath;  // 待加载的配置文件路径
    
    // 设备数据刷新定时器
    QTimer* m_refreshTimer;
    
    // 内部辅助方法
    QRectF calculateMapBounds() const;
    void connectFrameDeviceSignals();
    void scrollToDevice(int deviceKey, int offsetPixels = 10);
    void updateScrollBars();
    void saveBaseGeometry();
    void applyZoom();
    void updateCenterOffset();
};

#endif // CRANEMAPWIDGET_H
