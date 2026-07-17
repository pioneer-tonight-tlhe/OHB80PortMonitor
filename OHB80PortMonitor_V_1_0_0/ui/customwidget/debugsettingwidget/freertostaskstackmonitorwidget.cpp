#include "freertostaskstackmonitorwidget.h"

#include "app/shareddata.h"
#include "scheduler/tasks/free_rtos_task_stack_monitor_task/free_rtos_task_stack_monitor_task.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

FreeRTOSTaskStackMonitorWidget::FreeRTOSTaskStackMonitorWidget(QWidget *parent)
    : SettingWidget(parent)
{
    setTitle(QStringLiteral("FreeRTOS Task Stack Monitor"));
    initUI();
    bindTask();
}

void FreeRTOSTaskStackMonitorWidget::initUI()
{
    auto *container = new QWidget(this);
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    m_table = createTable(container);
    layout->addWidget(m_table);

    addCustomWidget(container);
}

void FreeRTOSTaskStackMonitorWidget::bindTask()
{
    m_task = SharedData::getFreeRTOSTaskStackMonitorTask();
    if (!m_task) {
        return;
    }

    connect(m_task, &FreeRTOSTaskStackMonitorTask::stackWaterLevelsUpdated,
            this, &FreeRTOSTaskStackMonitorWidget::onStackWaterLevelsUpdated,
            Qt::QueuedConnection);
}

QTableWidget *FreeRTOSTaskStackMonitorWidget::createTable(QWidget *parent)
{
    auto *table = new QTableWidget(parent);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("时间"),
        QStringLiteral("任务1栈"),
        QStringLiteral("任务2栈"),
        QStringLiteral("任务3栈"),
        QStringLiteral("任务4栈")
    });
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setMinimumHeight(220);
    return table;
}

void FreeRTOSTaskStackMonitorWidget::appendRow(const QVector<int> &values)
{
    if (!m_table || values.size() != 4) {
        return;
    }

    m_table->insertRow(0);
    m_table->setItem(0, 0, new QTableWidgetItem(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))));

    for (int i = 0; i < 4; ++i) {
        m_table->setItem(0, i + 1,
                         new QTableWidgetItem(QStringLiteral("%1字").arg(values.at(i))));
    }
}

void FreeRTOSTaskStackMonitorWidget::onStackWaterLevelsUpdated(const QVector<int> &values)
{
    appendRow(values);
}
