#ifndef GRAPHNODE_H
#define GRAPHNODE_H

#include <QString>
#include <QSharedPointer>

namespace Graph
{
// 节点类型枚举
enum class NodeType {
    SET,            // Set 节点
    TRACK,          // Track 节点
    UNKNOWN         // 未知类型
};

// SET节点位置枚举（相对于轨道）
enum class SetPosition {
    ABOVE,          // 轨道上方
    BELOW           // 轨道下方
};

// 图节点基类
class GraphNode
{
public:
    // 默认构造函数（父类默认轨道节点，仅仅起到绘图作用）
    GraphNode()
        : nodeType(NodeType::TRACK)
        , id(-1)
        , position(SetPosition::ABOVE)
        , distanceFromTrack(10.0)
    {
    }
    
    // 带参数构造函数
    GraphNode(NodeType type, int nodeId = -1)
        : nodeType(type)
        , id(nodeId)
        , position(SetPosition::ABOVE)
        , distanceFromTrack(10.0)
    {
    }
    
    // 拷贝构造函数
    GraphNode(const GraphNode& other)
        : nodeType(other.nodeType)
        , id(other.id)
        , position(other.position)
        , distanceFromTrack(other.distanceFromTrack)
    {
    }
    
    // 赋值运算符
    GraphNode& operator=(const GraphNode& other)
    {
        if (this != &other)
        {
            nodeType = other.nodeType;
            id = other.id;
            position = other.position;
            distanceFromTrack = other.distanceFromTrack;
        }
        return *this;
    }
    
    // 虚析构函数（使类成为多态类型，支持dynamic_cast）
    virtual ~GraphNode() = default;
    
    // Getter 方法
    NodeType getNodeType() const { return nodeType; }
    int getId() const { return id; }
    SetPosition getPosition() const { return position; }
    double getDistanceFromTrack() const { return distanceFromTrack; }
    
    // Setter 方法
    void setNodeType(NodeType type) { nodeType = type; }
    void setId(int nodeId) { id = nodeId; }
    void setPosition(SetPosition pos) { position = pos; }
    void setDistanceFromTrack(double distance) { distanceFromTrack = distance; }
    
    
public:
    NodeType nodeType;  // 节点类型
    int id;
    SetPosition position;  // SET节点位置（轨道上方/下方）
    double distanceFromTrack;  // 距离轨道的固定值（默认10）
};

class SetOfOHBNode : public GraphNode
{
public:
    // 构造函数（默认）
    SetOfOHBNode()
        : GraphNode(NodeType::SET)
        , firstFoupQRCode()
        , foupCount(0)
        , uiId(0)
    {
    }

    // 构造函数（带数据和ID）
    SetOfOHBNode(const QString& firstFoupQRCode, int foupCount, int nodeId = -1, 
                 SetPosition pos = SetPosition::ABOVE, int uiId = 0)
        : GraphNode(NodeType::SET, nodeId)
        , firstFoupQRCode(firstFoupQRCode)
        , foupCount(foupCount)
        , uiId(uiId)
    {
        position = pos;
    }

    // 拷贝构造函数
    SetOfOHBNode(const SetOfOHBNode& other)
        : GraphNode(other)
        , firstFoupQRCode(other.firstFoupQRCode)
        , foupCount(other.foupCount)
        , uiId(other.uiId)
    {
    }

    // 拷贝赋值运算符
    SetOfOHBNode& operator=(const SetOfOHBNode& other)
    {
        if (this != &other)
        {
            GraphNode::operator=(other);
            firstFoupQRCode = other.firstFoupQRCode;
            foupCount = other.foupCount;
            uiId = other.uiId;
        }
        return *this;
    }

    QString getFirstFoupQRCode() const { return firstFoupQRCode; }
    int getFoupCount() const { return foupCount; }
    int getUiId() const { return uiId; }

    void setFirstFoupQRCode(const QString& qrCode) { firstFoupQRCode = qrCode; }
    void setFoupCount(int count) { foupCount = count; }
    void setUiId(int id) { uiId = id; }

private:
    QString firstFoupQRCode;
    int foupCount;
    int uiId;  // UI ID，用于关联 SetOfOHBInfo 对象
};

}

// Qt元类型声明（QSharedPointer的特定实例化需要声明）
Q_DECLARE_METATYPE(QSharedPointer<Graph::GraphNode>)
Q_DECLARE_METATYPE(QSharedPointer<Graph::SetOfOHBNode>)

// 为QSharedPointer<GraphNode>提供哈希函数
inline uint qHash(const QSharedPointer<Graph::GraphNode>& key, uint seed = 0)
{
    if (!key) {
        return ::qHash(0, seed);
    }
    return ::qHash(key->getId(), seed);
}

// 为QSharedPointer<GraphNode>提供相等比较
inline bool operator==(const QSharedPointer<Graph::GraphNode>& lhs, const QSharedPointer<Graph::GraphNode>& rhs)
{
    if (!lhs && !rhs) return true;
    if (!lhs || !rhs) return false;
    return lhs->getId() == rhs->getId();
}

// 为输出流提供支持
inline std::ostream& operator<<(std::ostream& os, const QSharedPointer<Graph::GraphNode>& node)
{
    if (node) {
        os << node->getId();
    } else {
        os << "null";
    }
    return os;
}

#endif // GRAPHNODE_H
