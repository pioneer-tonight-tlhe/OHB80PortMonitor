#include "setlevelgraphbuilder.h"
#include "cranemapwidget.h"
#include "framedevice.h"
#include "framedevicepool.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include <cmath>
#include <algorithm>

Graph::SetLevelGraphBuilder::SetLevelGraphBuilder(GraphMultilist &graph, QMap<int, QSharedPointer<FrameDevice> > &frameDevices, 
                                       QList<Graph::DrawCommand>& drawCommands, CraneMapWidget *widget, FrameDevicePool* devicePool)
    : m_graph(graph),
      m_widget(widget),
      m_frameDevices(frameDevices),
      m_drawCommands(drawCommands),
      m_devicePool(devicePool),
      m_frameDeviceTrackGap(10.0),
      m_frameDeviceWidth(120),
      m_startNodePosition(20.0, 170.0),
      m_currentConfig(nullptr),
      m_logger(*LoggerManager::getInstance()),
      m_loggerFileName(AppLogger::CraneMapLoggerPath().toStdString()),
      m_stage(BuildStage::NotStarted)
{
}

Graph::SetLevelGraphBuilder::~SetLevelGraphBuilder()
{
}

const char* Graph::SetLevelGraphBuilder::stageToString(BuildStage stage)
{
    switch (stage) {
    case BuildStage::NotStarted: return "NotStarted";
    case BuildStage::MultilistBuilt: return "MultilistBuilt";
    case BuildStage::LayoutDrawn: return "LayoutDrawn";
    case BuildStage::Completed: return "Completed";
    default: return "Unknown";
    }
}

void Graph::SetLevelGraphBuilder::setStartNodePosition(const QPointF& position)
{
    m_startNodePosition = position;
    std::string loggerPre = "[ui][SetLevelGraphBuilder][setStartNodePosition]";
    m_logger.log(m_loggerFileName, Level::INFO, "{} 设置起始节点坐标: ({}, {})", 
                 loggerPre, position.x(), position.y());
}

void Graph::SetLevelGraphBuilder::setFrameDeviceTrackGap(double value)
{
    if (value >= 0.0) {
        m_frameDeviceTrackGap = value;
    }
}

void Graph::SetLevelGraphBuilder::setFrameDeviceWidth(int width)
{
    if (width > 0) {
        m_frameDeviceWidth = width;
    }
}

bool Graph::SetLevelGraphBuilder::buildGraph(const GraphConfig& config)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][buildGraph]";
    m_stage = BuildStage::NotStarted;
    m_logger.log(m_loggerFileName, Level::INFO, "====================Set视图天车轨道布局图重构  开始====================");
    
    m_logger.log(m_loggerFileName, Level::INFO, "{} 开始构建图，节点数: {}，边数: {}", 
                 loggerPre, config.nodes.size(), config.edges.size());
    
    m_currentConfig = &config;
    
    // 使用配置中的设备控件尺寸
    if (config.deviceSize > 0) {
        m_frameDeviceWidth = config.deviceSize;
    }
    
    m_visitedNodes.clear();
    m_visitedEdges.clear();
    m_nodePositions.clear();
    
    buildMultilist(config);
    if (m_stage != BuildStage::MultilistBuilt) {
        m_stage = BuildStage::MultilistBuilt;
        m_logger.log(m_loggerFileName, Level::INFO, "{} 阶段(1): 重建邻接多重表完成", loggerPre);
    }
    
    layoutAndDraw(config);
    if (m_stage != BuildStage::LayoutDrawn) {
        m_stage = BuildStage::LayoutDrawn;
        m_logger.log(m_loggerFileName, Level::INFO, "{} 阶段(2): 绘制图像界面完成", loggerPre);
    }
    
    m_stage = BuildStage::Completed;
    m_logger.log(m_loggerFileName, Level::INFO, "{} 阶段(3): 布局图重构完毕", loggerPre);
    m_logger.log(m_loggerFileName, Level::INFO, "{} 图构建完成", loggerPre);
    m_logger.log(m_loggerFileName, Level::INFO, "====================Set视图天车轨道布局图重构  结束====================");
    return true;
}

