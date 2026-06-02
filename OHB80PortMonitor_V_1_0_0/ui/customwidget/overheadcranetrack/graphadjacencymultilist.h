#ifndef GRAPHADJACENCYMULTILIST_H
#define GRAPHADJACENCYMULTILIST_H

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <sstream>
#include <memory>
#include <functional>

// 前置声明命名空间
namespace Graph {
    class GraphNode;
    class GraphEdge;
}

#include "graphnode.h"
#include "graphedge.h"
#include "loggermanager.h"
#include "app/applogger.h"

namespace Graph
{

// ===================== 邻接多重表的边节点 =====================
// 每条边只存储一次，通过ilink和jlink串联到两个端点的边链中
template <typename VertexType, typename EdgeType>
struct MultilistEdgeNode
{
    VertexType ivex;                    // 边的端点i
    VertexType jvex;                    // 边的端点j
    EdgeType edgeData;                  // 边数据
    MultilistEdgeNode* ilink;           // 指向依附于ivex的下一条边
    MultilistEdgeNode* jlink;           // 指向依附于jvex的下一条边

    MultilistEdgeNode()
        : ivex(), jvex(), edgeData(), ilink(nullptr), jlink(nullptr)
    {
    }

    MultilistEdgeNode(const VertexType& i, const VertexType& j, const EdgeType& edge)
        : ivex(i), jvex(j), edgeData(edge), ilink(nullptr), jlink(nullptr)
    {
    }
};

// ===================== 邻接多重表的顶点节点 =====================
template <typename VertexType, typename EdgeType>
struct MultilistVertexNode
{
    VertexType nodeData;                                    // 顶点数据
    MultilistEdgeNode<VertexType, EdgeType>* firstEdge;     // 指向依附于该顶点的第一条边

    MultilistVertexNode()
        : nodeData(), firstEdge(nullptr)
    {
    }

    explicit MultilistVertexNode(const VertexType& node)
        : nodeData(node), firstEdge(nullptr)
    {
    }
};

// ===================== 邻接多重表（模板类） =====================
template <typename VertexType, typename EdgeType>
class GraphAdjacencyMultilist
{
public:
    GraphAdjacencyMultilist();
    ~GraphAdjacencyMultilist();

    // ========== 顶点操作 ==========
    // 添加顶点
    void addVertex(const VertexType& vertex);

    // 检查顶点是否存在
    bool hasVertex(const VertexType& vertex) const;

    // 获取所有顶点
    QList<VertexType> getAllVertices() const;

    // ========== 边操作 ==========
    // 添加边（无向边，只存储一次）
    bool addEdge(const VertexType& from, const VertexType& to, const EdgeType& edge);

    // 检查边是否存在
    bool hasEdge(const VertexType& from, const VertexType& to) const;

    // 获取指定顶点的所有关联边节点
    QList<MultilistEdgeNode<VertexType, EdgeType>*> getEdgesOfVertex(const VertexType& vertex) const;

    // ========== 核心查询方法 ==========
    // 根据起始节点和边，返回对应的目的节点
    VertexType getTargetNode(const VertexType& startVertex, const EdgeType& edge) const;

    // 获取指定顶点的所有邻居节点
    QList<VertexType> getNeighbors(const VertexType& vertex) const;

    // ========== 回调函数设置 ==========
    // 设置：判断两条边是否相同的函数
    void setEdgeCompareFunc(std::function<bool(const EdgeType&, const EdgeType&)> func);

    // ========== 信息查询 ==========
    // 获取顶点数量
    int vertexCount() const;

    // 获取边数量
    int edgeCount() const;

    // 打印邻接多重表（调试用）
    void printMultilist() const;

    // 清空
    void clear();

private:
    // 顶点表：key=顶点，value=顶点节点
    QHash<VertexType, MultilistVertexNode<VertexType, EdgeType>> m_vertices;

    // 顶点插入顺序
    QList<VertexType> m_vertexOrder;

    // 所有边节点（用于内存管理）
    QList<MultilistEdgeNode<VertexType, EdgeType>*> m_allEdges;

    // 回调函数：判断两条边是否相同
    std::function<bool(const EdgeType&, const EdgeType&)> m_edgeCompare;

