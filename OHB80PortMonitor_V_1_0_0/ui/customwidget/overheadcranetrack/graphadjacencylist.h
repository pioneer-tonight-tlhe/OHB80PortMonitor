#ifndef GRAPHADJACENCYLIST_H
#define GRAPHADJACENCYLIST_H

#include <QHash>
#include <QList>
#include <QSet>
#include <QDebug>
#include <QString>
#include <functional>
#include <sstream>
#include <memory>

// 前置声明命名空间
namespace Graph {
    class GraphNode;
    class GraphEdge;
}

// 在使用QHash之前包含哈希函数定义
#include "graphnode.h"
#include "graphedge.h"
#include "loggermanager.h"
#include "app/applogger.h"

namespace Graph
{

// ===================== 通用模板化邻接表 =====================
template <typename VertexType, typename EdgeType>
class GraphAdjacencyList {
public:
    // 构造函数：指定是否为无向图（默认有向图）
    GraphAdjacencyList(bool isUndirected = false) : m_isUndirected(isUndirected) {}

    // ========== 核心操作：顶点管理 ==========
    // 添加顶点
    void addVertex(const VertexType& vertex);

    // 删除顶点（同时删除所有关联的边）
    bool removeVertex(const VertexType& vertex);

    // 检查顶点是否存在
    bool hasVertex(const VertexType& vertex) const;

    // 获取所有顶点
    QList<VertexType> getAllVertices() const;

    // ========== 核心操作：边管理 ==========
    // 添加边（from -> to）
    bool addEdge(const VertexType& from, const VertexType& to, const EdgeType& edge);

    // 删除边（from -> to）
    bool removeEdge(const VertexType& from, const VertexType& to);

    // 检查边是否存在
    bool hasEdge(const VertexType& from, const VertexType& to) const;

    // ========== 回调函数设置（关键：适配自定义边/顶点类型） ==========
    // 设置：从边对象中获取目标顶点的函数
    void setGetEdgeTargetFunc(std::function<VertexType(const EdgeType&)> func);

    // 设置：判断边（from->to）是否存在的函数
    void setIsEdgeExistFunc(std::function<bool(const VertexType&, const VertexType&)> func);

    // 设置：为无向图创建反向边的函数
    void setCreateReverseEdgeFunc(std::function<EdgeType(const EdgeType&)> func);

    // ========== 遍历操作 ==========
    // 打印邻接表（通用版，支持任意可输出的顶点/边类型）
    void printAdjacencyList() const;

    // 获取指定顶点的邻接顶点
    QList<VertexType> getNeighbors(const VertexType& vertex) const;

    // 深度优先遍历（DFS）
    void dfs(const VertexType& startVertex);

    // 广度优先遍历（BFS）
    void bfs(const VertexType& startVertex);

private:
    // 邻接表核心存储：键=顶点类型，值=该顶点的所有出边
    QHash<VertexType, QList<EdgeType>> m_adjacencyList;
    // 顶点插入顺序
    QList<VertexType> m_vertexOrder;
    // 是否为无向图
    bool m_isUndirected;

    // 回调函数：适配自定义边/顶点类型
    std::function<VertexType(const EdgeType&)> m_getEdgeTarget;       // 从边取目标顶点
    std::function<bool(const VertexType&, const VertexType&)> m_isEdgeExist; // 判断边是否存在
    std::function<EdgeType(const EdgeType&)> m_createReverseEdge;     // 创建反向边

    // DFS递归辅助函数
    void dfsHelper(const VertexType& currentVertex, QSet<VertexType>& visited);

    // 兼容低版本Qt的removeIf替代函数
    void removeEdgeByTarget(QList<EdgeType>& edges, const VertexType& target);
};

// ===================== 模板类实现（必须放在头文件中） =====================
template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::addVertex(const VertexType& vertex) {
    if (!m_adjacencyList.contains(vertex)) {
        m_adjacencyList[vertex] = QList<EdgeType>();
        m_vertexOrder.append(vertex);
        std::ostringstream oss;
        oss << vertex;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][addVertex] 顶点 {} 添加成功", oss.str());
    } else {
        std::ostringstream oss;
        oss << vertex;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][addVertex] 顶点 {} 已存在，无需重复添加", oss.str());
    }
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyList<VertexType, EdgeType>::removeVertex(const VertexType& vertex) {
    if (!m_adjacencyList.contains(vertex)) {
        std::ostringstream oss;
        oss << vertex;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][removeVertex] 顶点 {} 不存在，删除失败", oss.str());
        return false;
    }

    // 1. 删除该顶点的邻接边
    m_adjacencyList.remove(vertex);
    m_vertexOrder.removeAll(vertex);

    // 2. 删除所有指向该顶点的边（兼容低版本Qt，不用removeIf）
    if (m_getEdgeTarget) {
        // 遍历QHash的键值对（pair.first=顶点，pair.second=边列表）
        typename QHash<VertexType, QList<EdgeType>>::iterator it;
        for (it = m_adjacencyList.begin(); it != m_adjacencyList.end(); ++it) {
            QList<EdgeType>& edges = it.value(); // 替代pair.second
            removeEdgeByTarget(edges, vertex);  // 替代removeIf
        }
    } else {
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::WARN, "[ui][Graph][removeVertex] 未设置边的目标顶点获取函数，无法删除指向该顶点的边");
        return false;
    }

    std::ostringstream oss;
    oss << vertex;
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][removeVertex] 顶点 {} 及关联边已删除", oss.str());
    return true;
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyList<VertexType, EdgeType>::hasVertex(const VertexType& vertex) const {
    return m_adjacencyList.contains(vertex);
}

