#include "fouplevelgraphbuilder.h"
#include "cranemapwidget.h"
#include "framedevice.h"
#include "framedevicepool.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include <cmath>
#include <algorithm>

Graph::FoupLevelGraphBuilder::FoupLevelGraphBuilder(GraphMultilist &graph, QMap<int, QSharedPointer<FrameDevice> > &frameDevices,
                                          QList<Graph::DrawCommand>& drawCommands, CraneMapWidget *widget, FrameDevicePool* devicePool)
    : m_graph(graph),
      m_widget(widget),
      m_frameDevices(frameDevices),
      m_drawCommands(drawCommands),
      m_devicePool(devicePool),
      m_frameDeviceTrackGap(10.0),
      m_foupDeviceWidth(120),
      m_foupSpacing(5),
      m_setSpacing(10),
      m_foupUnit(0.0),
      m_startNodePosition(20.0, 170.0),
      m_currentConfig(nullptr),
      m_logger(*LoggerManager::getInstance()),
      m_loggerFileName(AppLogger::CraneMapLoggerPath().toStdString()),
      m_stage(BuildStage::NotStarted)
{
}

Graph::FoupLevelGraphBuilder::~FoupLevelGraphBuilder()
{
}

const char* Graph::FoupLevelGraphBuilder::stageToString(BuildStage stage)
{
    switch (stage) {
    case BuildStage::NotStarted: return "NotStarted";
    case BuildStage::MultilistBuilt: return "MultilistBuilt";
    case BuildStage::LayoutDrawn: return "LayoutDrawn";
    case BuildStage::Completed: return "Completed";
    default: return "Unknown";
    }
}

void Graph::FoupLevelGraphBuilder::setStartNodePosition(const QPointF& position)
{
    m_startNodePosition = position;
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][setStartNodePosition]";
    m_logger.log(m_loggerFileName, Level::INFO, "{}设置起始节点坐标: ({}, {})",
                 loggerPre, position.x(), position.y());
}

void Graph::FoupLevelGraphBuilder::setFrameDeviceTrackGap(double value)
{
    if (value >= 0.0) {
        m_frameDeviceTrackGap = value;
    }
}

void Graph::FoupLevelGraphBuilder::setFoupDeviceWidth(int width)
{
    if (width > 0) {
        m_foupDeviceWidth = width;
    }
}

void Graph::FoupLevelGraphBuilder::setFoupSpacing(int spacing)
{
    if (spacing >= 0) {
        m_foupSpacing = spacing;
    }
}

bool Graph::FoupLevelGraphBuilder::buildGraph(const GraphConfig& config)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][buildGraph]";
    m_stage = BuildStage::NotStarted;
    m_logger.log(m_loggerFileName, Level::INFO, "====================Foup视图天车轨道布局图重构  开始====================");

    m_logger.log(m_loggerFileName, Level::INFO, "{}开始构建 FoupLevel视图，节点数: {}，边数: {}",
                 loggerPre, config.nodes.size(), config.edges.size());

    m_currentConfig = &config;

    // 使用配置中的间距作为 Set 之间的间距
    if (config.deviceSpacing > 0) {
        m_setSpacing = config.deviceSpacing;
    }

    m_visitedNodes.clear();
    m_visitedEdges.clear();
    m_nodePositions.clear();

    // 计算 FoupLevel 单元宽度（用于 TRACK 间距缩放）
    m_foupUnit = computeFoupUnit();
    m_logger.log(m_loggerFileName, Level::INFO, "{}FoupLevel单元宽度: {}, SetLevel单元宽度: {}",
                 loggerPre, m_foupUnit, config.deviceSize + config.deviceSpacing);

    buildMultilist(config);
    if (m_stage != BuildStage::MultilistBuilt) {
        m_stage = BuildStage::MultilistBuilt;
        m_logger.log(m_loggerFileName, Level::INFO, "{}阶段(1): 重建邻接多重表完成", loggerPre);
    }
    
    layoutAndDraw(config);
    if (m_stage != BuildStage::LayoutDrawn) {
        m_stage = BuildStage::LayoutDrawn;
        m_logger.log(m_loggerFileName, Level::INFO, "{}阶段(2): 绘制图像界面完成", loggerPre);
    }
    
    m_stage = BuildStage::Completed;
    m_logger.log(m_loggerFileName, Level::INFO, "{}阶段(3): 布局图重构完毕", loggerPre);
    m_logger.log(m_loggerFileName, Level::INFO, "{}FoupLevel视图构建完成", loggerPre);
    m_logger.log(m_loggerFileName, Level::INFO, "====================Foup视图天车轨道布局图重构  结束====================");
    return true;
}

