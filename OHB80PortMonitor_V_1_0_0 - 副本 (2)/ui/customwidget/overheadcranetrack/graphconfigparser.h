#ifndef GRAPHCONFIGPARSER_H
#define GRAPHCONFIGPARSER_H

// ============================================================
// GraphConfigParser - 图配置 XML 解析器
//
// 负责解析 graph_config.xml 配置文件，提取图的基本设置、
// 节点定义和边定义，构建 GraphConfig 数据结构供布局引擎使用。
// ============================================================

#include "graphnode.h"
#include "graphedge.h"
#include "loggermanager.h"

#include <QDomDocument>
#include <QDomElement>
#include <QString>
#include <QMap>
#include <QList>

namespace Graph {

/// 图配置数据结构，保存解析后的完整图信息
struct GraphConfig
{
    bool isUndirected;       ///< 是否为无向图
    int startNodeId;         ///< 起始节点 ID（XML 中 start="true" 的节点）
    int deviceSize;          ///< 设备控件尺寸（像素），默认 120
    int deviceSpacing;       ///< 设备控件间距（像素），默认 96
    QMap<int, QSharedPointer<GraphNode>> nodes;  ///< 节点集合，key 为节点 ID
    QList<int> nodeIdOrder;  ///< 节点 ID 的出现顺序（按 XML 中定义顺序）
    
    /// 边信息，描述两个节点之间的连接关系
    struct EdgeInfo {
        int sourceId;            ///< 源节点 ID
        int targetId;            ///< 目标节点 ID
        EdgeType type;           ///< 边类型（直线、曲线、虚拟线等）
        double size;             ///< 边的尺寸系数
        double foupLevelOffset;  ///< FoupLevel 视图下边长度的偏移值（像素），默认为 0
        double setLevelOffset;   ///< SetLevel 视图下边长度的偏移值（像素），默认为 0
    };
    QList<EdgeInfo> edges;    ///< 边集合
};

/// XML 图配置解析器，将 graph_config.xml 解析为 GraphConfig 结构
class GraphConfigParser
{
public:
    /// 解析阶段枚举，用于跟踪解析进度和错误定位
    enum class ParseStage {
        NotStarted = 0,              ///< 尚未开始
        ParsedGraphConfigTag = 1,    ///< 已解析 <GraphConfig> 根标签
        ParsedEdgesTag = 2,          ///< 已解析 <Edges> 标签
        NodesCollected = 3,          ///< 收集节点定义
        EdgeEndpointsCollected = 4,  ///< 收集边的端点关系
        Completed = 5                ///< 解析完成
    };

    /// @param filePath graph_config.xml 文件路径
    explicit GraphConfigParser(const QString& filePath);
    ~GraphConfigParser();
    
    /// 执行解析，成功返回 true
    bool parse();

    /// 设置 "FixSpacing" 关键字对应的固定间距值（像素）
    void setFixSpacingValue(double value);
    
    /// 获取解析结果
    const GraphConfig& getConfig() const { return m_config; }

    /// 获取当前解析阶段
    ParseStage getStage() const { return m_stage; }

    /// 将解析阶段转为可读字符串（用于日志）
    static const char* stageToString(ParseStage stage);
    
private:
    bool parseGraphSettings(const QDomElement& root);   ///< 解析 <GraphSettings> 节
    bool parseEdges(const QDomElement& root);            ///< 解析 <Edges> 节中的所有边
    void processNode(const QDomElement& nodeEle);        ///< 处理单个 <Node> 元素，提取节点信息
    bool parseSizeValue(const QString& sizeStr, double& size) const; ///< 解析 Size 值（支持 "FixSpacing" 关键字）
    
    // ---------- XML 辅助方法 ----------
    QString getElementText(const QDomElement& parent, const QString& tagName);
    QString getAttributeText(const QDomElement& element, const QString& attrName);
    int getAttributeInt(const QDomElement& element, const QString& attrName, int defaultValue = 0);
    
private:
    QString m_filePath;          ///< XML 配置文件路径
    GraphConfig m_config;        ///< 解析结果
    bool m_startNodeFound;       ///< 是否已找到起始节点
    double m_fixSpacingValue;    ///< "FixSpacing" 对应的固定间距值
    int m_setNodeIndex;          ///< SET 节点计数器，按文档顺序设置 SharedData 中的 uiId
    
    LoggerManager& m_logger;     ///< 日志管理器引用
    std::string m_loggerFileName;///< 日志文件名
    ParseStage m_stage;          ///< 当前解析阶段
};

} // namespace Graph

#endif // GRAPHCONFIGPARSER_H
