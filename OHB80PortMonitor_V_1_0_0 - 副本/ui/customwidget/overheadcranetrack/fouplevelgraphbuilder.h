#ifndef FOUPLEVELGRAPHBUILDER_H
#define FOUPLEVELGRAPHBUILDER_H

#include "graphadjacencymultilist.h"
#include "graphconfigparser.h"
#include "loggermanager.h"
#include "drawcommand.h"
#include "app/app.h"

#include <QPointF>
#include <QString>

// 图邻接多重表类型别名
using GraphMultilist = Graph::GraphAdjacencyMultilist<QSharedPointer<Graph::GraphNode>, QSharedPointer<Graph::GraphEdge>>;

class CraneMapWidget;
class FrameDevice;
class FrameDevicePool;

namespace Graph {

// FoupLevel 视图构建器：每个 SET 节点展开为多个 Foup 级别的 FrameDevice
class FoupLevelGraphBuilder
{
public:
    enum class BuildStage {
        NotStarted = 0,           // 未开始
        MultilistBuilt = 1,       // 重建邻接多重表完成
        LayoutDrawn = 2,          // 绘制图像界面完成
        Completed = 3             // 布局图重构完毕
    };
    explicit FoupLevelGraphBuilder(GraphMultilist& graph, QMap<int, QSharedPointer<FrameDevice>>& frameDevices,
                              QList<DrawCommand>& drawCommands, CraneMapWidget* widget, FrameDevicePool* devicePool);
    ~FoupLevelGraphBuilder();

    // 设置起始节点坐标
    void setStartNodePosition(const QPointF& position);

    // 设置设备控件到线的距离
    void setFrameDeviceTrackGap(double value);

    // 设置 Foup 设备控件宽度
    void setFoupDeviceWidth(int width);

    // 设置同一 Set 内 Foup 之间的间距
    void setFoupSpacing(int spacing);

    // 使用配置构建图（构建邻接多重表 + 计算布局坐标 + 绘制控件）
    bool buildGraph(const GraphConfig& config);
    
    // 获取当前构建阶段
    BuildStage getStage() const { return m_stage; }
    
    // 将阶段枚举转换为字符串
    static const char* stageToString(BuildStage stage);

private:
    GraphMultilist& m_graph;
    CraneMapWidget* m_widget;
    QMap<int, QSharedPointer<FrameDevice>>& m_frameDevices;
    QList<Graph::DrawCommand>& m_drawCommands;
    FrameDevicePool* m_devicePool;  // 控件池引用

    // ========== 状态跟踪 ==========
    QMap<int, QPointF> m_nodePositions;              // 节点ID -> 坐标
    QSet<int> m_visitedNodes;                        // 已访问节点集合
    QSet<QPair<int,int>> m_visitedEdges;             // 已访问边集合
    double m_frameDeviceTrackGap;                    // 设备控件到线的距离
    int m_foupDeviceWidth;                           // Foup 设备控件宽度
    int m_foupSpacing;                               // 同一 Set 内 Foup 之间的间距
    int m_setSpacing;                                // 不同 Set 之间的间距（来自 config.deviceSpacing）
    double m_foupUnit;                               // FoupLevel 下一个 "set 单元" 的像素宽度

    // 起始节点坐标配置
    QPointF m_startNodePosition;                     // 外部设置的起始节点坐标

    // 配置数据引用（用于访问节点信息）
    const GraphConfig* m_currentConfig;

    // 日志实例
    LoggerManager& m_logger;
    std::string m_loggerFileName;
    
    // 构建阶段状态
    BuildStage m_stage;

    // ========== 核心流程 ==========
    void buildMultilist(const GraphConfig& config);
    void layoutAndDraw(const GraphConfig& config);

    // ========== 节点处理 ==========
    void processNode(int nodeId, const QSharedPointer<GraphNode>& node);
    void createFoupDevices(int nodeId, const QPointF& trackPos);

    // ========== 边处理 ==========
    void processEdge(const GraphConfig::EdgeInfo& edgeInfo,
                     const QSharedPointer<GraphNode>& fromNode,
                     const QSharedPointer<GraphNode>& toNode);

    void processStraightLine(int fromId, int toId, double size, double offset = 0.0);
    void processVirtualLine(int fromId, int toId, EdgeType edgeType, double size, double offset = 0.0);
    void processSCurve(int fromId, int toId, EdgeType edgeType, double size, double offset = 0.0);
    void processSemicircleArc(int fromId, int toId, EdgeType edgeType, double size, double offset = 0.0);

    // ========== 辅助方法 ==========
    // 计算某个 SET 节点中所有 Foup 的总宽度
    double computeSetFoupWidth(int nodeId) const;
    // 计算 FoupLevel 下一个 "set 单元" 的像素宽度（用于 TRACK 间距缩放）
    double computeFoupUnit() const;
    // 将 SetLevel 像素距离等比缩放到 FoupLevel
    double rescaleToFoupLevel(double setLevelSize) const;
    QString edgeTypeToString(EdgeType type);
};

}

#endif // FOUPLEVELGRAPHBUILDER_H