void Graph::FoupLevelGraphBuilder::buildMultilist(const GraphConfig& config)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][buildMultilist]";

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

void Graph::FoupLevelGraphBuilder::layoutAndDraw(const GraphConfig& config)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][layoutAndDraw]";

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

void Graph::FoupLevelGraphBuilder::processNode(int nodeId, const QSharedPointer<GraphNode>& node)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][processNode]";

    if (node->getNodeType() == NodeType::SET)
    {
        if (m_nodePositions.contains(nodeId))
        {
            createFoupDevices(nodeId, m_nodePositions[nodeId]);
            m_logger.log(m_loggerFileName, Level::INFO, "{}创建SET节点的Foup设备: ID={}", loggerPre, nodeId);
        }
    }
}

void Graph::FoupLevelGraphBuilder::createFoupDevices(int nodeId, const QPointF& trackPos)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][createFoupDevices]";

    if (!m_currentConfig || !m_currentConfig->nodes.contains(nodeId)) {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}无法找到节点ID={}", loggerPre, nodeId);
        return;
    }

    QSharedPointer<GraphNode> node = m_currentConfig->nodes[nodeId];
    if (!node) {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}节点ID={}为空指针", loggerPre, nodeId);
        return;
    }

    // 根据 uiId 获取对应的 SetOfOHBInfo 对象
    int uiId = node->getId();
    QSharedPointer<SetOfOHBInfo> setInfo = SharedData::getSetOfOHBInfoByUiId(uiId);
    if (!setInfo) {
        m_logger.log(m_loggerFileName, Level::WARN, "{}未找到uiId={}对应的SetOfOHBInfo对象", loggerPre, uiId);
        return;
    }

    const QVector<FoupOfOHBInfo>& foups = setInfo->getFoups();
    int foupCount = foups.size();
    if (foupCount == 0) {
        m_logger.log(m_loggerFileName, Level::WARN, "{}SetOfOHBInfo uiId={} 没有Foup数据", loggerPre, uiId);
        return;
    }

    // 计算所有 Foup 的总宽度，居中放置在 trackPos 上
    double totalWidth = foupCount * m_foupDeviceWidth + (foupCount - 1) * m_foupSpacing;
    double startX = trackPos.x() - totalWidth / 2.0;
    
    m_logger.log(m_loggerFileName, Level::INFO, 
                 "{}开始创建 {} 个Foup设备, SET_ID={}, uiId={}, trackPos=({}, {}), totalWidth={}, startX={}",
                 loggerPre, foupCount, nodeId, uiId, trackPos.x(), trackPos.y(), totalWidth, startX);

    for (int i = 0; i < foupCount; ++i) {
        // 使用 nodeId * 100 + foupIndex 作为唯一键
        int deviceKey = nodeId * 100 + i;
        
        // 从控件池获取或创建 FrameDevice（复用机制）
        auto frameDevice = m_devicePool->acquire(FrameDevicePool::FoupLevel, deviceKey, FrameDevice::DeviceType::Foup);
        frameDevice->adjustSize();
        frameDevice->setFixedWidth(m_foupDeviceWidth);

        // 创建指向 FoupOfOHBInfo 的智能指针（使用空删除器，因为数据由 App 管理）
        QSharedPointer<FoupOfOHBInfo> foupPtr(
            const_cast<FoupOfOHBInfo*>(&foups[i]),
            [](FoupOfOHBInfo*) { /* 空删除器 */ }
        );
        frameDevice->setFoupOfOHBInfo(foupPtr);

        // 计算 Foup 设备的 X 坐标
        double foupX = startX + i * (m_foupDeviceWidth + m_foupSpacing);

        // 计算设备高度
        QSize deviceSize = frameDevice->sizeHint().expandedTo(frameDevice->minimumSizeHint());
        int deviceHeight = qMax(deviceSize.height(), frameDevice->height());

        // 计算实际位置（根据 ABOVE/BELOW 定位）
        QPointF actualPos;
        actualPos.setX(foupX);

        if (node->getPosition() == SetPosition::ABOVE) {
            actualPos.setY(trackPos.y() - m_frameDeviceTrackGap - deviceHeight);
        } else {
            actualPos.setY(trackPos.y() + m_frameDeviceTrackGap);
        }

        frameDevice->move(actualPos.x(), actualPos.y());
        frameDevice->show();

        // 存储到设备映射表（deviceKey 已在循环开始时计算）
        m_frameDevices[deviceKey] = frameDevice;

        m_logger.log(m_loggerFileName, Level::INFO,
                     "{}创建Foup设备: SET_ID={}, Foup[{}], qrCode={}, 坐标=({}, {})",
                     loggerPre, nodeId, i,
                     foups[i].qrCode().toStdString(),
                     actualPos.x(), actualPos.y());
    }
    
    m_logger.log(m_loggerFileName, Level::INFO, 
                 "{}完成创建 {} 个Foup设备, SET_ID={}, 设备键范围: {} - {}",
                 loggerPre, foupCount, nodeId, nodeId * 100, nodeId * 100 + foupCount - 1);
}

