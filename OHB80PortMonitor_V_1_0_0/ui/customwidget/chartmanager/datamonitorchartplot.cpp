/**
 * @file datamonitorchartplot.cpp
 * @brief DataMonitorChartPlot 数据监控图表类实现。
 * @author Simon <3> 2026-06-05
 * @version 1.0.0
 */
#include "datamonitorchartplot.h"

#include <QBrush>
#include <QFont>
#include <QtGlobal>

DataMonitorChartPlot::DataMonitorChartPlot(const QString &plotId,
                                           QCustomPlot *chart,
                                           QObject *parent)
    : QObject(parent),
      m_plotId(plotId),
      m_chart(chart)
{
    m_xAxisController = new ChartXAxisController(chart, this);
    setupChart();

    if (m_chart) {
        connect(m_chart.data(), &QObject::destroyed, this, [this]() {
            m_chart.clear();
            m_graphs.clear();
            m_xData.clear();
        });
    }
}

QString DataMonitorChartPlot::plotId() const
{
    return m_plotId;
}

QCustomPlot *DataMonitorChartPlot::chart() const
{
    return m_chart.data();
}

int DataMonitorChartPlot::graphCount() const
{
    return m_graphs.size();
}

int DataMonitorChartPlot::pointCount() const
{
    return m_xData.size();
}

bool DataMonitorChartPlot::isValidGraphId(int graphId) const
{
    return graphId >= 0 && graphId < m_graphs.size() && !m_graphs[graphId].isNull();
}

ChartXAxisController *DataMonitorChartPlot::xAxisController() const
{
    return m_xAxisController;
}

bool DataMonitorChartPlot::addGraph(const ChartGraph::Config &config)
{
    if (!m_chart)
        return false;

    QCPGraph *qcpGraph = m_chart->addGraph();
    if (!qcpGraph)
        return false;

    ChartGraph::Config graphConfig = config;
    graphConfig.id = m_chart->graphCount() - 1;

    QSharedPointer<ChartGraph> graph = QSharedPointer<ChartGraph>::create(qcpGraph, graphConfig);
    graph->appendInvalidValues(m_xData.size());
    graph->refresh(m_xData);

    m_graphs.append(graph);
    m_chart->replot(QCustomPlot::rpQueuedReplot);
    return true;
}

bool DataMonitorChartPlot::hideGraph(int graphId, bool hidden)
{
    if (!m_chart || !isValidGraphId(graphId))
        return false;

    m_graphs[graphId]->setHidden(hidden);
    m_chart->replot(QCustomPlot::rpQueuedReplot);
    return true;
}

bool DataMonitorChartPlot::removeGraph(int graphId)
{
    if (!m_chart || !isValidGraphId(graphId))
        return false;

    QCPGraph *qcpGraph = m_graphs[graphId]->qcpGraph();
    if (qcpGraph)
        m_chart->removeGraph(qcpGraph);

    m_graphs.removeAt(graphId);
    syncGraphIds();

    m_chart->replot(QCustomPlot::rpQueuedReplot);
    return true;
}

bool DataMonitorChartPlot::refreshGraph(int graphId, double value)
{
    if (!m_chart || !m_xAxisController || !isValidGraphId(graphId))
        return false;

    const double currentX = m_xAxisController->nextX();
    m_xData.append(currentX);

    for (int i = 0; i < m_graphs.size(); ++i) {
        if (i == graphId)
            m_graphs[i]->appendValue(value);
        else
            m_graphs[i]->appendInvalidValue();
    }

    trimDataIfNeeded();

    const bool refreshed = refreshAllGraphs();
    m_xAxisController->refreshRange(currentX, m_xData);
    m_chart->replot(QCustomPlot::rpQueuedReplot);
    return refreshed;
}

