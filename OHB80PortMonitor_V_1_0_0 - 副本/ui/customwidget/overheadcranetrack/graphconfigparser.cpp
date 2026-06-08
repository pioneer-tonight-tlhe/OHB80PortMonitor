#include "graphconfigparser.h"
#include "app/applogger.h"
#include "app/shareddata.h"

#include <QFile>
#include <QFileInfo>

Graph::GraphConfigParser::GraphConfigParser(const QString& filePath)
    : m_filePath(filePath),
      m_startNodeFound(false),
      m_fixSpacingValue(10.0),
      m_setNodeIndex(0),
      m_logger(*LoggerManager::getInstance()),
      m_loggerFileName(AppLogger::CraneMapLoggerPath().toStdString()),
      m_stage(ParseStage::NotStarted)
{
    m_config.isUndirected = false;
    m_config.startNodeId = 0;
    m_config.deviceSize = 120;
    m_config.deviceSpacing = 96;
}

Graph::GraphConfigParser::~GraphConfigParser()
{
}

const char* Graph::GraphConfigParser::stageToString(ParseStage stage)
{
    switch (stage) {
    case ParseStage::NotStarted: return "NotStarted";
    case ParseStage::ParsedGraphConfigTag: return "ParsedGraphConfigTag";
    case ParseStage::ParsedEdgesTag: return "ParsedEdgesTag";
    case ParseStage::NodesCollected: return "NodesCollected";
    case ParseStage::EdgeEndpointsCollected: return "EdgeEndpointsCollected";
    case ParseStage::Completed: return "Completed";
    default: return "Unknown";
    }
}

void Graph::GraphConfigParser::setFixSpacingValue(double value)
{
    if (value >= 0) {
        m_fixSpacingValue = value;
    }
}

bool Graph::GraphConfigParser::parse()
{
    std::string loggerPre = "[ui][GraphConfig][parse]";
    m_logger.log(m_loggerFileName, Level::INFO, "====================天车轨道布局图配置文件解析 开始====================");
    
    QFile file(m_filePath);
    QFileInfo fileInfo(file);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}文件无法打开: {}", 
                     loggerPre, fileInfo.fileName().toStdString());
        return false;
    }
    
    QDomDocument doc;
    QString errorMsg;
    int errorLine;
    int errorColumn;
    if (!doc.setContent(&file, &errorMsg, &errorLine, &errorColumn))
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}文件解析失败: {}", 
                     loggerPre, fileInfo.fileName().toStdString());
        m_logger.log(m_loggerFileName, Level::ERROR, "错误信息: {} (行: {}, 列: {})",
                     errorMsg.toStdString(), errorLine, errorColumn);
        file.close();
        return false;
    }
    file.close();
    
    QDomElement root = doc.documentElement();
    
    m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(1): 解析GraphConfig标签开始", loggerPre);

    if (!parseGraphSettings(root))
        return false;
    if (m_stage != ParseStage::ParsedGraphConfigTag) {
        m_stage = ParseStage::ParsedGraphConfigTag;
        m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(1): 解析GraphConfig标签完成", loggerPre);
    }
        
    if (!parseEdges(root))
        return false;
    
    m_stage = ParseStage::Completed;
    m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(5): 配置文件解析完毕", loggerPre);
    m_logger.log(m_loggerFileName, Level::INFO, "{}配置文件解析完成，节点数: {}，边数: {}", 
                 loggerPre, m_config.nodes.size(), m_config.edges.size());
    
    m_logger.log(m_loggerFileName, Level::INFO, "====================天车轨道布局图配置文件解析 结束====================");
    return true;
}

