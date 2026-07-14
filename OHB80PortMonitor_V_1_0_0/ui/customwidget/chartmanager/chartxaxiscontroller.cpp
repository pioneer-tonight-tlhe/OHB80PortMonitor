/**
 * @file chartxaxiscontroller.cpp
 * @brief ChartXAxisController 图表 X 轴控制器实现。
 * @author Simon <3> 2026-06-05
 * @version 1.0.0
 */
#include "chartxaxiscontroller.h"

#include <QFont>
#include <QSharedPointer>
#include <QtGlobal>

ChartXAxisController::ChartXAxisController(QCustomPlot *chart,
                                           QObject *parent)
    : QObject(parent),
      m_chart(chart)
{
    resetTimeline();
}

void ChartXAxisController::setChart(QCustomPlot *chart)
{
    m_chart = chart;
    setupAxis();
}

QCustomPlot *ChartXAxisController::chart() const
{
    return m_chart.data();
}

ChartXAxisController::Mode ChartXAxisController::mode() const
{
    return m_mode;
}

void ChartXAxisController::setMode(Mode mode)
{
    if (m_mode == mode) {
        applyTicker();
        applyAxisLabel();
        resetRange();
        return;
    }

    m_mode = mode;
    resetTimeline();
    applyTicker();
    applyAxisLabel();
    resetRange();

    emit modeChanged(m_mode);
}

void ChartXAxisController::setModeByCheckState(int arg1)
{
    setMode(arg1 == Qt::Checked ? ElapsedSeconds : DateTime);
}

double ChartXAxisController::nextX()
{
    if (m_mode == ElapsedSeconds) {
        const QDateTime now = QDateTime::currentDateTime();
        m_elapsedSeconds += m_lastUpdateTime.msecsTo(now) / 1000.0;
        m_lastUpdateTime = now;
        return m_elapsedSeconds;
    }

    return QDateTime::currentMSecsSinceEpoch() / 1000.0;
}

void ChartXAxisController::refreshRange(double currentX, const QVector<double> &xData)
{
    if (!m_chart)
        return;

    if (m_mode == ElapsedSeconds) {
        const double lower = xData.isEmpty() ? 0.0 : qMin(0.0, xData.first());
        const double upper = qMax(currentX, lower + 1.0);
        m_chart->xAxis->setRange(lower, upper);
        return;
    }

    m_chart->xAxis->setRange(currentX - m_visibleWindowSeconds, currentX);
}

void ChartXAxisController::setupAxis()
{
    if (!m_chart)
        return;

    m_chart->xAxis->setLabelFont(QFont("Microsoft YaHei", 9));
    m_chart->xAxis->setTickLength(10, 0);
    m_chart->xAxis->setSubTickLength(2, 0);

    applyTicker();
    applyAxisLabel();
    resetRange();
}

void ChartXAxisController::resetTimeline()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_referenceTime = now;
    m_lastUpdateTime = now;
    m_elapsedSeconds = 0.0;
}

void ChartXAxisController::resetRange()
{
    if (!m_chart)
        return;

    if (m_mode == ElapsedSeconds) {
        m_chart->xAxis->setRange(0, m_visibleWindowSeconds);
        return;
    }

    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    m_chart->xAxis->setRange(now - m_visibleWindowSeconds, now);
}

void ChartXAxisController::setVisibleWindowSeconds(double seconds)
{
    m_visibleWindowSeconds = qMax(1.0, seconds);
    resetRange();
}

double ChartXAxisController::visibleWindowSeconds() const
{
    return m_visibleWindowSeconds;
}

void ChartXAxisController::applyTicker()
{
    if (!m_chart)
        return;

    if (m_mode == ElapsedSeconds) {
        QSharedPointer<QCPAxisTickerFixed> fixedTicker(new QCPAxisTickerFixed);
        fixedTicker->setTickStep(1.0);
        fixedTicker->setScaleStrategy(QCPAxisTickerFixed::ssMultiples);
        m_chart->xAxis->setTicker(fixedTicker);
        return;
    }

    QSharedPointer<QCPAxisTickerDateTime> dateTicker(new QCPAxisTickerDateTime);
    dateTicker->setDateTimeFormat("hh:mm:ss");
    dateTicker->setTickStepStrategy(QCPAxisTicker::tssMeetTickCount);
    dateTicker->setTickCount(6);
    m_chart->xAxis->setTicker(dateTicker);
}

void ChartXAxisController::applyAxisLabel()
{
    if (!m_chart)
        return;

    if (m_mode == ElapsedSeconds) {
        m_chart->xAxis->setLabel("Elapsed time /s");
        return;
    }

    m_chart->xAxis->setLabel("Sampling time");
}
