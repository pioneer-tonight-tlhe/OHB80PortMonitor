#ifndef SETLEVELGRAPHBUILDER_H
#define SETLEVELGRAPHBUILDER_H

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

// SetLevel 视图构建器：每个 SET 节点创建一个 FrameDevice（Set级别）
class SetLevelGraphBuilder
{
public:
    enum class BuildStage {
        NotStarted = 0,           // 未开始
        MultilistBuilt = 1,       // 重建邻接多重表完成
        LayoutDrawn = 2,          // 绘制图像界面完成
        Completed = 3             // 布局图重构完毕
    };
    explicit SetLevelGraphBuilder(GraphMultilist& graph, QMap<int, QSharedPointer<FrameDevice>>& frameDevices, 
                          QList<DrawCommand>& drawCommands, CraneMapWidget* widget, FrameDevicePool* devicePool);
    ~SetLevelGraphBuilder();

    // 设置起始节点坐标
    void setStartNodePosition(const QPointF& position);

    // 设置设备控件到线的距离
    void setFrameDeviceTrackGap(double value);

    // 设置设备控件宽度
    void setFrameDeviceWidth(int width);
    
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
    int m_frameDeviceWidth;                             // 设备控件宽度
    
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
    // 根据配置构建邻接多重表（添加所有节点和边到图中）
    void buildMultilist(const GraphConfig& config);
    // 计算节点布局坐标并绘制图形控件（遍历边，处理节点和边的绘制）
    void layoutAndDraw(const GraphConfig& config);

    // ========== 节点处理 ==========
    void processNode(int nodeId, const QSharedPointer<GraphNode>& node);
    void createFrameDevice(int nodeId, const QPointF& pos);

    // ========== 边处理 ==========
    void processEdge(const GraphConfig::EdgeInfo& edgeInfo,
                     const QSharedPointer<GraphNode>& fromNode,
                     const QSharedPointer<GraphNode>& toNode);

    void processStraightLine(int fromId, int toId, double size, double offset = 0.0);
    void processVirtualLine(int fromId, int toId, EdgeType edgeType, double size, double offset = 0.0);
    void processSCurve(int fromId, int toId, EdgeType edgeType, double size, double offset = 0.0);
    void processSemicircleArc(int fromId, int toId, EdgeType edgeType, double size, double offset = 0.0);
    
    // 辅助方法
    QString edgeTypeToString(EdgeType type);
};

}

#endif // SETLEVELGRAPHBUILDER_H
