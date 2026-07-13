#ifndef CHARTGRAPH_H
#define CHARTGRAPH_H

#include <QObject>
#include <QPen>
#include <QPointer>
#include <QVector>

#include "qcustomplot.h"
/***************************************************************************
 * @file chartgraph.h
 *
 * @author Simon <工号：13> 2026-06-05
 * @maintainer 李四 <A00456> 2026-06-05
 * @version 1.0.0
 *
 * @brief 单条 QCPGraph 曲线的业务封装类，负责曲线配置、Y 轴数据缓存和图例状态维护。
 *
 * ChartGraph 只管理一条 QCPGraph 对应的业务状态，不负责管理 QCustomPlot。
 * X 轴时间数据由上层图表对象统一维护，ChartGraph 只保存本曲线的 Y 轴数据。
 *
 * 职责边界：
 * - 不创建 QCPGraph，QCPGraph 由上层 QCustomPlot::addGraph() 创建。
 * - 不删除 QCPGraph，删除动作由上层通过 QCustomPlot::removeGraph() 完成。
 * - 不生成 X 轴时间点，刷新时由上层传入共享 xData。
 * - 不调用 QCustomPlot::replot()，重绘由上层统一调度。
 ***************************************************************************/
class ChartGraph : public QObject
{
    Q_OBJECT

public:
    // ------曲线配置------
    // 单条曲线的创建配置。
    struct Config {
        // QCustomPlot 内部 QCPGraph 数组索引。
        // 注意：调用 QCustomPlot::removeGraph() 后，剩余 graph 的索引会变化，
        // 上层需要通过 setId() 重新同步该值。
        int id = -1;

        // 图例显示名称，例如 RH、T、O2。
        // 如果为空，图例名称会退回显示 id。
        QString displayName;

        // 图例数值单位，例如 %、℃、Pa。
        QString unit;

        // 曲线画笔配置，包含颜色、线宽、线型等基础绘制信息。
        QPen pen;

        // 曲线绘制样式，默认使用阶梯线，适合采样点数据展示。
        QCPGraph::LineStyle lineStyle = QCPGraph::lsStepLeft;

        // 初始显示状态。
        bool visible = true;

        // 图例中最后一个数值的小数位数，只影响显示文本，不影响原始数据。
        int precision = 1;
    };

    // ================================构造函数================================
    // 使用已经创建好的 QCPGraph 构造曲线包装对象，并立即应用 Config 配置。
    explicit ChartGraph(QCPGraph *graph,
                        const Config &config,
                        QObject *parent = nullptr);

    // ================================曲线配置================================
    // 获取当前曲线在 QCustomPlot 内部的 QCPGraph 数组索引。
    int id() const;

    // 更新当前曲线在 QCustomPlot 内部的 QCPGraph 数组索引。
    // 删除 graph 后，上层应重新遍历并同步所有 ChartGraph 的 id。
    void setId(int id);

    // 获取图例显示名称。
    QString displayName() const;

    // 获取图例数值单位。
    QString unit() const;

    // 获取当前封装的 QCPGraph 指针。
    // 该指针归 QCustomPlot 所有，调用方不要手动 delete。
    QCPGraph *qcpGraph() const;

    // ================================数据刷新================================
    // 追加一个 Y 轴数据点。
    // X 轴时间点由上层统一保存，ChartGraph 不持有 xData。
    void appendValue(double y);

    // 追加一个无效 Y 轴数据点。
    // 用于共享 xData 下某条曲线在当前时间点没有数据的情况，QCustomPlot 会把 NaN 显示为断点。
    void appendInvalidValue();

    // 批量追加无效 Y 轴数据点。
    // 新增 graph 或清空单条 graph 后，可用该接口快速补齐到共享 xData 长度。
    void appendInvalidValues(int count);

    // 使用上层传入的共享 xData 刷新 QCPGraph。
    // 要求 xData.size() 必须与本曲线 yData.size() 一致。
    bool refresh(const QVector<double> &xData);

    // 清空当前曲线的 Y 轴缓存和 QCPGraph 内部数据。
    void clearData();

    // 从头部移除指定数量的 Y 轴数据点。
    // 上层裁剪共享 xData 时，必须同步调用该接口裁剪每条 ChartGraph。
    void removeFirst(int count);

    // ================================状态控制================================
    // 设置当前曲线是否隐藏。
    void setHidden(bool hidden);

    // 判断当前曲线是否处于显示状态。
    bool isVisible() const;

    // 获取当前曲线缓存的数据点数量。
    int pointCount() const;

    // 获取当前曲线最后一次追加的 Y 轴数值。
    double lastValue() const;

private:
    // ------ 曲线配置 ------
    // 将 Config 中的画笔、线型、显示状态应用到 QCPGraph。
    void applyConfig();

    // 根据最后一个数值更新图例名称。
    void updateLegendName();

    // 获取图例标题。displayName 为空时，使用 id 作为兜底标题。
    QString legendTitle() const;

private:
    // ------ 曲线对象 ------
    // 当前业务对象封装的 QCPGraph。
    // 使用 QPointer 避免 QCPGraph 被 QCustomPlot 删除后产生悬空指针。
    QPointer<QCPGraph> m_graph;

    // ------ 曲线配置 ------
    // 当前曲线的静态配置和显示格式配置。
    Config m_config;

    // ------ 数据刷新 ------
    // 当前曲线的 Y 轴数据缓存。
    // X 轴数据由上层图表对象统一维护，避免同一 QCustomPlot 下多条曲线重复保存时间轴。
    QVector<double> m_yData;

    // 当前曲线最后一次追加的 Y 轴值，用于图例显示。
    double m_lastValue = 0.0;
};

#endif // CHARTGRAPH_H