bool DataMonitorChartPlot::refreshGraphs(const QVector<double> &values)
{
    if (!m_chart || !m_xAxisController || values.size() != m_graphs.size() || m_graphs.isEmpty())
        return false;

    const double currentX = m_xAxisController->nextX();
    m_xData.append(currentX);

    for (int i = 0; i < m_graphs.size(); ++i)
        m_graphs[i]->appendValue(values[i]);

    trimDataIfNeeded();

    const bool refreshed = refreshAllGraphs();
    m_xAxisController->refreshRange(currentX, m_xData);
    m_chart->replot(QCustomPlot::rpQueuedReplot);
    return refreshed;
}

bool DataMonitorChartPlot::clearGraphData(int graphId)
{
    if (!m_chart || !isValidGraphId(graphId))
        return false;

    m_graphs[graphId]->clearData();
    m_graphs[graphId]->appendInvalidValues(m_xData.size());

    const bool refreshed = m_graphs[graphId]->refresh(m_xData);
    m_chart->replot(QCustomPlot::rpQueuedReplot);
    return refreshed;
}

void DataMonitorChartPlot::clearAllGraphData()
{
    m_xData.clear();

    for (const QSharedPointer<ChartGraph> &graph : m_graphs) {
        if (graph)
            graph->clearData();
    }

    if (m_xAxisController) {
        m_xAxisController->resetTimeline();
        m_xAxisController->resetRange();
    }

    if (m_chart)
        m_chart->replot(QCustomPlot::rpQueuedReplot);
}

void DataMonitorChartPlot::setXAxisMode(int arg1)
{
    if (!m_xAxisController)
        return;

    m_xAxisController->setModeByCheckState(arg1);
    clearAllGraphData();
}

void DataMonitorChartPlot::setMaxPointCount(int count)
{
    m_maxPointCount = qMax(1, count);
    trimDataIfNeeded();
    refreshAllGraphs();

    if (m_chart)
        m_chart->replot(QCustomPlot::rpQueuedReplot);
}

int DataMonitorChartPlot::maxPointCount() const
{
    return m_maxPointCount;
}

void DataMonitorChartPlot::setVisibleWindowSeconds(double seconds)
{
    if (m_xAxisController)
        m_xAxisController->setVisibleWindowSeconds(seconds);

    if (m_chart)
        m_chart->replot(QCustomPlot::rpQueuedReplot);
}

double DataMonitorChartPlot::visibleWindowSeconds() const
{
    return m_xAxisController ? m_xAxisController->visibleWindowSeconds() : 0.0;
}

void DataMonitorChartPlot::setupChart()
{
    if (!m_chart)
        return;

    m_chart->setBackground(Qt::transparent);
    m_chart->axisRect()->setBackground(QBrush(Qt::transparent));
    m_chart->setNotAntialiasedElements(QCP::aeAll);

    m_chart->legend->setVisible(true);
    m_chart->legend->setFont(QFont("Microsoft YaHei", 8));
    m_chart->legend->setBrush(QBrush(Qt::transparent));
    m_chart->legend->setBorderPen(Qt::NoPen);

    m_chart->yAxis->setLabel("data");
    m_chart->yAxis->setLabelFont(QFont("Microsoft YaHei", 9));
    m_chart->yAxis->setRange(0, 75);

    if (m_xAxisController)
        m_xAxisController->setupAxis();
}

bool DataMonitorChartPlot::refreshAllGraphs()
{
    bool allRefreshed = true;

    for (const QSharedPointer<ChartGraph> &graph : m_graphs) {
        if (graph)
            allRefreshed = graph->refresh(m_xData) && allRefreshed;
    }

    return allRefreshed;
}

void DataMonitorChartPlot::trimDataIfNeeded()
{
    if (m_xData.size() <= m_maxPointCount)
        return;

    const int removeCount = m_xData.size() - m_maxPointCount;
    m_xData.remove(0, removeCount);

    for (const QSharedPointer<ChartGraph> &graph : m_graphs) {
        if (graph)
            graph->removeFirst(removeCount);
    }
}

void DataMonitorChartPlot::syncGraphIds()
{
    for (int i = 0; i < m_graphs.size(); ++i) {
        if (m_graphs[i])
            m_graphs[i]->setId(i);
    }
}