bool Graph::GraphConfigParser::parseGraphSettings(const QDomElement& root)
{
    std::string loggerPre = "[ui][GraphConfig][parseGraphSettings]";
    
    QDomElement settingsEle = root.firstChildElement("GraphSettings");
    if (settingsEle.isNull())
    {
        m_logger.log(m_loggerFileName, Level::WARN, "{}未找到GraphSettings标签", loggerPre);
        return true;
    }
    
    QString isUndirectedText = getElementText(settingsEle, "IsUndirected");
    if (!isUndirectedText.isEmpty())
    {
        m_config.isUndirected = isUndirectedText.toLower() == "true";
        m_logger.log(m_loggerFileName, Level::INFO, "{}图类型: {}", 
                     loggerPre, m_config.isUndirected ? "无向图" : "有向图");
    }
    
    // 解析 DeviceSize 标签
    QDomElement deviceSizeEle = settingsEle.firstChildElement("DeviceSize");
    if (!deviceSizeEle.isNull())
    {
        QString sizeText = getElementText(deviceSizeEle, "Size");
        if (!sizeText.isEmpty())
        {
            bool ok = false;
            int s = sizeText.toInt(&ok);
            if (ok && s > 0) m_config.deviceSize = s;
        }
        QString spacingText = getElementText(deviceSizeEle, "Spacing");
        if (!spacingText.isEmpty())
        {
            bool ok = false;
            int sp = spacingText.toInt(&ok);
            if (ok && sp > 0) m_config.deviceSpacing = sp;
        }
        m_logger.log(m_loggerFileName, Level::INFO, "{}设备控件尺寸: {}, 间距: {}", 
                     loggerPre, m_config.deviceSize, m_config.deviceSpacing);
    }
    
    return true;
}