template <typename VertexType, typename EdgeType>
QList<VertexType> GraphAdjacencyList<VertexType, EdgeType>::getAllVertices() const {
    return m_vertexOrder;
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyList<VertexType, EdgeType>::addEdge(const VertexType& from, const VertexType& to, const EdgeType& edge) {
    // 先确保顶点存在
    addVertex(from);
    addVertex(to);

    // 检查边是否已存在
    if (m_isEdgeExist && m_isEdgeExist(from, to)) {
        std::ostringstream oss1, oss2;
        oss1 << from;
        oss2 << to;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][addEdge] 边 {} -> {} 已存在，添加失败", oss1.str(), oss2.str());
        return false;
    }

    // 添加有向边
    m_adjacencyList[from].append(edge);

    // 如果是无向图，添加反向边
    if (m_isUndirected && from != to) {
        if (m_createReverseEdge) {
            EdgeType reverseEdge = m_createReverseEdge(edge);
            m_adjacencyList[to].append(reverseEdge);
        } else {
            LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::WARN, "[ui][Graph][addEdge] 未设置反向边构造函数，无向图反向边添加失败");
            return false;
        }
    }

    std::ostringstream oss1, oss2;
    oss1 << from;
    oss2 << to;
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][addEdge] 边 {} -> {} 添加成功", oss1.str(), oss2.str());
    return true;
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyList<VertexType, EdgeType>::removeEdge(const VertexType& from, const VertexType& to) {
    if (!hasVertex(from) || !hasVertex(to)) {
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][removeEdge] 顶点不存在，删除边失败");
        return false;
    }

    if (!m_getEdgeTarget) {
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::WARN, "[ui][Graph][removeEdge] 未设置边的目标顶点获取函数，无法删除边");
        return false;
    }

    // 删除正向边
    QList<EdgeType>& edges = m_adjacencyList[from];
    bool removed = false;
    for (int i = 0; i < edges.size(); ++i) {
        if (m_getEdgeTarget(edges[i]) == to) {
            edges.removeAt(i);
            removed = true;
            break;
        }
    }

    // 如果是无向图，删除反向边
    if (m_isUndirected && from != to && removed) {
        QList<EdgeType>& reverseEdges = m_adjacencyList[to];
        for (int i = 0; i < reverseEdges.size(); ++i) {
            if (m_getEdgeTarget(reverseEdges[i]) == from) {
                reverseEdges.removeAt(i);
                break;
            }
        }
    }

    if (removed) {
        std::ostringstream oss1, oss2;
        oss1 << from;
        oss2 << to;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][removeEdge] 边 {} -> {} 删除成功", oss1.str(), oss2.str());
    } else {
        std::ostringstream oss1, oss2;
        oss1 << from;
        oss2 << to;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][removeEdge] 边 {} -> {} 不存在，删除失败", oss1.str(), oss2.str());
    }
    return removed;
}

