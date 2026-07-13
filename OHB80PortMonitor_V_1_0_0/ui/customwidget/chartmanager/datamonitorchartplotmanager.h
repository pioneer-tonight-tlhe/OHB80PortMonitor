#ifndef DATAMONITORCHARTPLOTMANAGER_H
#define DATAMONITORCHARTPLOTMANAGER_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>

#include "datamonitorchartplot.h"
#include "singleton.h"

/****************************************************************************
 * @file datamonitorchartplotmanager.h
 * @brief 数据监控图表管理类，负责通过 plotId 管理多个 DataMonitorChartPlot。
 * @author Simon <工号：3> 2026-06-05
 * @version 1.0.0
 *
 * @brief 多个数据监控图表的统一管理入口。
 *
 * DataMonitorChartPlotManager 面向全新接口设计，不兼容旧版 ChartManager 槽函数。
 * 外部通过 plotId 访问指定设备图表，例如 device1Plot、device2Plot、device3Plot。
 *
 * 职责边界：
 * - 只管理多个 DataMonitorChartPlot，不直接管理 QCPGraph。
 * - 不使用 QCustomPlot* 作为索引，QCustomPlot* 只在 registerPlot() 时绑定。
 * - 不维护旧版 onXAxisModeChanged(QCustomPlot*, int) 兼容接口。
 * - 所有 graph 操作都转发给对应的 DataMonitorChartPlot。
 ****************************************************************************/
class DataMonitorChartPlotManager : public QObject, public Singleton<DataMonitorChartPlotManager>
{
    Q_OBJECT

public:
    // ================================单例入口================================
    // 通过 Singleton<DataMonitorChartPlotManager>::getInstance() 获取全局唯一对象。
    // 示例：DataMonitorChartPlotManager::getInstance()->registerPlot(...);
    using Singleton<DataMonitorChartPlotManager>::getInstance;

    // ================================图表管理================================
    // 注册一个设备图表。
    // plotId 是业务侧唯一标识，例如 device1Plot。
    bool registerPlot(const QString &plotId, QCustomPlot *chart);

    // 注销一个设备图表，并释放对应 DataMonitorChartPlot。
    bool unregisterPlot(const QString &plotId);

    // 判断指定 plotId 是否已经注册。
    bool containsPlot(const QString &plotId) const;

    // 获取指定 plotId 对应的数据监控图表对象。
    DataMonitorChartPlot *plot(const QString &plotId) const;

    // ================================曲线管理================================
    // 给指定设备图表添加一条曲线。
    bool addGraph(const QString &plotId, const ChartGraph::Config &config);

    // 隐藏或显示指定设备图表中的指定曲线。
    bool hideGraph(const QString &plotId, int graphId, bool hidden = true);

    // 删除指定设备图表中的指定曲线。
    bool removeGraph(const QString &plotId, int graphId);

    // ================================数据刷新================================
    // 刷新指定设备图表中的单条曲线。
    bool refreshGraph(const QString &plotId, int graphId, double value);

    // 批量刷新指定设备图表中的所有曲线。
    // values 的顺序需要和该图表内 graphId 顺序保持一致。
    bool refreshGraphs(const QString &plotId, const QVector<double> &values);

    // 清空指定设备图表中的单条曲线数据。
    bool clearGraphData(const QString &plotId, int graphId);

    // 清空指定设备图表中的全部曲线数据。
    bool clearAllGraphData(const QString &plotId);

    // ================================坐标轴管理================================
    // 设置指定设备图表的 X 轴模式。
    bool setXAxisMode(const QString &plotId, int arg1);

signals:
    // ------ 图表管理 ------
    // 设备图表注册成功后触发。
    void plotRegistered(const QString &plotId);

    // 设备图表注销成功后触发。
    void plotUnregistered(const QString &plotId);

    // ------ 坐标轴管理 ------
    // 指定设备图表的 X 轴模式切换成功后触发。
    void xAxisModeChanged(const QString &plotId, int arg1);

private:
    friend class Singleton<DataMonitorChartPlotManager>;

    explicit DataMonitorChartPlotManager(QObject *parent = nullptr);
    Q_DISABLE_COPY(DataMonitorChartPlotManager)

private:
    // ------ 图表管理 ------
    // 设备图表映射表。
    // QString：设备图表业务 ID，例如 device1Plot。
    // DataMonitorChartPlot*：单个 QCustomPlot 的数据监控图表对象，由本 manager 持有。
    QMap<QString, DataMonitorChartPlot*> m_plotMap;

    // 设备图表销毁连接映射表。
    // 注销 plot 时主动断开对应 destroyed 连接，避免旧连接影响后续重新注册。
    QMap<QString, QMetaObject::Connection> m_destroyConnectionMap;
};

#endif // DATAMONITORCHARTPLOTMANAGER_H