void Graph::FoupLevelGraphBuilder::processEdge(const GraphConfig::EdgeInfo& edgeInfo,
                                           const QSharedPointer<GraphNode>& /*fromNode*/,
                                           const QSharedPointer<GraphNode>& /*toNode*/)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][processEdge]";
    
    m_logger.log(m_loggerFileName, Level::DEBUG, 
                 "{}处理边: {} -> {}, 类型: {}, size: {}, offset: {}",
                 loggerPre, edgeInfo.sourceId, edgeInfo.targetId, 
                 edgeTypeToString(edgeInfo.type).toStdString(), 
                 edgeInfo.size, edgeInfo.foupLevelOffset);
    
    switch (edgeInfo.type)
    {
    case EdgeType::STRAIGHT_LINE:
        processStraightLine(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.size, edgeInfo.foupLevelOffset);
        break;
    case EdgeType::HORIZONTAL_VIRTUAL_LINE:
    case EdgeType::VERTICAL_VIRTUAL_LINE:
        processVirtualLine(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.type, edgeInfo.size, edgeInfo.foupLevelOffset);
        break;
    case EdgeType::S_CURVE:
    case EdgeType::REVERSE_S_CURVE:
        processSCurve(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.type, edgeInfo.size, edgeInfo.foupLevelOffset);
        break;
    case EdgeType::LEFT_SEMICIRCLE_ARC:
    case EdgeType::RIGHT_SEMICIRCLE_ARC:
    case EdgeType::LEFT_SEMICIRCLE_ARC_2:
    case EdgeType::RIGHT_SEMICIRCLE_ARC_2:
        processSemicircleArc(edgeInfo.sourceId, edgeInfo.targetId, edgeInfo.type, edgeInfo.size, edgeInfo.foupLevelOffset);
        break;
    default:
        m_logger.log(m_loggerFileName, Level::WARN, "{}未知的边类型", loggerPre);
        break;
    }
}