    // 日志
    LoggerManager& m_logger;
    std::string m_loggerFileName;
};

// ===================== 模板类实现（必须放在头文件中） =====================

template <typename VertexType, typename EdgeType>
GraphAdjacencyMultilist<VertexType, EdgeType>::GraphAdjacencyMultilist()
    : m_logger(*LoggerManager::getInstance())
    , m_loggerFileName(AppLogger::CraneMapLoggerPath().toStdString())
{
}

template <typename VertexType, typename EdgeType>
GraphAdjacencyMultilist<VertexType, EdgeType>::~GraphAdjacencyMultilist()
{
    clear();
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyMultilist<VertexType, EdgeType>::addVertex(const VertexType& vertex)
{
    if (m_vertices.contains(vertex))
    {
        std::ostringstream oss;
        oss << vertex;
        m_logger.log(m_loggerFileName, Level::DEBUG,
                     "[ui][Graph][addVertex] 顶点 {} 已存在，跳过", oss.str());
        return;
    }

    m_vertices[vertex] = MultilistVertexNode<VertexType, EdgeType>(vertex);
    m_vertexOrder.append(vertex);
    std::ostringstream oss;
    oss << vertex;
    m_logger.log(m_loggerFileName, Level::DEBUG,
                 "[ui][Graph][addVertex] 添加顶点 {}", oss.str());
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyMultilist<VertexType, EdgeType>::hasVertex(const VertexType& vertex) const
{
    return m_vertices.contains(vertex);
}

template <typename VertexType, typename EdgeType>
QList<VertexType> GraphAdjacencyMultilist<VertexType, EdgeType>::getAllVertices() const
{
    return m_vertexOrder;
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyMultilist<VertexType, EdgeType>::addEdge(const VertexType& from, const VertexType& to, const EdgeType& edge)
{
    // 自动添加不存在的顶点
    addVertex(from);
    addVertex(to);

    // 检查边是否已存在
    if (hasEdge(from, to))
    {
        std::ostringstream oss1, oss2;
        oss1 << from;
        oss2 << to;
        m_logger.log(m_loggerFileName, Level::DEBUG,
                     "[ui][Graph][addEdge] 边 ({},{}) 已存在，跳过",
                     oss1.str(), oss2.str());
        return false;
    }

    // 创建边节点
    auto* edgeNode = new MultilistEdgeNode<VertexType, EdgeType>(from, to, edge);
    m_allEdges.append(edgeNode);

    // 将边节点插入到ivex（from）的边链头部
    auto& fromVertex = m_vertices[from];
    edgeNode->ilink = fromVertex.firstEdge;
    fromVertex.firstEdge = edgeNode;

    // 将边节点插入到jvex（to）的边链头部（无向边，同时属于两个顶点）
    if (!(from == to))
    {
        auto& toVertex = m_vertices[to];
        edgeNode->jlink = toVertex.firstEdge;
        toVertex.firstEdge = edgeNode;
    }

    std::ostringstream oss1, oss2;
    oss1 << from;
    oss2 << to;
    m_logger.log(m_loggerFileName, Level::DEBUG,
                 "[ui][Graph][addEdge] 添加边 ({},{})",
                 oss1.str(), oss2.str());
    return true;
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyMultilist<VertexType, EdgeType>::hasEdge(const VertexType& from, const VertexType& to) const
{
    if (!hasVertex(from)) return false;

    const auto& vertex = m_vertices[from];
    auto* p = vertex.firstEdge;

    while (p)
    {
        if ((p->ivex == from && p->jvex == to) ||
            (p->ivex == to && p->jvex == from))
        {
            return true;
        }

        // 沿边链遍历
        if (p->ivex == from)
            p = p->ilink;
        else
            p = p->jlink;
    }
    return false;
}

template <typename VertexType, typename EdgeType>
QList<MultilistEdgeNode<VertexType, EdgeType>*>
GraphAdjacencyMultilist<VertexType, EdgeType>::getEdgesOfVertex(const VertexType& vertex) const
{
    QList<MultilistEdgeNode<VertexType, EdgeType>*> result;
    if (!hasVertex(vertex)) return result;

    const auto& vNode = m_vertices[vertex];
    auto* p = vNode.firstEdge;

    while (p)
    {
        result.append(p);

        // 沿边链遍历
        if (p->ivex == vertex)
            p = p->ilink;
        else
            p = p->jlink;
    }
    return result;
}

template <typename VertexType, typename EdgeType>
VertexType GraphAdjacencyMultilist<VertexType, EdgeType>::getTargetNode(
    const VertexType& startVertex, const EdgeType& edge) const
{
    if (!hasVertex(startVertex)) return VertexType();

    const auto& vertex = m_vertices[startVertex];
    auto* p = vertex.firstEdge;

    while (p)
    {
        // 匹配边数据
        bool matched = false;
        if (m_edgeCompare)
        {
            matched = m_edgeCompare(p->edgeData, edge);
        }
        else
        {
            matched = (p->edgeData == edge);
        }

        if (matched)
        {
            // 确定目的节点：如果起始节点是ivex，则目的是jvex，反之亦然
            return (p->ivex == startVertex) ? p->jvex : p->ivex;
        }

        // 沿边链遍历
        if (p->ivex == startVertex)
            p = p->ilink;
        else
            p = p->jlink;
    }

    std::ostringstream oss;
    oss << startVertex;
    m_logger.log(m_loggerFileName, Level::DEBUG,
                 "[ui][Graph][getTargetNode] 从顶点 {} 未找到匹配的边",
                 oss.str());
    return VertexType();
}

template <typename VertexType, typename EdgeType>
QList<VertexType> GraphAdjacencyMultilist<VertexType, EdgeType>::getNeighbors(const VertexType& vertex) const
{
    QList<VertexType> result;
    if (!hasVertex(vertex)) return result;

    const auto& vNode = m_vertices[vertex];
    auto* p = vNode.firstEdge;

    while (p)
    {
        VertexType neighbor = (p->ivex == vertex) ? p->jvex : p->ivex;
        if (!result.contains(neighbor))
        {
            result.append(neighbor);
        }

        // 沿边链遍历
        if (p->ivex == vertex)
            p = p->ilink;
        else
            p = p->jlink;
    }
    return result;
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyMultilist<VertexType, EdgeType>::setEdgeCompareFunc(
    std::function<bool(const EdgeType&, const EdgeType&)> func)
{
    m_edgeCompare = func;
}

template <typename VertexType, typename EdgeType>
int GraphAdjacencyMultilist<VertexType, EdgeType>::vertexCount() const
{
    return m_vertices.size();
}

template <typename VertexType, typename EdgeType>
int GraphAdjacencyMultilist<VertexType, EdgeType>::edgeCount() const
{
    return m_allEdges.size();
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyMultilist<VertexType, EdgeType>::printMultilist() const
{
    m_logger.log(m_loggerFileName, Level::DEBUG,
                 "[ui][Graph][printMultilist] ========== 邻接多重表 ==========");
    m_logger.log(m_loggerFileName, Level::DEBUG,
                 "[ui][Graph][printMultilist] 顶点数: {}，边数: {}",
                 vertexCount(), edgeCount());

    for (const VertexType& v : m_vertexOrder)
    {
        if (!m_vertices.contains(v)) continue;

        const auto& vNode = m_vertices[v];
        std::ostringstream ossVertex;
        ossVertex << v;
        std::string line = "[" + ossVertex.str() + "] -> ";

        auto* p = vNode.firstEdge;
        while (p)
        {
            VertexType neighbor = (p->ivex == v) ? p->jvex : p->ivex;
            std::ostringstream ossNeighbor, ossEdge;
            ossNeighbor << neighbor;
            ossEdge << *(p->edgeData);
            line += "(" + ossNeighbor.str() + ", " + ossEdge.str() + ") ";

            if (p->ivex == v)
                p = p->ilink;
            else
                p = p->jlink;
        }

        m_logger.log(m_loggerFileName, Level::DEBUG,
                     "[ui][Graph][printMultilist] {}", line);
    }
    m_logger.log(m_loggerFileName, Level::DEBUG,
                 "[ui][Graph][printMultilist] ====================================");
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyMultilist<VertexType, EdgeType>::clear()
{
    // 释放所有边节点的内存
    for (auto* edge : m_allEdges)
    {
        delete edge;
    }
    m_allEdges.clear();
    m_vertices.clear();
    m_vertexOrder.clear();
}

} // namespace Graph

#endif // GRAPHADJACENCYMULTILIST_H
