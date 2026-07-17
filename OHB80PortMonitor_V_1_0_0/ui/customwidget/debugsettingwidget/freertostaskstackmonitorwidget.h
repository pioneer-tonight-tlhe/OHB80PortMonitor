#ifndef FREE_RTOS_TASK_STACK_MONITOR_WIDGET_H
#define FREE_RTOS_TASK_STACK_MONITOR_WIDGET_H

#include "../settingwidget/settingwidget.h"

#include <QVector>

class FreeRTOSTaskStackMonitorTask;
class QTableWidget;

class FreeRTOSTaskStackMonitorWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit FreeRTOSTaskStackMonitorWidget(QWidget *parent = nullptr);
    ~FreeRTOSTaskStackMonitorWidget() override = default;

private:
    void initUI();
    void bindTask();
    static QTableWidget *createTable(QWidget *parent);
    void appendRow(const QVector<int> &values);

private slots:
    void onStackWaterLevelsUpdated(const QVector<int> &values);

private:
    FreeRTOSTaskStackMonitorTask *m_task = nullptr;
    QTableWidget *m_table = nullptr;
};

#endif // FREE_RTOS_TASK_STACK_MONITOR_WIDGET_H