void Graph::SetLevelGraphBuilder::buildMultilist(const GraphConfig& config)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][buildMultilist]";
    
    for (auto it = config.nodes.begin(); it != config.nodes.end(); ++it)
    {
        m_graph.addVertex(it.value());
    }
    
    for (const auto& edgeInfo : config.edges)
    {
        auto sourceNode = config.nodes.value(edgeInfo.sourceId);
        auto targetNode = config.nodes.value(edgeInfo.targetId);
        
        if (!sourceNode || !targetNode)
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}边 {} -> {} 的节点不存在",
                         loggerPre, edgeInfo.sourceId, edgeInfo.targetId);
            continue;
        }
        
        auto edge = QSharedPointer<GraphEdge>::create(edgeInfo.type, edgeInfo.size);
        m_graph.addEdge(sourceNode, targetNode, edge);
    }
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}邻接多重表构建完成", loggerPre);
}

void Graph::SetLevelGraphBuilder::layoutAndDraw(const GraphConfig& config)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][layoutAndDraw]";
    
    if (config.nodes.contains(config.startNodeId))
    {
        m_nodePositions[config.startNodeId] = m_startNodePosition;
        m_logger.log(m_loggerFileName, Level::INFO, "{}起始节点ID: {}, 坐标: ({}, {})", 
                     loggerPre, config.startNodeId, m_startNodePosition.x(), m_startNodePosition.y());
    }
    
    for (const auto& edgeInfo : config.edges)
    {
        int fromId = edgeInfo.sourceId;
        int toId = edgeInfo.targetId;
        
        QPair<int, int> edgePair(fromId, toId);
        if (m_visitedEdges.contains(edgePair))
            continue;
        m_visitedEdges.insert(edgePair);
        
        auto fromNode = config.nodes.value(fromId);
        auto toNode = config.nodes.value(toId);
        
        if (!fromNode || !toNode)
            continue;
        
        if (!m_visitedNodes.contains(fromId))
        {
            processNode(fromId, fromNode);
            m_visitedNodes.insert(fromId);
        }
        
        processEdge(edgeInfo, fromNode, toNode);
        
        if (!m_visitedNodes.contains(toId))
        {
            processNode(toId, toNode);
            m_visitedNodes.insert(toId);
        }
    }
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}布局和绘制完成", loggerPre);
}

void Graph::SetLevelGraphBuilder::processNode(int nodeId, const QSharedPointer<GraphNode>& node)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][processNode]";
    
    if (node->getNodeType() == NodeType::SET)
    {
        if (m_nodePositions.contains(nodeId))
        {
            createFrameDevice(nodeId, m_nodePositions[nodeId]);
            m_logger.log(m_loggerFileName, Level::INFO, "{}创建SET节点: ID={}", loggerPre, nodeId);
        }
    }
}