void Graph::FoupLevelGraphBuilder::processStraightLine(int fromId, int toId, double size, double offset)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][processStraightLine]";

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
        auto fromNode = m_currentConfig->nodes.value(fromId);
        auto toNode = m_currentConfig->nodes.value(toId);

        bool fromIsSet = fromNode && fromNode->getNodeType() == NodeType::SET;
        bool toIsSet = toNode && toNode->getNodeType() == NodeType::SET;
        bool fromIsTrack = fromNode && fromNode->getNodeType() == NodeType::TRACK;
        bool toIsTrack = toNode && toNode->getNodeType() == NodeType::TRACK;

        m_logger.log(m_loggerFileName, Level::INFO, 
                     "{}边 {} -> {} 类型分析:\n"
                     "  fromId {}: {} {}\n"
                     "  toId {}: {} {}\n"
                     "  原始 size = {}",
                     loggerPre, fromId, toId,
                     fromId, fromIsSet ? "SET" : (fromIsTrack ? "TRACK" : "UNKNOWN"), fromNode ? "" : "(null)",
                     toId, toIsSet ? "SET" : (toIsTrack ? "TRACK" : "UNKNOWN"), toNode ? "" : "(null)",
                     size);

        double distance = size;

        if (fromIsSet || toIsSet) {
            // 对于涉及 SET 节点的边，Size 表示 set 单位的数量
            // 计算一个 set 的实际宽度
            int typicalFoupCount = 4;
            for (auto it = m_currentConfig->nodes.begin(); it != m_currentConfig->nodes.end(); ++it) {
                if (it.value()->getNodeType() == NodeType::SET) {
                    auto setNode = qSharedPointerCast<SetOfOHBNode>(it.value());
                    if (setNode && setNode->getFoupCount() > 0) {
                        typicalFoupCount = setNode->getFoupCount();
                        break;
                    }
                }
            }
            
            double foupDevicesWidth = typicalFoupCount * m_foupDeviceWidth;
            double totalFoupSpacing = (typicalFoupCount - 1) * m_foupSpacing;
            double setFoupWidth = foupDevicesWidth + totalFoupSpacing;
            
            // 计算原始倍数
            double unit = m_currentConfig->deviceSize + m_currentConfig->deviceSpacing;
            double multiplier = (unit > 0) ? (size / unit) : 1.0;
            
            // 如果两端都是 SET，需要加上 SET 间距
            if (fromIsSet && toIsSet) {
                // SET-SET: distance = multiplier × (setFoupWidth + m_setSpacing) + offset
                distance = multiplier * (setFoupWidth + m_setSpacing) + offset;
                
                if (offset != 0.0) {
                    m_logger.log(m_loggerFileName, Level::INFO,
                                 "{}SET-SET 边计算过程:\n"
                                 "  typicalFoupCount = {}\n"
                                 "  m_foupDeviceWidth = {} → foupDevicesWidth = {} × {} = {}\n"
                                 "  m_foupSpacing = {} → totalFoupSpacing = ({} - 1) × {} = {}\n"
                                 "  setFoupWidth = {} + {} = {}\n"
                                 "  m_setSpacing = {}\n"
                                 "  原始 size = {}\n"
                                 "  unit = deviceSize + deviceSpacing = {} + {} = {}\n"
                                 "  multiplier = size / unit = {} / {} = {}\n"
                                 "  基础 distance = {} × ({} + {}) = {}\n"
                                 "  offset = {}\n"
                                 "  最终 distance = {} + {} = {}",
                                 loggerPre,
                                 typicalFoupCount,
                                 m_foupDeviceWidth, typicalFoupCount, m_foupDeviceWidth, foupDevicesWidth,
                                 m_foupSpacing, typicalFoupCount, m_foupSpacing, totalFoupSpacing,
                                 foupDevicesWidth, totalFoupSpacing, setFoupWidth,
                                 m_setSpacing,
                                 size,
                                 m_currentConfig->deviceSize, m_currentConfig->deviceSpacing, unit,
                                 size, unit, multiplier,
                                 multiplier, setFoupWidth, m_setSpacing, distance - offset,
                                 offset,
                                 distance - offset, offset, distance);
                } else {
                    m_logger.log(m_loggerFileName, Level::INFO,
                                 "{}SET-SET 边计算过程:\n"
                                 "  typicalFoupCount = {}\n"
                                 "  m_foupDeviceWidth = {} → foupDevicesWidth = {} × {} = {}\n"
                                 "  m_foupSpacing = {} → totalFoupSpacing = ({} - 1) × {} = {}\n"
                                 "  setFoupWidth = {} + {} = {}\n"
                                 "  m_setSpacing = {}\n"
                                 "  原始 size = {}\n"
                                 "  unit = deviceSize + deviceSpacing = {} + {} = {}\n"
                                 "  multiplier = size / unit = {} / {} = {}\n"
                                 "  distance = multiplier × (setFoupWidth + m_setSpacing) = {} × ({} + {}) = {}",
                                 loggerPre,
                                 typicalFoupCount,
                                 m_foupDeviceWidth, typicalFoupCount, m_foupDeviceWidth, foupDevicesWidth,
                                 m_foupSpacing, typicalFoupCount, m_foupSpacing, totalFoupSpacing,
                                 foupDevicesWidth, totalFoupSpacing, setFoupWidth,
                                 m_setSpacing,
                                 size,
                                 m_currentConfig->deviceSize, m_currentConfig->deviceSpacing, unit,
                                 size, unit, multiplier,
                                 multiplier, setFoupWidth, m_setSpacing, distance);
                }
            } else {
                // TRACK-SET 或 SET-TRACK: distance = multiplier × setFoupWidth + offset
                distance = multiplier * setFoupWidth + offset;
                
                if (offset != 0.0) {
                    m_logger.log(m_loggerFileName, Level::INFO,
                                 "{}{}-SET 边计算过程:\n"
                                 "  typicalFoupCount = {}\n"
                                 "  m_foupDeviceWidth = {} → foupDevicesWidth = {} × {} = {}\n"
                                 "  m_foupSpacing = {} → totalFoupSpacing = ({} - 1) × {} = {}\n"
                                 "  setFoupWidth = {} + {} = {}\n"
                                 "  原始 size = {}\n"
                                 "  unit = deviceSize + deviceSpacing = {} + {} = {}\n"
                                 "  multiplier = size / unit = {} / {} = {}\n"
                                 "  基础 distance = {} × {} = {}\n"
                                 "  offset = {}\n"
                                 "  最终 distance = {} + {} = {}",
                                 loggerPre,
                                 fromIsSet ? "SET" : "TRACK",
                                 typicalFoupCount,
                                 m_foupDeviceWidth, typicalFoupCount, m_foupDeviceWidth, foupDevicesWidth,
                                 m_foupSpacing, typicalFoupCount, m_foupSpacing, totalFoupSpacing,
                                 foupDevicesWidth, totalFoupSpacing, setFoupWidth,
                                 size,
                                 m_currentConfig->deviceSize, m_currentConfig->deviceSpacing, unit,
                                 size, unit, multiplier,
                                 multiplier, setFoupWidth, distance - offset,
                                 offset,
                                 distance - offset, offset, distance);
                } else {
                    m_logger.log(m_loggerFileName, Level::INFO,
                                 "{}{}-SET 边计算过程:\n"
                                 "  typicalFoupCount = {}\n"
                                 "  m_foupDeviceWidth = {} → foupDevicesWidth = {} × {} = {}\n"
                                 "  m_foupSpacing = {} → totalFoupSpacing = ({} - 1) × {} = {}\n"
                                 "  setFoupWidth = {} + {} = {}\n"
                                 "  原始 size = {}\n"
                                 "  unit = deviceSize + deviceSpacing = {} + {} = {}\n"
                                 "  multiplier = size / unit = {} / {} = {}\n"
                                 "  distance = multiplier × setFoupWidth = {} × {} = {}",
                                 loggerPre,
                                 fromIsSet ? "SET" : "TRACK",
                                 typicalFoupCount,
                                 m_foupDeviceWidth, typicalFoupCount, m_foupDeviceWidth, foupDevicesWidth,
                                 m_foupSpacing, typicalFoupCount, m_foupSpacing, totalFoupSpacing,
                                 foupDevicesWidth, totalFoupSpacing, setFoupWidth,
                                 size,
                                 m_currentConfig->deviceSize, m_currentConfig->deviceSpacing, unit,
                                 size, unit, multiplier,
                                 multiplier, setFoupWidth, distance);
                }
            }
        } else if (fromIsTrack && toIsTrack) {
            // 两端都是 TRACK 节点：使用 set 的 foup 宽度作为单位
            // 计算一个 set 的 foup 宽度（不含 set 间距）
            int typicalFoupCount = 4;
            for (auto it = m_currentConfig->nodes.begin(); it != m_currentConfig->nodes.end(); ++it) {
                if (it.value()->getNodeType() == NodeType::SET) {
                    auto setNode = qSharedPointerCast<SetOfOHBNode>(it.value());
                    if (setNode && setNode->getFoupCount() > 0) {
                        typicalFoupCount = setNode->getFoupCount();
                        break;
                    }
                }
            }
            
            double foupDevicesWidth = typicalFoupCount * m_foupDeviceWidth;
            double totalFoupSpacing = (typicalFoupCount - 1) * m_foupSpacing;
            double setFoupWidth = foupDevicesWidth + totalFoupSpacing;
            
            // 计算原始倍数
            double setUnit = m_currentConfig->deviceSize + m_currentConfig->deviceSpacing;
            double multiplier = (setUnit > 0) ? (size / setUnit) : 1.0;
            
            // 边长 = 倍数 × set 的 foup 宽度 + offset
            distance = multiplier * setFoupWidth + offset;
            
            if (offset != 0.0) {
                m_logger.log(m_loggerFileName, Level::INFO,
                             "{}TRACK-TRACK 边计算过程:\n"
                             "  typicalFoupCount = {}\n"
                             "  m_foupDeviceWidth = {} → foupDevicesWidth = {} × {} = {}\n"
                             "  m_foupSpacing = {} → totalFoupSpacing = ({} - 1) × {} = {}\n"
                             "  setFoupWidth = {} + {} = {}\n"
                             "  原始 size = {}\n"
                             "  setUnit = deviceSize + deviceSpacing = {} + {} = {}\n"
                             "  multiplier = size / unit = {} / {} = {}\n"
                             "  基础 distance = {} × {} = {}\n"
                             "  offset = {}\n"
                             "  最终 distance = {} + {} = {}",
                             loggerPre,
                             typicalFoupCount,
                             m_foupDeviceWidth, typicalFoupCount, m_foupDeviceWidth, foupDevicesWidth,
                             m_foupSpacing, typicalFoupCount, m_foupSpacing, totalFoupSpacing,
                             foupDevicesWidth, totalFoupSpacing, setFoupWidth,
                             size,
                             m_currentConfig->deviceSize, m_currentConfig->deviceSpacing, setUnit,
                             size, setUnit, multiplier,
                             multiplier, setFoupWidth, distance - offset,
                             offset,
                             distance - offset, offset, distance);
            } else {
                m_logger.log(m_loggerFileName, Level::INFO,
                             "{}TRACK-TRACK 边计算过程:\n"
                             "  typicalFoupCount = {}\n"
                             "  m_foupDeviceWidth = {} → foupDevicesWidth = {} × {} = {}\n"
                             "  m_foupSpacing = {} → totalFoupSpacing = ({} - 1) × {} = {}\n"
                             "  setFoupWidth = {} + {} = {}\n"
                             "  原始 size = {}\n"
                             "  setUnit = deviceSize + deviceSpacing = {} + {} = {}\n"
                             "  multiplier = size / unit = {} / {} = {}\n"
                             "  distance = multiplier × setFoupWidth = {} × {} = {}",
                             loggerPre,
                             typicalFoupCount,
                             m_foupDeviceWidth, typicalFoupCount, m_foupDeviceWidth, foupDevicesWidth,
                             m_foupSpacing, typicalFoupCount, m_foupSpacing, totalFoupSpacing,
                             foupDevicesWidth, totalFoupSpacing, setFoupWidth,
                             size,
                             m_currentConfig->deviceSize, m_currentConfig->deviceSpacing, setUnit,
                             size, setUnit, multiplier,
                             multiplier, setFoupWidth, distance);
            }
        } else {
            m_logger.log(m_loggerFileName, Level::WARN, "{}未知节点类型组合，使用原始 size = {}", loggerPre, size);
        }

        toPos.setX(fromPos.x() + distance);
        toPos.setY(fromPos.y());
        m_nodePositions[toId] = toPos;

        m_logger.log(m_loggerFileName, Level::INFO, "{}直线: {} -> {}, 最终距离: {}, 起点({},{}), 终点({},{})",
                     loggerPre, fromId, toId, distance, fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
    }

    Graph::DrawCommand cmd;
    cmd.type = Graph::DrawCommand::Line;
    cmd.from = fromPos;
    cmd.to = toPos;
    m_drawCommands.append(cmd);
}

