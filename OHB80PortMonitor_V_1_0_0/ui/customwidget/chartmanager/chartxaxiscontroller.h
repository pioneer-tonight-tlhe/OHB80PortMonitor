#ifndef CHARTXAXISCONTROLLER_H
#define CHARTXAXISCONTROLLER_H

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QVector>

#include "qcustomplot.h"

/*******************************************************************
 * @file chartxaxiscontroller.h
 *
 * @author Simon <工号：13> 2026-06-05
 * @version 1.0.0
 *
 * @brief 图表 X 轴控制器，负责时分秒模式和秒模式的切换、时间点生成和范围刷新。
 *
 * ChartXAxisController 只负责一张 QCustomPlot 的 X 轴状态，不管理曲线数据。
 * 不同 QCustomPlot 应该拥有不同的 ChartXAxisController，以保证各自时间线互不影响。
 *
 * 支持的 X 轴模式：
 * - DateTime：使用真实时间戳，坐标轴显示 hh:mm:ss。
 * - ElapsedSeconds：使用累计秒数，坐标轴显示秒。
*******************************************************************/
class ChartXAxisController : public QObject
{
    Q_OBJECT

public:
    // ================================坐标轴模式================================
    // X 轴显示模式。
    enum Mode {
        DateTime = 0,
        ElapsedSeconds = 1
    };
    Q_ENUM(Mode)

    // ================================构造函数================================
    // 使用指定 QCustomPlot 创建 X 轴控制器。
    explicit ChartXAxisController(QCustomPlot *chart = nullptr,
                                  QObject *parent = nullptr);

    // ================================图表绑定================================
    // 重新绑定 QCustomPlot。
    void setChart(QCustomPlot *chart);

    // 获取当前绑定的 QCustomPlot。
    QCustomPlot *chart() const;

    // ------ 模式切换 ------
    // 获取当前 X 轴模式。
    Mode mode() const;

    // 设置当前 X 轴模式。
    void setMode(Mode mode);

    // 使用旧版 QCheckBox::stateChanged(int) 参数设置 X 轴模式。
    // Qt::Checked 表示秒模式，其他状态表示时分秒模式。
    void setModeByCheckState(int arg1);

    // ================================数据刷新================================
    // 根据当前模式生成下一个 X 轴坐标。
    double nextX();

    // 根据当前模式和最新 X 值刷新坐标轴范围。
    void refreshRange(double currentX, const QVector<double> &xData);

    // ================================坐标轴配置================================
    // 初始化 X 轴标签、刻度和显示范围。
    void setupAxis();

    // 重置当前图表自己的时间线。
    void resetTimeline();

    // 重置当前 X 轴显示范围。
    void resetRange();

    // 设置实时窗口模式下 X 轴显示的时间窗口，单位：秒。
    void setVisibleWindowSeconds(double seconds);

    // 获取实时窗口模式下 X 轴显示的时间窗口，单位：秒。
    double visibleWindowSeconds() const;

signals:
    // ------ 模式切换 ------
    // X 轴模式发生变化后触发。
    void modeChanged(ChartXAxisController::Mode mode);

private:
    // ------ 坐标轴配置 ------
    // 根据当前模式应用对应 ticker。
    void applyTicker();

    // 根据当前模式刷新 X 轴标题。
    void applyAxisLabel();

private:
    // ------ 图表绑定 ------
    // 当前控制的 QCustomPlot。
    QPointer<QCustomPlot> m_chart;

    // ------ 坐标轴模式 ------
    // 当前 X 轴显示模式。
    Mode m_mode = DateTime;

    // ------ 时间线状态 ------
    // 当前时间线的参考时间。
    QDateTime m_referenceTime;

    // 上一次生成 X 坐标的时间。
    QDateTime m_lastUpdateTime;

    // 累计秒数，用于秒模式。
    double m_elapsedSeconds = 0.0;

    // 时分秒模式下可见的最近时间窗口，单位：秒。
    double m_visibleWindowSeconds = 20.0;
};

#endif // CHARTXAXISCONTROLLER_H