template <typename VertexType, typename EdgeType>
bool GraphAdjacencyList<VertexType, EdgeType>::hasEdge(const VertexType& from, const VertexType& to) const {
    if (!m_isEdgeExist) {
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::WARN, "[ui][Graph][hasEdge] 未设置边存在判断函数，默认返回false");
        return false;
    }
    return m_isEdgeExist(from, to);
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::setGetEdgeTargetFunc(std::function<VertexType(const EdgeType&)> func) {
    m_getEdgeTarget = func;
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::setIsEdgeExistFunc(std::function<bool(const VertexType&, const VertexType&)> func) {
    m_isEdgeExist = func;
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::setCreateReverseEdgeFunc(std::function<EdgeType(const EdgeType&)> func) {
    m_createReverseEdge = func;
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::printAdjacencyList() const {
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][printAdjacencyList] ========== 邻接表 ==========");
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][printAdjacencyList] 图类型：{}", m_isUndirected ? "无向图" : "有向图");

    for (const VertexType& vertex : m_vertexOrder) {
        if (!m_adjacencyList.contains(vertex)) {
            continue;
        }
        const QList<EdgeType>& edges = m_adjacencyList[vertex];

        // 显式转换为QString，避免arg()函数重载解析冲突
        QString vertexStr = QVariant::fromValue(vertex).toString();
        QString line = QString("顶点%1: ").arg(vertexStr);

        if (edges.isEmpty()) {
            line += "无邻接顶点";
        } else {
            for (const EdgeType& edge : edges) {
                if (m_getEdgeTarget) {
                    VertexType target = m_getEdgeTarget(edge);
                    QString targetStr = QVariant::fromValue(target).toString();
                    line += QString("(%1) ").arg(targetStr);
                } else {
                    line += "[边对象] ";
                }
            }
        }
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][printAdjacencyList] {}", line.toStdString());
    }
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][printAdjacencyList] ===========================");
}

template <typename VertexType, typename EdgeType>
QList<VertexType> GraphAdjacencyList<VertexType, EdgeType>::getNeighbors(const VertexType& vertex) const {
    QList<VertexType> neighbors;
    if (!m_adjacencyList.contains(vertex) || !m_getEdgeTarget) {
        return neighbors;
    }

    const QList<EdgeType>& edges = m_adjacencyList[vertex];
    for (const EdgeType& edge : edges) {
        neighbors.append(m_getEdgeTarget(edge));
    }
    return neighbors;
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::dfs(const VertexType& startVertex) {
    if (!hasVertex(startVertex)) {
        std::ostringstream oss;
        oss << startVertex;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][dfs] 起始顶点 {} 不存在", oss.str());
        return;
    }

    QSet<VertexType> visited;
    std::ostringstream oss;
    oss << startVertex;
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][dfs] ========== DFS遍历（起始顶点：{}）==========", oss.str());
    dfsHelper(startVertex, visited);
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][dfs] ===========================");
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::bfs(const VertexType& startVertex) {
    if (!hasVertex(startVertex)) {
        std::ostringstream oss;
        oss << startVertex;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][bfs] 起始顶点 {} 不存在", oss.str());
        return;
    }

    QSet<VertexType> visited;
    QList<VertexType> queue;

    // 初始化队列
    queue.append(startVertex);
    visited.insert(startVertex);

    std::ostringstream oss;
    oss << startVertex;
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][bfs] ========== BFS遍历（起始顶点：{}）==========", oss.str());
    while (!queue.isEmpty()) {
        VertexType current = queue.takeFirst();
        std::ostringstream oss_cur;
        oss_cur << current;
        LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][bfs] 访问顶点：{}", oss_cur.str());

        // 遍历邻接顶点
        if (m_adjacencyList.contains(current)) {
            const QList<EdgeType>& edges = m_adjacencyList[current];
            for (const EdgeType& edge : edges) {
                VertexType neighbor = m_getEdgeTarget(edge);
                if (!visited.contains(neighbor)) {
                    visited.insert(neighbor);
                    queue.append(neighbor);
                }
            }
        }
    }
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][bfs] ===========================");
}

template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::dfsHelper(const VertexType& currentVertex, QSet<VertexType>& visited) {
    // 标记已访问
    visited.insert(currentVertex);
    std::ostringstream oss;
    oss << currentVertex;
    LoggerManager::getInstance()->log(AppLogger::CraneMapLoggerPath().toStdString(), Level::DEBUG, "[ui][Graph][dfsHelper] 访问顶点：{}", oss.str());

    // 遍历所有邻接顶点
    if (m_adjacencyList.contains(currentVertex)) {
        const QList<EdgeType>& edges = m_adjacencyList[currentVertex];
        for (const EdgeType& edge : edges) {
            VertexType neighbor = m_getEdgeTarget(edge);
            if (!visited.contains(neighbor)) {
                dfsHelper(neighbor, visited);
            }
        }
    }
}

// 兼容低版本Qt的removeIf替代函数：删除边列表中目标顶点为target的边
template <typename VertexType, typename EdgeType>
void GraphAdjacencyList<VertexType, EdgeType>::removeEdgeByTarget(QList<EdgeType>& edges, const VertexType& target) {
    if (!m_getEdgeTarget) return;

    for (int i = edges.size() - 1; i >= 0; --i) {
        if (m_getEdgeTarget(edges[i]) == target) {
            edges.removeAt(i);
        }
    }
}

}

#endif // GRAPHADJACENCYLIST_H