void Graph::SetLevelGraphBuilder::createFrameDevice(int nodeId, const QPointF& pos)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][createFrameDevice]";
    
    if (!m_currentConfig || !m_currentConfig->nodes.contains(nodeId)) {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}无法找到节点ID={}", loggerPre, nodeId);
        return;
    }
    
    QSharedPointer<GraphNode> node = m_currentConfig->nodes[nodeId];
    if (!node) {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}节点ID={}为空指针", loggerPre, nodeId);
        return;
    }
    
    // 从控件池获取或创建 FrameDevice（复用机制）
    auto frameDevice = m_devicePool->acquire(FrameDevicePool::SetLevel, nodeId, FrameDevice::DeviceType::Set);
    frameDevice->adjustSize();
    
    // 根据 uiId 获取对应的 SetOfOHBInfo 对象
    int uiId = node->getId();
    
    if (node->getNodeType() == Graph::NodeType::SET && uiId > 0) {
        QSharedPointer<SetOfOHBInfo> setInfo = SharedData::getSetOfOHBInfoByUiId(uiId);
        if (setInfo) {
            frameDevice->setSetOfOHBInfo(setInfo);
            
            // 记录成功赋值的信息
            m_logger.log(m_loggerFileName, Level::INFO, "{}成功为节点ID={}设置Set信息: uiId={}, setId={}, 压力范围={}, 流量平均值={}, 湿度范围={}", 
                         loggerPre, nodeId, uiId, 
                         setInfo->getSetId().toStdString(),
                         setInfo->getInletPressureRange().toStdString(),
                         setInfo->getInletFlowAverage().toStdString(),
                         setInfo->getRHRange().toStdString());
        } else {
            m_logger.log(m_loggerFileName, Level::WARN, "{}未找到uiId={}对应的SetOfOHBInfo对象", loggerPre, uiId);
        }
    } else {
        m_logger.log(m_loggerFileName, Level::INFO, "{}节点ID={}不是SET类型或uiId为0，跳过数据绑定", loggerPre, nodeId);
    }

    frameDevice->setFixedWidth(m_frameDeviceWidth);

    QSize deviceSize = frameDevice->sizeHint().expandedTo(frameDevice->minimumSizeHint());
    int deviceWidth = frameDevice->width();
    int deviceHeight = qMax(deviceSize.height(), frameDevice->height());

    QPointF actualPos = pos;
    double distanceFromTrack = m_frameDeviceTrackGap;
    
    if (node->getPosition() == SetPosition::ABOVE) {
        actualPos.setY(pos.y() - distanceFromTrack - deviceHeight);
    } else {
        actualPos.setY(pos.y() + distanceFromTrack);
    }
    
    actualPos.setX(pos.x() - deviceWidth / 2.0);
    
    frameDevice->move(actualPos.x(), actualPos.y());
    frameDevice->show();
    
    m_frameDevices[nodeId] = frameDevice;
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}创建FrameDevice: ID={}, 轨道坐标=({}, {}), 实际坐标=({}, {}), 位置={}", 
                 loggerPre, nodeId, pos.x(), pos.y(), actualPos.x(), actualPos.y(),
                 node->getPosition() == SetPosition::ABOVE ? "ABOVE" : "BELOW");
}

void Graph::SetLevelGraphBuilder::processEdge(const GraphConfig::EdgeInfo& edgeInfo,
                                       const QSharedPointer<GraphNode>& /*fromNode*/,
                                       const QSharedPointer<GraphNode>& /*toNode*/)
{
    switch (edgeInfo.type)
    {
    case EdgeType::STRAIGHT_LINE:
        processStraightLine(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.size, edgeInfo.setLevelOffset);
        break;
    case EdgeType::HORIZONTAL_VIRTUAL_LINE:
    case EdgeType::VERTICAL_VIRTUAL_LINE:
        processVirtualLine(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.type, edgeInfo.size, edgeInfo.setLevelOffset);
        break;
    case EdgeType::S_CURVE:
    case EdgeType::REVERSE_S_CURVE:
        processSCurve(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.type, edgeInfo.size, edgeInfo.setLevelOffset);
        break;
    case EdgeType::LEFT_SEMICIRCLE_ARC:
    case EdgeType::RIGHT_SEMICIRCLE_ARC:
    case EdgeType::LEFT_SEMICIRCLE_ARC_2:
    case EdgeType::RIGHT_SEMICIRCLE_ARC_2:
        processSemicircleArc(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.type, edgeInfo.size, edgeInfo.setLevelOffset);
        break;
    default:
        break;
    }
}