bool Graph::GraphConfigParser::parseEdges(const QDomElement& root)
{
    std::string loggerPre = "[ui][GraphConfig][parseEdges]";
    
    QDomElement edgesEle = root.firstChildElement("Edges");
    if (edgesEle.isNull())
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}未找到Edges标签", loggerPre);
        return false;
    }
    if (m_stage != ParseStage::ParsedEdgesTag) {
        m_stage = ParseStage::ParsedEdgesTag;
        m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(2): 解析Edges标签", loggerPre);
    }
    
    QDomNodeList edgeList = edgesEle.elementsByTagName("Edge");
    m_logger.log(m_loggerFileName, Level::INFO, "{}找到{}条边数据", loggerPre, edgeList.size());
    
    for (int i = 0; i < edgeList.size(); i++)
    {
        QDomElement edgeEle = edgeList.at(i).toElement();
        if (edgeEle.isNull())
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}第{}个边元素为空", loggerPre, i);
            continue;
        }
        
        QString typeStr = getAttributeText(edgeEle, "type");
        if (typeStr.isEmpty())
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}第{}个边缺少type属性", loggerPre, i);
            continue;
        }
        
        EdgeType edgeType = stringToEdgeType(typeStr);
        if (edgeType == EdgeType::UNKNOWN)
        {
            m_logger.log(m_loggerFileName, Level::WARN, "{}第{}个边的类型'{}'未知", 
                         loggerPre, i, typeStr.toStdString());
        }
        
        QString sizeStr = getElementText(edgeEle, "Size");
        if (sizeStr.isEmpty())
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}第{}个边缺少Size标签", loggerPre, i);
            continue;
        }
        
        double size = 0.0;
        if (!parseSizeValue(sizeStr, size))
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}第{}个边的Size值无效: {}", 
                         loggerPre, i, sizeStr.toStdString());
            continue;
        }
        
        // 解析 FoupLevelOffset 标签（可选，默认为0）
        double foupLevelOffset = 0.0;
        QString foupLevelOffsetStr = getElementText(edgeEle, "FoupLevelOffset");
        if (!foupLevelOffsetStr.isEmpty())
        {
            bool ok = false;
            foupLevelOffset = foupLevelOffsetStr.toDouble(&ok);
            if (!ok)
            {
                m_logger.log(m_loggerFileName, Level::WARN, "{}第{}个边的FoupLevelOffset值无效: {}，使用默认值0", 
                             loggerPre, i, foupLevelOffsetStr.toStdString());
                foupLevelOffset = 0.0;
            }
        }
        
        // 解析 SetLevelOffset 标签（可选，默认为0）
        double setLevelOffset = 0.0;
        QString setLevelOffsetStr = getElementText(edgeEle, "SetLevelOffset");
        if (!setLevelOffsetStr.isEmpty())
        {
            bool ok = false;
            setLevelOffset = setLevelOffsetStr.toDouble(&ok);
            if (!ok)
            {
                m_logger.log(m_loggerFileName, Level::WARN, "{}第{}个边的SetLevelOffset值无效: {}，使用默认值0", 
                             loggerPre, i, setLevelOffsetStr.toStdString());
                setLevelOffset = 0.0;
            }
        }
        
        QDomNodeList nodeList = edgeEle.elementsByTagName("Node");
        if (nodeList.size() != 2)
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}第{}个边必须有2个Node节点，实际找到{}个", 
                         loggerPre, i, nodeList.size());
            continue;
        }
        
        QDomElement sourceNodeEle = nodeList.at(0).toElement();
        QDomElement targetNodeEle = nodeList.at(1).toElement();
        
        int sourceId = getAttributeInt(sourceNodeEle, "id", -1);
        if (sourceId < 0)
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}第{}个边的起始节点ID无效", loggerPre, i);
            continue;
        }
        
        int targetId = getAttributeInt(targetNodeEle, "id", -1);
        if (targetId < 0)
        {
            m_logger.log(m_loggerFileName, Level::ERROR, "{}第{}个边的目标节点ID无效", loggerPre, i);
            continue;
        }
        
        if (!m_startNodeFound)
        {
            bool isStartNode = getAttributeText(sourceNodeEle, "start").toLower() == "true";
            if (isStartNode)
            {
                m_config.startNodeId = sourceId;
                m_startNodeFound = true;
            }
        }
        
        bool hadSource = m_config.nodes.contains(sourceId);
        bool hadTarget = m_config.nodes.contains(targetId);
        processNode(sourceNodeEle);
        processNode(targetNodeEle);
        
        bool addedSource = !hadSource && m_config.nodes.contains(sourceId);
        bool addedTarget = !hadTarget && m_config.nodes.contains(targetId);
        
        if (addedSource || addedTarget) {
            if (m_stage != ParseStage::NodesCollected) {
                m_stage = ParseStage::NodesCollected;
            }
        }
        
        if (addedSource) {
            QString sourceType = getAttributeText(sourceNodeEle, "type");
            if (sourceType == "SET") {
                QString firstFoupQRCode = getElementText(sourceNodeEle, "FirstFoupQRCode");
                QString foupCountStr = getElementText(sourceNodeEle, "FoupCount");
                QString positionStr = getElementText(sourceNodeEle, "Position");
                int foupCount = foupCountStr.toInt();
                m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(3):收集SET节点: ID={}, QRCode={}, Count={}, Position={}", 
                             loggerPre, sourceId, firstFoupQRCode.toStdString(), foupCount, positionStr.toStdString());
            } else if (sourceType == "TRACK") {
                m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(3):收集TRACK节点: ID={}", loggerPre, sourceId);
            }
        }
        
        if (addedTarget) {
            QString targetType = getAttributeText(targetNodeEle, "type");
            if (targetType == "SET") {
                QString firstFoupQRCode = getElementText(targetNodeEle, "FirstFoupQRCode");
                QString foupCountStr = getElementText(targetNodeEle, "FoupCount");
                QString positionStr = getElementText(targetNodeEle, "Position");
                int foupCount = foupCountStr.toInt();
                m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(3):收集SET节点: ID={}, QRCode={}, Count={}, Position={}", 
                             loggerPre, targetId, firstFoupQRCode.toStdString(), foupCount, positionStr.toStdString());
            } else if (targetType == "TRACK") {
                m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(3):收集TRACK节点: ID={}", loggerPre, targetId);
            }
        }
        
        GraphConfig::EdgeInfo edgeInfo;
        edgeInfo.sourceId = sourceId;
        edgeInfo.targetId = targetId;
        edgeInfo.type = edgeType;
        edgeInfo.size = size;
        edgeInfo.foupLevelOffset = foupLevelOffset;
        edgeInfo.setLevelOffset = setLevelOffset;
        m_config.edges.append(edgeInfo);
        if (m_stage != ParseStage::EdgeEndpointsCollected) {
            m_stage = ParseStage::EdgeEndpointsCollected;
        }
        
        if (foupLevelOffset != 0.0 || setLevelOffset != 0.0) {
            m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(4):成功解析边: {} -> {}，类型: {}，大小: {}，FoupLevel偏移: {}，SetLevel偏移: {}", 
                         loggerPre, sourceId, targetId, typeStr.toStdString(), size, foupLevelOffset, setLevelOffset);
        } else {
            m_logger.log(m_loggerFileName, Level::INFO, "{}解析阶段(4):成功解析边: {} -> {}，类型: {}，大小: {}", 
                         loggerPre, sourceId, targetId, typeStr.toStdString(), size);
        }
    }
    
    return true;
}

