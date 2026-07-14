/**
 * @file datamonitorchartplotmanager.cpp
 * @brief DataMonitorChartPlotManager 数据监控图表管理类实现。
 * @author Simon <3> 2026-06-05
 * @version 1.0.0
 */
#include "datamonitorchartplotmanager.h"

DataMonitorChartPlotManager::DataMonitorChartPlotManager(QObject *parent)
    : QObject(parent)
{
}

bool DataMonitorChartPlotManager::registerPlot(const QString &plotId, QCustomPlot *chart)
{
    if (plotId.isEmpty() || !chart || m_plotMap.contains(plotId))
        return false;

    DataMonitorChartPlot *monitorPlot = new DataMonitorChartPlot(plotId, chart, this);
    m_plotMap.insert(plotId, monitorPlot);

    const QMetaObject::Connection connection = connect(chart, &QObject::destroyed, this, [this, plotId]() {
        unregisterPlot(plotId);
    });
    m_destroyConnectionMap.insert(plotId, connection);

    emit plotRegistered(plotId);
    return true;
}

bool DataMonitorChartPlotManager::unregisterPlot(const QString &plotId)
{
    if (!m_plotMap.contains(plotId))
        return false;

    if (m_destroyConnectionMap.contains(plotId))
        QObject::disconnect(m_destroyConnectionMap.take(plotId));

    DataMonitorChartPlot *monitorPlot = m_plotMap.take(plotId);
    delete monitorPlot;

    emit plotUnregistered(plotId);
    return true;
}

bool DataMonitorChartPlotManager::containsPlot(const QString &plotId) const
{
    return m_plotMap.contains(plotId);
}

DataMonitorChartPlot *DataMonitorChartPlotManager::plot(const QString &plotId) const
{
    return m_plotMap.value(plotId, nullptr);
}

bool DataMonitorChartPlotManager::addGraph(const QString &plotId, const ChartGraph::Config &config)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->addGraph(config) : false;
}

bool DataMonitorChartPlotManager::hideGraph(const QString &plotId, int graphId, bool hidden)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->hideGraph(graphId, hidden) : false;
}

bool DataMonitorChartPlotManager::removeGraph(const QString &plotId, int graphId)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->removeGraph(graphId) : false;
}

bool DataMonitorChartPlotManager::refreshGraph(const QString &plotId, int graphId, double value)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->refreshGraph(graphId, value) : false;
}

bool DataMonitorChartPlotManager::refreshGraphs(const QString &plotId, const QVector<double> &values)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->refreshGraphs(values) : false;
}

bool DataMonitorChartPlotManager::clearGraphData(const QString &plotId, int graphId)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->clearGraphData(graphId) : false;
}

bool DataMonitorChartPlotManager::clearAllGraphData(const QString &plotId)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    if (!monitorPlot)
        return false;

    monitorPlot->clearAllGraphData();
    return true;
}

bool DataMonitorChartPlotManager::addVerticalMarker(const QString &plotId,
                                                    double x,
                                                    const QPen &pen)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->addVerticalMarker(x, pen) : false;
}

bool DataMonitorChartPlotManager::addVerticalMarkerAtLatestX(const QString &plotId,
                                                             const QPen &pen,
                                                             double *x)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    return monitorPlot ? monitorPlot->addVerticalMarkerAtLatestX(pen, x) : false;
}

bool DataMonitorChartPlotManager::clearVerticalMarkers(const QString &plotId)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    if (!monitorPlot) {
        return false;
    }

    monitorPlot->clearVerticalMarkers();
    return true;
}

bool DataMonitorChartPlotManager::setXAxisMode(const QString &plotId, int arg1)
{
    DataMonitorChartPlot *monitorPlot = plot(plotId);
    if (!monitorPlot)
        return false;

    monitorPlot->setXAxisMode(arg1);
    emit xAxisModeChanged(plotId, arg1);
    return true;
}