void Graph::SetLevelGraphBuilder::processStraightLine(int fromId, int toId, double size, double offset)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][processStraightLine]";
    
    if (!m_nodePositions.contains(fromId))
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}起始节点ID={}没有坐标", loggerPre, fromId);
        return;
    }
    
    QPointF fromPos = m_nodePositions[fromId];
    QPointF toPos;
    
    if (m_nodePositions.contains(toId))
    {
        toPos = m_nodePositions[toId];
        m_logger.log(m_loggerFileName, Level::INFO, "{}直线: {} -> {}, 目标节点已有坐标，起点({},{}), 终点({},{})", 
                     loggerPre, fromId, toId, fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
    }
    else
    {
        double distance = size + offset;
        toPos.setX(fromPos.x() + distance);
        toPos.setY(fromPos.y());
        m_nodePositions[toId] = toPos;
        if (offset != 0.0) {
            m_logger.log(m_loggerFileName, Level::INFO, "{}直线: {} -> {}, 长度: {} + 偏移: {} = {}, 起点({},{}), 终点({},{})", 
                         loggerPre, fromId, toId, size, offset, distance, fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
        } else {
            m_logger.log(m_loggerFileName, Level::INFO, "{}直线: {} -> {}, 长度: {}, 起点({},{}), 终点({},{})", 
                         loggerPre, fromId, toId, size, fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
        }
    }
    
    Graph::DrawCommand cmd;
    cmd.type = Graph::DrawCommand::Line;
    cmd.from = fromPos;
    cmd.to = toPos;
    m_drawCommands.append(cmd);
}

void Graph::SetLevelGraphBuilder::processVirtualLine(int fromId, int toId, EdgeType edgeType, double size, double /*offset*/)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][processVirtualLine]";
    
    if (!m_nodePositions.contains(fromId))
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}起始节点ID={}没有坐标", loggerPre, fromId);
        return;
    }
    
    QPointF fromPos = m_nodePositions[fromId];
    QPointF toPos;
    
    if (edgeType == EdgeType::HORIZONTAL_VIRTUAL_LINE)
    {
        toPos.setX(fromPos.x() + size);
        toPos.setY(fromPos.y());
    }
    else
    {
        toPos.setX(fromPos.x());
        toPos.setY(fromPos.y() + size);
    }
    
    m_nodePositions[toId] = toPos;
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}虚线: {} -> {}, 类型: {}, 起点({},{}), 终点({},{})",
                 loggerPre, fromId, toId, edgeTypeToString(edgeType).toStdString(),
                 fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
}

void Graph::SetLevelGraphBuilder::processSCurve(int fromId, int toId, EdgeType edgeType, double size, double /*offset*/)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][processSCurve]";
    
    if (!m_nodePositions.contains(fromId))
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}起始节点ID={}没有坐标", loggerPre, fromId);
        return;
    }
    
    QPointF fromPos = m_nodePositions[fromId];
    QPointF toPos;
    
    bool targetHasPosition = m_nodePositions.contains(toId);
    
    if (targetHasPosition)
    {
        toPos = m_nodePositions[toId];
        m_logger.log(m_loggerFileName, Level::INFO, "{}S曲线: {} -> {}, 目标节点已有坐标，使用已有坐标", 
                     loggerPre, fromId, toId);
    }
    else
    {
        double yDistance = size;
        double xDistance = yDistance * 2.0;
        
        toPos.setX(fromPos.x() + xDistance);
        
        // 判断是S型还是反向S型
        if (edgeType == EdgeType::S_CURVE) {
            // S型曲线：向上
            toPos.setY(fromPos.y() + yDistance);
        } else {
            // 反向S型曲线：向下
            toPos.setY(fromPos.y() - yDistance);
        }
        
        m_nodePositions[toId] = toPos;
        
        m_logger.log(m_loggerFileName, Level::INFO, "{}S曲线: {} -> {}, 类型: {}, Y距离: {}, X距离: {}", 
                     loggerPre, fromId, toId, edgeTypeToString(edgeType).toStdString(), yDistance, xDistance);
    }
    
    Graph::DrawCommand cmd;
    
    // 设置绘制指令类型
    if (edgeType == EdgeType::S_CURVE) {
        cmd.type = Graph::DrawCommand::SCurve;
    } else {
        cmd.type = Graph::DrawCommand::ReverseSCurve;
    }
    cmd.from = fromPos;
    cmd.to = toPos;
    
    // 计算控制点
    // 使用相对偏移量确保平行曲线形状一致
    // 控制点X = 起点X + (终点X - 起点X) * 0.5
    // 这样两条平移的曲线会保持相同的形状
    double xDistance = toPos.x() - fromPos.x();
    double controlX = fromPos.x() + xDistance * 0.5;
    
    // 两个控制点X坐标相同，Y坐标分别对应起点和终点
    // 这样曲线在起点和终点都有水平切线
    cmd.control1 = QPointF(controlX, fromPos.y());
    cmd.control2 = QPointF(controlX, toPos.y());
    
    // 详细日志：记录曲线的所有关键点
    m_logger.log(m_loggerFileName, Level::INFO, 
                 "{}绘制曲线 {} -> {}, 类型: {}, 起点({},{}), 终点({},{}), 控制点1({},{}), 控制点2({},{})",
                 loggerPre, fromId, toId, edgeTypeToString(edgeType).toStdString(),
                 fromPos.x(), fromPos.y(), toPos.x(), toPos.y(),
                 cmd.control1.x(), cmd.control1.y(), cmd.control2.x(), cmd.control2.y());
    
    m_drawCommands.append(cmd);
}

