/**
 * @file chartgraph.cpp
 * @brief ChartGraph 单条曲线业务封装类实现。
 * @author Simon <3> 2026-06-05
 * @version 1.0.0
 */
#include "chartgraph.h"

#include <QtMath>
#include <QtGlobal>

ChartGraph::ChartGraph(QCPGraph *graph,
                       const Config &config,
                       QObject *parent)
    : QObject(parent),
      m_graph(graph),
      m_config(config)
{
    applyConfig();
}

int ChartGraph::id() const
{
    return m_config.id;
}

void ChartGraph::setId(int id)
{
    m_config.id = id;
    updateLegendName();
}

QString ChartGraph::displayName() const
{
    return m_config.displayName;
}

QString ChartGraph::unit() const
{
    return m_config.unit;
}

QCPGraph *ChartGraph::qcpGraph() const
{
    return m_graph.data();
}

void ChartGraph::appendValue(double y)
{
    m_yData.append(y);
    m_lastValue = y;
    updateLegendName();
}

void ChartGraph::appendInvalidValue()
{
    m_yData.append(qQNaN());
}

void ChartGraph::appendInvalidValues(int count)
{
    if (count <= 0)
        return;

    for (int i = 0; i < count; ++i)
        appendInvalidValue();
}

bool ChartGraph::refresh(const QVector<double> &xData)
{
    if (!m_graph)
        return false;

    // xData 由上层统一维护，刷新前必须保证 X/Y 数据数量一致。
    if (xData.size() != m_yData.size())
        return false;

    m_graph->setData(xData, m_yData, true);
    updateLegendName();
    return true;
}

void ChartGraph::clearData()
{
    m_yData.clear();
    m_lastValue = 0.0;

    if (m_graph)
        m_graph->data()->clear();

    updateLegendName();
}

void ChartGraph::removeFirst(int count)
{
    if (count <= 0 || m_yData.isEmpty())
        return;

    const int removeCount = qMin(count, m_yData.size());
    m_yData.remove(0, removeCount);
    m_lastValue = m_yData.isEmpty() ? 0.0 : m_yData.last();

    updateLegendName();
}

void ChartGraph::setHidden(bool hidden)
{
    if (!m_graph)
        return;

    m_graph->setVisible(!hidden);
}

bool ChartGraph::isVisible() const
{
    return m_graph && m_graph->visible();
}

int ChartGraph::pointCount() const
{
    return m_yData.size();
}

double ChartGraph::lastValue() const
{
    return m_lastValue;
}

void ChartGraph::applyConfig()
{
    if (!m_graph)
        return;

    m_graph->setPen(m_config.pen);
    m_graph->setLineStyle(m_config.lineStyle);
    m_graph->setAntialiased(false);
    m_graph->setAntialiasedFill(false);
    m_graph->setVisible(m_config.visible);

    updateLegendName();
}

void ChartGraph::updateLegendName()
{
    if (!m_graph)
        return;

    const int precision = qMax(0, m_config.precision);
    const QString valueText = QString::number(m_lastValue, 'f', precision);
    m_graph->setName(legendTitle() + " : " + valueText + m_config.unit);
}

QString ChartGraph::legendTitle() const
{
    return m_config.displayName.isEmpty() ? QString::number(m_config.id) : m_config.displayName;
}
