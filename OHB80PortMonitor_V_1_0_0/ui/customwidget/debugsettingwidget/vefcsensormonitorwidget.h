/*******************************************************************************************
 * @file vefcsensormonitorwidget.h
 * @author Simon <工号：13> 2026-06-29
 *
 * @class VEFCSensorMonitorWidget
 * @brief DebugPage 中用于展示 VEFC 监控快照数据的调试控件。
 *
 * 设计目标：
 *      1. 以 Tab 形式分开展示设备摘要记录和当日定时采集记录，降低单页信息拥挤度。
 *      2. 通过 QRCode 过滤同一份调度快照数据，避免 UI 层重复向下游查询。
 *      3. 保持展示层只负责筛选和渲染，调度层继续负责快照聚合与数据来源管理。
 *******************************************************************************************/
#ifndef VEFC_SENSOR_MONITOR_WIDGET_H
#define VEFC_SENSOR_MONITOR_WIDGET_H

#include "../settingwidget/settingwidget.h"
#include "scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_types.h"

class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QWidget;
class VEFCSensorMonitorTask;

class VEFCSensorMonitorWidget : public SettingWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit VEFCSensorMonitorWidget(QWidget *parent = nullptr);
    ~VEFCSensorMonitorWidget() override = default;

private:
    // ============================ 界面构建 ============================
    void initUI();
    void bindTask();
    QWidget *createMainWidget();
    QWidget *createOverviewTab();
    QWidget *createDailyRecordsTab();
    QWidget *createTableSection(const QString &title,
                                QTableWidget *table,
                                QWidget *parent = nullptr) const;
    QTableWidget *createRecordTable(QWidget *parent, bool timeFirst) const;
    QTableWidget *createStatisticsTable(QWidget *parent) const;

    // ============================ 数据处理 ============================
    void initQrcodeSpinBox(QSpinBox *spinBox);
    QString getSelectedQrCode(QSpinBox *spinBox) const;
    QVector<VEFCSensorMonitorRecord> filterRecords(
        const QVector<VEFCSensorMonitorRecord> &records,
        const QString &qrCode) const;
    QVector<VEFCSensorMonitor::DebugMetricStats> buildStatistics(
        const QVector<VEFCSensorMonitorRecord> &records) const;
    void fillRecordTable(QTableWidget *table,
                         const QVector<VEFCSensorMonitorRecord> &records,
                         bool timeFirst);
    void fillStatisticsTable(const QVector<VEFCSensorMonitor::DebugMetricStats> &statistics);
    static QString formatDouble(double value);

private slots:
    // ---- 数据刷新 ----
    void requestSnapshot();
    void updateSnapshot(const VEFCSensorMonitor::DebugSnapshot &snapshot);
    void refreshVisibleTables();

private:
    // ---- 状态成员 ----
    VEFCSensorMonitorTask *m_task;
    VEFCSensorMonitor::DebugSnapshot m_snapshot;

    // ---- 功能模块成员 ----
    QLabel *m_statusLabel;
    QSpinBox *m_tab1QrcodeSpinBox;
    QSpinBox *m_tab2QrcodeSpinBox;
    QTableWidget *m_softwareFirstOpenTable;
    QTableWidget *m_todayFirstTable;
    QTableWidget *m_todayLatestTable;
    QTableWidget *m_todayRecordsTable;
    QTableWidget *m_statisticsTable;
    QPushButton *m_refreshButton;
};

#endif // VEFC_SENSOR_MONITOR_WIDGET_H