void Graph::FoupLevelGraphBuilder::processVirtualLine(int fromId, int toId, EdgeType edgeType, double size, double /*offset*/)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][processVirtualLine]";

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

void Graph::FoupLevelGraphBuilder::processSCurve(int fromId, int toId, EdgeType edgeType, double size, double /*offset*/)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][processSCurve]";

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
    }
    else
    {
        double yDistance = size;
        double xDistance = yDistance * 2.0;

        toPos.setX(fromPos.x() + xDistance);

        if (edgeType == EdgeType::S_CURVE) {
            toPos.setY(fromPos.y() + yDistance);
        } else {
            toPos.setY(fromPos.y() - yDistance);
        }

        m_nodePositions[toId] = toPos;
    }

    Graph::DrawCommand cmd;

    if (edgeType == EdgeType::S_CURVE) {
        cmd.type = Graph::DrawCommand::SCurve;
    } else {
        cmd.type = Graph::DrawCommand::ReverseSCurve;
    }
    cmd.from = fromPos;
    cmd.to = toPos;

    double xDistance = toPos.x() - fromPos.x();
    double controlX = fromPos.x() + xDistance * 0.5;

    cmd.control1 = QPointF(controlX, fromPos.y());
    cmd.control2 = QPointF(controlX, toPos.y());

    m_drawCommands.append(cmd);

    m_logger.log(m_loggerFileName, Level::INFO,
                 "{}S曲线: {} -> {}, 类型: {}, 起点({},{}), 终点({},{})",
                 loggerPre, fromId, toId, edgeTypeToString(edgeType).toStdString(),
                 fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
}