void Graph::GraphConfigParser::processNode(const QDomElement& nodeEle)
{
    std::string loggerPre = "[ui][GraphConfig][processNode]";
    
    int nodeId = getAttributeInt(nodeEle, "id", -1);
    QString nodeType = getAttributeText(nodeEle, "type");
    
    if (nodeId < 0 || nodeType.isEmpty())
    {
        m_logger.log(m_loggerFileName, Level::ERROR, "{}节点缺少必要属性: ID={}, Type={}", 
                     loggerPre, nodeId, nodeType.toStdString());
        return;
    }
    
    if (m_config.nodes.contains(nodeId))
        return;
    
    QSharedPointer<GraphNode> node;
    
    if (nodeType == "SET")
    {
        QString firstFoupQRCode = getElementText(nodeEle, "FirstFoupQRCode");
        QString foupCountStr = getElementText(nodeEle, "FoupCount");
        int foupCount = foupCountStr.toInt();
        
        // 解析Position属性
        QString positionStr = getElementText(nodeEle, "Position");
        SetPosition position = SetPosition::ABOVE; // 默认为上方
        if (positionStr == "BELOW") {
            position = SetPosition::BELOW;
        } else if (positionStr == "ABOVE") {
            position = SetPosition::ABOVE;
        }
        
        
        // 创建SET节点对象（带位置参数）
        node = QSharedPointer<SetOfOHBNode>::create(firstFoupQRCode, foupCount, nodeId, position);

        // 按文档顺序将 SET 节点 ID 设置为对应 SetOfOHBInfo 的 uiId
        if (m_setNodeIndex < SharedData::setOfOHBInfoList.size()) {
            SharedData::setOfOHBInfoList[m_setNodeIndex].setUiId(nodeId);
            m_logger.log(m_loggerFileName, Level::INFO,
                         "{}SET节点文档顺序#{}: 设置 setOfOHBInfoList[{}].uiId = {}",
                         loggerPre, m_setNodeIndex, m_setNodeIndex, nodeId);
            m_setNodeIndex++;
        }
    }
    else if (nodeType == "TRACK")
    {
        
        // 创建TRACK节点对象
        node = QSharedPointer<GraphNode>::create(NodeType::TRACK, nodeId);
    }
    else
    {
        m_logger.log(m_loggerFileName, Level::WARN, "{}未知节点类型: {}, ID={}", 
                     loggerPre, nodeType.toStdString(), nodeId);
        return;
    }
    
    // 添加节点到配置
    m_config.nodes[nodeId] = node;
    // 记录节点ID顺序
    m_config.nodeIdOrder.append(nodeId);
}

QString Graph::GraphConfigParser::getElementText(const QDomElement& parent, const QString& tagName)
{
    QDomElement element = parent.firstChildElement(tagName);
    return element.isNull() ? QString() : element.text().trimmed();
}

QString Graph::GraphConfigParser::getAttributeText(const QDomElement& element, const QString& attrName)
{
    return element.attribute(attrName).trimmed();
}

bool Graph::GraphConfigParser::parseSizeValue(const QString& sizeStr, double& size) const
{
    if (sizeStr.compare("FixSpcaing", Qt::CaseInsensitive) == 0)
    {
        size = m_fixSpacingValue;
        return true;
    }

    bool sizeOk = false;
    double parsedValue = sizeStr.toDouble(&sizeOk);
    if (!sizeOk || parsedValue < 0)
    {
        return false;
    }

    // Size值 = 倍数 * (deviceSize + deviceSpacing)
    double unit = m_config.deviceSize + m_config.deviceSpacing;
    size = parsedValue * unit;
    return true;
}

int Graph::GraphConfigParser::getAttributeInt(const QDomElement& element, const QString& attrName, int defaultValue)
{
    QString text = getAttributeText(element, attrName);
    bool ok = false;
    int value = text.toInt(&ok);
    return ok ? value : defaultValue;
}