void Graph::SetLevelGraphBuilder::processSemicircleArc(int fromId, int toId, EdgeType edgeType, double size, double /*offset*/)
{
    std::string loggerPre = "[ui][SetLevelGraphBuilder][processSemicircleArc]";
    
    if (!m_nodePositions.contains(fromId))
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}起始节点ID={}没有坐标", loggerPre, fromId);
        return;
    }
    
    QPointF fromPos = m_nodePositions[fromId];
    QPointF toPos;
    
    // 半圆弧总是使用终点的已有坐标
    // 配置文件通过垂直虚线确保了起点和终点的Y坐标差
    if (m_nodePositions.contains(toId))
    {
        toPos = m_nodePositions[toId];
    }
    else
    {
        // 如果终点没有坐标，则计算（正常情况下不应该发生）
        toPos.setX(fromPos.x());
        toPos.setY(fromPos.y() + size);
        m_nodePositions[toId] = toPos;
        m_logger.log(m_loggerFileName, Level::WARN, "{}半圆弧: {} -> {}, 终点节点没有坐标，自动计算",
                     loggerPre, fromId, toId);
    }
    
    Graph::DrawCommand cmd;
    if (edgeType == EdgeType::LEFT_SEMICIRCLE_ARC) {
        cmd.type = Graph::DrawCommand::LeftArc;
    } else if (edgeType == EdgeType::RIGHT_SEMICIRCLE_ARC) {
        cmd.type = Graph::DrawCommand::RightArc;
    } else if (edgeType == EdgeType::LEFT_SEMICIRCLE_ARC_2) {
        cmd.type = Graph::DrawCommand::LeftArc2;
    } else {
        cmd.type = Graph::DrawCommand::RightArc2;
    }
    cmd.from = fromPos;
    cmd.to = toPos;
    cmd.arcDiameter = size;
    m_drawCommands.append(cmd);
    
    // 详细日志：记录半圆弧的起点和终点坐标
    m_logger.log(m_loggerFileName, Level::INFO, 
                 "{}绘制半圆弧 {} -> {}, 类型: {}, 起点({},{}), 终点({},{})",
                 loggerPre, fromId, toId, edgeTypeToString(edgeType).toStdString(),
                 fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}半圆弧: {} -> {}, 类型: {}",
                 loggerPre, fromId, toId, edgeTypeToString(edgeType).toStdString());
}

QString Graph::SetLevelGraphBuilder::edgeTypeToString(EdgeType type)
{
    switch (type)
    {
    case EdgeType::STRAIGHT_LINE: return "STRAIGHT_LINE";
    case EdgeType::HORIZONTAL_VIRTUAL_LINE: return "HORIZONTAL_VIRTUAL_LINE";
    case EdgeType::VERTICAL_VIRTUAL_LINE: return "VERTICAL_VIRTUAL_LINE";
    case EdgeType::S_CURVE: return "S_CURVE";
    case EdgeType::REVERSE_S_CURVE: return "REVERSE_S_CURVE";
    case EdgeType::LEFT_SEMICIRCLE_ARC: return "LEFT_SEMICIRCLE_ARC";
    case EdgeType::RIGHT_SEMICIRCLE_ARC: return "RIGHT_SEMICIRCLE_ARC";
    case EdgeType::LEFT_SEMICIRCLE_ARC_2: return "LEFT_SEMICIRCLE_ARC_2";
    case EdgeType::RIGHT_SEMICIRCLE_ARC_2: return "RIGHT_SEMICIRCLE_ARC_2";
    default: return "UNKNOWN";
    }
}