void Graph::FoupLevelGraphBuilder::processSemicircleArc(int fromId, int toId, EdgeType edgeType, double size, double /*offset*/)
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][processSemicircleArc]";

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
    }
    else
    {
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

    m_logger.log(m_loggerFileName, Level::INFO,
                 "{}半圆弧: {} -> {}, 类型: {}, 起点({},{}), 终点({},{})",
                 loggerPre, fromId, toId, edgeTypeToString(edgeType).toStdString(),
                 fromPos.x(), fromPos.y(), toPos.x(), toPos.y());
}

double Graph::FoupLevelGraphBuilder::computeFoupUnit() const
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][computeFoupUnit]";
    
    if (!m_currentConfig) {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}m_currentConfig 为空", loggerPre);
        return 0;
    }

    // 扫描 SET 节点获取典型 foupCount
    int typicalFoupCount = 4;
    m_logger.log(m_loggerFileName, Level::INFO, "{}开始扫描 SET 节点获取典型 foupCount", loggerPre);
    
    for (auto it = m_currentConfig->nodes.begin(); it != m_currentConfig->nodes.end(); ++it) {
        if (it.value()->getNodeType() == NodeType::SET) {
            auto setNode = qSharedPointerCast<SetOfOHBNode>(it.value());
            if (setNode && setNode->getFoupCount() > 0) {
                typicalFoupCount = setNode->getFoupCount();
                m_logger.log(m_loggerFileName, Level::INFO, "{}找到 SET 节点 ID={}, foupCount={}", 
                             loggerPre, it.key(), typicalFoupCount);
                break;
            }
        }
    }
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}最终使用 typicalFoupCount={}", loggerPre, typicalFoupCount);

    // 计算 set 展开后的总宽度
    double foupDevicesWidth = typicalFoupCount * m_foupDeviceWidth;
    double totalFoupSpacing = (typicalFoupCount - 1) * m_foupSpacing;
    double setFoupWidth = foupDevicesWidth + totalFoupSpacing;
    double foupUnit = setFoupWidth + m_setSpacing;
    
    m_logger.log(m_loggerFileName, Level::INFO, 
                 "{}FoupLevel 单元计算过程:\n"
                 "  typicalFoupCount = {}\n"
                 "  m_foupDeviceWidth = {} → foupDevicesWidth = {} × {} = {}\n"
                 "  m_foupSpacing = {} → totalFoupSpacing = ({} - 1) × {} = {}\n"
                 "  setFoupWidth = {} + {} = {}\n"
                 "  m_setSpacing = {}\n"
                 "  foupUnit = {} + {} = {}",
                 loggerPre,
                 typicalFoupCount,
                 m_foupDeviceWidth, typicalFoupCount, m_foupDeviceWidth, foupDevicesWidth,
                 m_foupSpacing, typicalFoupCount, m_foupSpacing, totalFoupSpacing,
                 foupDevicesWidth, totalFoupSpacing, setFoupWidth,
                 m_setSpacing,
                 setFoupWidth, m_setSpacing, foupUnit);
    
    return foupUnit;
}

