#ifndef DATAMONITORCHARTPLOT_H
#define DATAMONITORCHARTPLOT_H

#include <QObject>
#include <QPointer>
#include <QSharedPointer>
#include <QString>
#include <QVector>

#include "chartgraph.h"
#include "chartxaxiscontroller.h"

/*******************************************************************************
 * @file datamonitorchartplot.h

 * @author Simon <工号：13> 2026-06-05
 * @version 1.0.0
 *
 * @brief 数据监控图表类，负责管理一个 QCustomPlot 的共享时间轴、多条曲线和统一重绘。
 *
 * DataMonitorChartPlot 绑定一个 QCustomPlot，负责该图表内所有 ChartGraph 的统一管理。
 * 每个 DataMonitorChartPlot 都拥有自己独立的 ChartXAxisController，
 * 因此不同图表的 X 轴模式、时间线和显示范围互不影响。
 *
 * 职责边界：
 * - 管理一个 QCustomPlot，不管理多个图表。
 * - 创建、隐藏、删除、刷新和清空该 QCustomPlot 内的 QCPGraph。
 * - 维护共享 xData，并保证所有 ChartGraph 的 yData 与 xData 对齐。
 * - 委托 ChartXAxisController 管理 X 轴模式、时间点生成和范围刷新。
 * - 统一调用 QCustomPlot::replot()，避免每条曲线单独重绘。
 *******************************************************************************/
class DataMonitorChartPlot : public QObject
{
    Q_OBJECT

public:
    // ================================构造函数================================
    // 使用指定图表 ID 和 QCustomPlot 控件创建数据监控图表对象。
    explicit DataMonitorChartPlot(const QString &plotId,
                                  QCustomPlot *chart,
                                  QObject *parent = nullptr);
    ~DataMonitorChartPlot() override;

    // ================================图表信息================================
    // 获取当前数据监控图表的业务 ID。
    QString plotId() const;

    // 获取当前绑定的 QCustomPlot 指针。
    // 该指针归 Qt 界面对象树所有，调用方不要手动 delete。
    QCustomPlot *chart() const;

    // 获取当前图表内由本类管理的曲线数量。
    int graphCount() const;

    // 获取当前图表共享 X 轴数据点数量。
    int pointCount() const;

    // 判断 graphId 是否是有效的曲线索引。
    bool isValidGraphId(int graphId) const;

    // 获取当前图表独立的 X 轴控制器。
    ChartXAxisController *xAxisController() const;

    // ================================曲线管理================================
    // 添加一条曲线，并自动将 Config::id 同步为 QCustomPlot 内部 QCPGraph 数组索引。
    bool addGraph(const ChartGraph::Config &config);

    // 隐藏或显示指定曲线。
    bool hideGraph(int graphId, bool hidden = true);

    // 删除指定曲线。
    // 删除后，剩余曲线的 graphId 会重新同步为最新 QCustomPlot 数组索引。
    bool removeGraph(int graphId);

    // ================================数据刷新================================
    // 刷新单条曲线。
    // 当前时间点会追加到共享 xData，其他曲线会补 NaN 以保持数据长度一致。
    bool refreshGraph(int graphId, double value);

    // 批量刷新全部曲线。
    // 推荐用于同一采样周期内同时刷新温度、湿度、氧气等多条曲线。
    bool refreshGraphs(const QVector<double> &values);

    // 清空指定曲线的数据。
    // 为保持与共享 xData 对齐，会用 NaN 补齐当前已有时间点。
    bool clearGraphData(int graphId);

    // 清空当前图表的所有曲线数据和共享 xData。
    void clearAllGraphData();

    bool addVerticalMarker(double x, const QPen &pen);
    bool addVerticalMarkerAtLatestX(const QPen &pen, double *x = nullptr);
    void clearVerticalMarkers();

    // ================================坐标轴管理================================
    // 设置 X 轴模式。
    // arg1 兼容 QCheckBox::stateChanged(int)，Qt::Checked 表示秒模式。
    void setXAxisMode(int arg1);

    // 设置最大缓存点数。
    void setMaxPointCount(int count);

    // 获取最大缓存点数。
    int maxPointCount() const;

    // 设置时分秒模式下 X 轴显示的最近时间窗口，单位：秒。
    void setVisibleWindowSeconds(double seconds);

    // 获取时分秒模式下 X 轴显示的最近时间窗口，单位：秒。
    double visibleWindowSeconds() const;

private:
    // ------ 初始化框架 ------
    // 初始化 QCustomPlot 的背景、图例、Y 轴等基础显示配置。
    void setupChart();

    // ------ 数据刷新 ------
    // 刷新全部 ChartGraph 的 QCPGraph 数据。
    bool refreshAllGraphs();

    // 当缓存点数超过上限时，同步裁剪 xData 和所有 yData。
    void trimDataIfNeeded();

    // ------ 曲线管理 ------
    // 删除曲线后重新同步所有 ChartGraph 的 id。
    void syncGraphIds();

private:
    // ------ 图表信息 ------
    // 当前数据监控图表的业务 ID，由上层管理器作为索引使用。
    QString m_plotId;

    // 当前绑定的 QCustomPlot。
    // 使用 QPointer 避免界面销毁后产生悬空指针。
    QPointer<QCustomPlot> m_chart;

    // ------ 坐标轴管理 ------
    // 当前图表独立的 X 轴控制器。
    // 不同 DataMonitorChartPlot 不能共享该对象，否则时间线会互相影响。
    ChartXAxisController *m_xAxisController = nullptr;

    // ------ 曲线管理 ------
    // 当前 QCustomPlot 内由本类管理的曲线集合。
    // QVector 下标与 QCustomPlot 内部 QCPGraph 数组索引保持一致。
    QVector<QSharedPointer<ChartGraph>> m_graphs;
    QVector<QCPItemLine *> m_verticalMarkers;

    // ------ 数据刷新 ------
    // 当前图表共享的 X 轴时间数据。
    QVector<double> m_xData;

    // 最大缓存点数，超过后会从头部同步裁剪 xData 和所有 yData。
    int m_maxPointCount = 25000;
};

#endif // DATAMONITORCHARTPLOT_H
