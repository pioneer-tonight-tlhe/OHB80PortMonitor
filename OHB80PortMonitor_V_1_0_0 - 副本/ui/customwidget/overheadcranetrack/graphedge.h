#ifndef GRAPHEDGE_H
#define GRAPHEDGE_H

#include <QString>
#include <QSharedPointer>
#include <iostream>
#include <unordered_map>

namespace Graph {
// 边类型枚举
enum class EdgeType {
    LEFT_SEMICIRCLE_ARC,        // 弧顶朝左半圆弧
    RIGHT_SEMICIRCLE_ARC,       // 弧顶朝右半圆弧
    LEFT_SEMICIRCLE_ARC_2,      // 弧顶朝左半圆弧_2
    RIGHT_SEMICIRCLE_ARC_2,     // 弧顶朝右半圆弧_2
    S_CURVE,                    // S型曲线
    REVERSE_S_CURVE,            // 反向S型曲线
    STRAIGHT_LINE,              // 直线
    HORIZONTAL_VIRTUAL_LINE,    // 水平虚线（向右绘制，只改变终端节点的x轴）
    VERTICAL_VIRTUAL_LINE,      // 垂直虚线（向下绘制，只改变终端节点的y轴）
    UNKNOWN                     // 未知类型
};

// 图边类
class GraphEdge
{
public:
    // 默认构造函数
    GraphEdge()
        : edgeType(EdgeType::UNKNOWN)
        , size(0.0)
    {
    }
    
    // 带参数构造函数
    GraphEdge(EdgeType type, double edgeSize)
        : edgeType(type)
        , size(edgeSize)
    {
    }
    
    // 拷贝构造函数
    GraphEdge(const GraphEdge& other)
        : edgeType(other.edgeType)
        , size(other.size)
    {
    }
    
    // 赋值运算符
    GraphEdge& operator=(const GraphEdge& other)
    {
        if (this != &other)
        {
            edgeType = other.edgeType;
            size = other.size;
        }
        return *this;
    }
    
    // 析构函数
    ~GraphEdge() = default;
    
    // Getter 方法
    EdgeType getEdgeType() const { return edgeType; }
    double getSize() const { return size; }
    
    // Setter 方法
    void setEdgeType(EdgeType type) { edgeType = type; }
    void setSize(double edgeSize) { size = edgeSize; }
    
    // 比较运算符
    bool operator==(const GraphEdge& other) const
    {
        return edgeType == other.edgeType && size == other.size;
    }
    
    bool operator!=(const GraphEdge& other) const
    {
        return !(*this == other);
    }
    
    // 用于输出流（调试用）
    friend std::ostream& operator<<(std::ostream& os, const GraphEdge& edge)
    {
        os << "GraphEdge[Type=" << static_cast<int>(edge.edgeType) 
           << ", Size=" << edge.size << "]";
        return os;
    }
    
public:
    EdgeType edgeType;  // 边类型
    double size;        // 边的尺寸
};

}

// 全局函数：将 EdgeType 枚举转换为字符串
inline QString edgeTypeToString(Graph::EdgeType type)
{
    switch (type) {
    case Graph::EdgeType::LEFT_SEMICIRCLE_ARC:
        return "LEFT_SEMICIRCLE_ARC";
    case Graph::EdgeType::RIGHT_SEMICIRCLE_ARC:
        return "RIGHT_SEMICIRCLE_ARC";
    case Graph::EdgeType::LEFT_SEMICIRCLE_ARC_2:
        return "LEFT_SEMICIRCLE_ARC_2";
    case Graph::EdgeType::RIGHT_SEMICIRCLE_ARC_2:
        return "RIGHT_SEMICIRCLE_ARC_2";
    case Graph::EdgeType::S_CURVE:
        return "S_CURVE";
    case Graph::EdgeType::REVERSE_S_CURVE:
        return "REVERSE_S_CURVE";
    case Graph::EdgeType::STRAIGHT_LINE:
        return "STRAIGHT_LINE";
    case Graph::EdgeType::HORIZONTAL_VIRTUAL_LINE:
        return "HORIZONTAL_VIRTUAL_LINE";
    case Graph::EdgeType::VERTICAL_VIRTUAL_LINE:
        return "VERTICAL_VIRTUAL_LINE";
    case Graph::EdgeType::UNKNOWN:
    default:
        return "UNKNOWN";
    }
}

// 全局函数：将字符串转换为 EdgeType 枚举
inline Graph::EdgeType stringToEdgeType(const QString& str)
{
    static const std::unordered_map<QString, Graph::EdgeType> stringToTypeMap = {
        {"LEFT_SEMICIRCLE_ARC", Graph::EdgeType::LEFT_SEMICIRCLE_ARC},
        {"RIGHT_SEMICIRCLE_ARC", Graph::EdgeType::RIGHT_SEMICIRCLE_ARC},
        {"LEFT_SEMICIRCLE_ARC_2", Graph::EdgeType::LEFT_SEMICIRCLE_ARC_2},
        {"RIGHT_SEMICIRCLE_ARC_2", Graph::EdgeType::RIGHT_SEMICIRCLE_ARC_2},
        {"S_CURVE", Graph::EdgeType::S_CURVE},
        {"REVERSE_S_CURVE", Graph::EdgeType::REVERSE_S_CURVE},
        {"STRAIGHT_LINE", Graph::EdgeType::STRAIGHT_LINE},
        {"HORIZONTAL_VIRTUAL_LINE", Graph::EdgeType::HORIZONTAL_VIRTUAL_LINE},
        {"VERTICAL_VIRTUAL_LINE", Graph::EdgeType::VERTICAL_VIRTUAL_LINE},
        {"UNKNOWN", Graph::EdgeType::UNKNOWN}
    };
    
    auto it = stringToTypeMap.find(str);
    if (it != stringToTypeMap.end()) {
        return it->second;
    }
    return Graph::EdgeType::UNKNOWN;
}

// Qt元类型声明（QSharedPointer的特定实例化需要声明）
Q_DECLARE_METATYPE(QSharedPointer<Graph::GraphEdge>)

#endif // GRAPHEDGE_H