double Graph::FoupLevelGraphBuilder::rescaleToFoupLevel(double setLevelSize) const
{
    std::string loggerPre = "[ui][FoupLevelGraphBuilder][rescaleToFoupLevel]";
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}输入参数: setLevelSize={}, m_foupUnit={}", 
                 loggerPre, setLevelSize, m_foupUnit);
    
    if (!m_currentConfig) {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}m_currentConfig 为空", loggerPre);
        return setLevelSize;
    }
    
    double setUnit = m_currentConfig->deviceSize + m_currentConfig->deviceSpacing;
    if (setUnit <= 0 || m_foupUnit <= 0) {
        m_logger.log(m_loggerFileName, Level::WARN, "{}setUnit={} 或 m_foupUnit={} 无效，返回原始值", 
                     loggerPre, setUnit, m_foupUnit);
        return setLevelSize;
    }
    
    double scaleFactor = m_foupUnit / setUnit;
    double scaledSize = setLevelSize * scaleFactor;
    
    m_logger.log(m_loggerFileName, Level::INFO,
                 "{}SetLevel 到 FoupLevel 缩放:\n"
                 "  setLevelSize = {}\n"
                 "  setUnit = deviceSize + deviceSpacing = {} + {} = {}\n"
                 "  foupUnit = {}\n"
                 "  scaleFactor = foupUnit / setUnit = {} / {} = {}\n"
                 "  scaledSize = {} × {} = {}",
                 loggerPre,
                 setLevelSize,
                 m_currentConfig->deviceSize, m_currentConfig->deviceSpacing, setUnit,
                 m_foupUnit,
                 m_foupUnit, setUnit, scaleFactor,
                 setLevelSize, scaleFactor, scaledSize);
    
    return scaledSize;
}

double Graph::FoupLevelGraphBuilder::computeSetFoupWidth(int nodeId) const
{
    auto node = m_currentConfig->nodes.value(nodeId);
    if (!node || node->getNodeType() != NodeType::SET) return 0;

    auto setNode = qSharedPointerCast<SetOfOHBNode>(node);
    if (!setNode) return 0;

    int foupCount = setNode->getFoupCount();
    if (foupCount <= 0) {
        // 从 SetOfOHBInfo 获取实际 Foup 数量
        QSharedPointer<SetOfOHBInfo> setInfo = SharedData::getSetOfOHBInfoByUiId(node->getId());
        if (setInfo) {
            foupCount = setInfo->getFoups().size();
        }
    }

    if (foupCount <= 0) return m_foupDeviceWidth;

    return foupCount * m_foupDeviceWidth + (foupCount - 1) * m_foupSpacing;
}

QString Graph::FoupLevelGraphBuilder::edgeTypeToString(EdgeType type)
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
