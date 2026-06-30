#include "vefcsensormonitorwidget.h"

#include "app/shareddata.h"
#include "scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QStringList sortedNumericQrcodes(QStringList qrcodes)
{
    qrcodes.removeDuplicates();
    std::sort(qrcodes.begin(), qrcodes.end(), [](const QString &lhs, const QString &rhs) {
        bool leftOk = false;
        bool rightOk = false;
        const int left = lhs.toInt(&leftOk);
        const int right = rhs.toInt(&rightOk);
        if (leftOk && rightOk) {
            return left < right;
        }
        return lhs < rhs;
    });
    return qrcodes;
}

void addMetricStat(QVector<VEFCSensorMonitor::DebugMetricStats> &statistics,
                   const QString &name,
                   const QString &unit,
                   const QVector<VEFCSensorMonitorRecord> &records,
                   double VEFCSensorMonitorRecord::*member)
{
    VEFCSensorMonitor::DebugMetricStats stat;
    stat.name = name;
    stat.unit = unit;
    stat.count = records.size();
    stat.hasData = !records.isEmpty();
    if (!stat.hasData) {
        statistics.append(stat);
        return;
    }

    stat.min = records.first().*member;
    stat.max = records.first().*member;
    double sum = 0.0;
    for (const VEFCSensorMonitorRecord &record : records) {
        const double value = record.*member;
        stat.min = qMin(stat.min, value);
        stat.max = qMax(stat.max, value);
        sum += value;
    }
    stat.average = sum / records.size();
    statistics.append(stat);
}
}

VEFCSensorMonitorWidget::VEFCSensorMonitorWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_task(nullptr)
    , m_snapshot()
    , m_statusLabel(nullptr)
    , m_tab1QrcodeSpinBox(nullptr)
    , m_tab2QrcodeSpinBox(nullptr)
    , m_softwareFirstOpenTable(nullptr)
    , m_todayFirstTable(nullptr)
    , m_todayLatestTable(nullptr)
    , m_todayRecordsTable(nullptr)
    , m_statisticsTable(nullptr)
    , m_refreshButton(nullptr)
{
    setTitle(QStringLiteral("VEFC Sensor Monitor"));
    initUI();
    bindTask();
    requestSnapshot();
}

void VEFCSensorMonitorWidget::initUI()
{
    addCustomWidget(createMainWidget());

    connect(m_refreshButton, &QPushButton::clicked,
            this, &VEFCSensorMonitorWidget::requestSnapshot);
    connect(m_tab1QrcodeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VEFCSensorMonitorWidget::refreshVisibleTables);
    connect(m_tab2QrcodeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &VEFCSensorMonitorWidget::refreshVisibleTables);
}

void VEFCSensorMonitorWidget::bindTask()
{
    m_task = SharedData::getVEFCSensorMonitorTask();
    if (!m_task) {
        if (m_statusLabel) {
            m_statusLabel->setText(QStringLiteral("VEFC monitor task is not available"));
        }
        return;
    }

    connect(m_task, &VEFCSensorMonitorTask::debugSnapshotUpdated,
            this, &VEFCSensorMonitorWidget::updateSnapshot,
            Qt::QueuedConnection);
}

void VEFCSensorMonitorWidget::requestSnapshot()
{
    if (!m_task) {
        bindTask();
    }
    if (!m_task) {
        if (m_statusLabel) {
            m_statusLabel->setText(QStringLiteral("VEFC monitor task is not available"));
        }
        return;
    }

    QMetaObject::invokeMethod(m_task,
                              "publishDebugSnapshot",
                              Qt::QueuedConnection);
}

void VEFCSensorMonitorWidget::updateSnapshot(const VEFCSensorMonitor::DebugSnapshot &snapshot)
{
    m_snapshot = snapshot;
    refreshVisibleTables();

    if (m_statusLabel) {
        m_statusLabel->setText(snapshot.databaseAvailable
                                   ? QStringLiteral("Date: %1")
                                         .arg(snapshot.date.toString(QStringLiteral("yyyy-MM-dd")))
                                   : snapshot.errorMessage);
    }
}

void VEFCSensorMonitorWidget::refreshVisibleTables()
{
    const QString tab1QrCode = getSelectedQrCode(m_tab1QrcodeSpinBox);
    fillRecordTable(m_softwareFirstOpenTable,
                    filterRecords(m_snapshot.softwareFirstOpenRecords, tab1QrCode),
                    false);
    fillRecordTable(m_todayFirstTable,
                    filterRecords(m_snapshot.todayFirstRecords, tab1QrCode),
                    false);
    fillRecordTable(m_todayLatestTable,
                    filterRecords(m_snapshot.todayLatestRecords, tab1QrCode),
                    false);

    const QString tab2QrCode = getSelectedQrCode(m_tab2QrcodeSpinBox);
    const QVector<VEFCSensorMonitorRecord> todayRecords
        = filterRecords(m_snapshot.todayRecords, tab2QrCode);
    fillRecordTable(m_todayRecordsTable, todayRecords, true);
    fillStatisticsTable(buildStatistics(todayRecords));
}

QWidget *VEFCSensorMonitorWidget::createMainWidget()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *headerRow = new QWidget(page);
    auto *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(8);

    m_statusLabel = new QLabel(QStringLiteral("Waiting for VEFC monitor snapshot"), headerRow);
    headerLayout->addWidget(m_statusLabel);
    headerLayout->addStretch();

    m_refreshButton = new QPushButton(QStringLiteral("Refresh"), headerRow);
    headerLayout->addWidget(m_refreshButton);
    layout->addWidget(headerRow);

    auto *tabs = new QTabWidget(page);
    tabs->addTab(createOverviewTab(), QStringLiteral("Overview"));
    tabs->addTab(createDailyRecordsTab(), QStringLiteral("Today Records"));
    layout->addWidget(tabs);

    return page;
}

QWidget *VEFCSensorMonitorWidget::createOverviewTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *filterRow = new QWidget(page);
    auto *filterLayout = new QFormLayout(filterRow);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_tab1QrcodeSpinBox = new QSpinBox(filterRow);
    initQrcodeSpinBox(m_tab1QrcodeSpinBox);
    filterLayout->addRow(QStringLiteral("qrcode:"), m_tab1QrcodeSpinBox);
    layout->addWidget(filterRow);

    m_softwareFirstOpenTable = createRecordTable(page, false);
    layout->addWidget(createTableSection(QStringLiteral("Software First Open"), m_softwareFirstOpenTable, page));

    m_todayFirstTable = createRecordTable(page, false);
    layout->addWidget(createTableSection(QStringLiteral("Today First"), m_todayFirstTable, page));

    m_todayLatestTable = createRecordTable(page, false);
    layout->addWidget(createTableSection(QStringLiteral("Today Latest"), m_todayLatestTable, page));

    return page;
}

QWidget *VEFCSensorMonitorWidget::createDailyRecordsTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *filterRow = new QWidget(page);
    auto *filterLayout = new QFormLayout(filterRow);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_tab2QrcodeSpinBox = new QSpinBox(filterRow);
    initQrcodeSpinBox(m_tab2QrcodeSpinBox);
    filterLayout->addRow(QStringLiteral("qrcode:"), m_tab2QrcodeSpinBox);
    layout->addWidget(filterRow);

    m_todayRecordsTable = createRecordTable(page, true);
    layout->addWidget(createTableSection(QStringLiteral("Today Timed VEFC Records"), m_todayRecordsTable, page));

    m_statisticsTable = createStatisticsTable(page);
    layout->addWidget(createTableSection(QStringLiteral("Today VEFC Statistics"), m_statisticsTable, page));

    return page;
}

QWidget *VEFCSensorMonitorWidget::createTableSection(const QString &title,
                                                     QTableWidget *table,
                                                     QWidget *parent) const
{
    auto *section = new QWidget(parent);
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto *label = new QLabel(title, section);
    layout->addWidget(label);
    layout->addWidget(table);
    return section;
}

QTableWidget *VEFCSensorMonitorWidget::createRecordTable(QWidget *parent, bool timeFirst) const
{
    auto *table = new QTableWidget(parent);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(timeFirst
        ? QStringList{
              QStringLiteral("Time"),
              QStringLiteral("QRCode"),
              QStringLiteral("Gas Pressure"),
              QStringLiteral("Actual Flow"),
              QStringLiteral("VEFC Pressure"),
              QStringLiteral("VEFC Temperature")}
        : QStringList{
              QStringLiteral("QRCode"),
              QStringLiteral("Time"),
              QStringLiteral("Gas Pressure"),
              QStringLiteral("Actual Flow"),
              QStringLiteral("VEFC Pressure"),
              QStringLiteral("VEFC Temperature")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setMinimumHeight(150);
    return table;
}

QTableWidget *VEFCSensorMonitorWidget::createStatisticsTable(QWidget *parent) const
{
    auto *table = new QTableWidget(parent);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels(QStringList{
        QStringLiteral("Field"),
        QStringLiteral("Count"),
        QStringLiteral("Min"),
        QStringLiteral("Max"),
        QStringLiteral("Avg"),
        QStringLiteral("Unit")
    });
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setMinimumHeight(150);
    return table;
}

void VEFCSensorMonitorWidget::initQrcodeSpinBox(QSpinBox *spinBox)
{
    if (!spinBox) {
        return;
    }

    const QStringList qrcodes = sortedNumericQrcodes(SharedData::getAllQrcodes());
    if (qrcodes.isEmpty()) {
        spinBox->setRange(0, 0);
        spinBox->setValue(0);
        return;
    }

    bool minOk = false;
    bool maxOk = false;
    const int minValue = qrcodes.first().toInt(&minOk);
    const int maxValue = qrcodes.last().toInt(&maxOk);
    if (minOk && maxOk) {
        spinBox->setRange(minValue, maxValue);
        spinBox->setValue(minValue);
    } else {
        spinBox->setRange(0, 99999);
        spinBox->setValue(0);
    }
}

QString VEFCSensorMonitorWidget::getSelectedQrCode(QSpinBox *spinBox) const
{
    return spinBox ? QString::number(spinBox->value()) : QString();
}

QVector<VEFCSensorMonitorRecord> VEFCSensorMonitorWidget::filterRecords(
    const QVector<VEFCSensorMonitorRecord> &records,
    const QString &qrCode) const
{
    if (qrCode.isEmpty()) {
        return records;
    }

    QVector<VEFCSensorMonitorRecord> filtered;
    for (const VEFCSensorMonitorRecord &record : records) {
        if (record.qrCode == qrCode) {
            filtered.append(record);
        }
    }
    return filtered;
}

QVector<VEFCSensorMonitor::DebugMetricStats> VEFCSensorMonitorWidget::buildStatistics(
    const QVector<VEFCSensorMonitorRecord> &records) const
{
    QVector<VEFCSensorMonitor::DebugMetricStats> statistics;
    addMetricStat(statistics,
                  QStringLiteral("Gas Pressure"),
                  QStringLiteral("KPa"),
                  records,
                  &VEFCSensorMonitorRecord::gasPressure);
    addMetricStat(statistics,
                  QStringLiteral("Actual Flow"),
                  QStringLiteral("L/Min"),
                  records,
                  &VEFCSensorMonitorRecord::actualFlow);
    addMetricStat(statistics,
                  QStringLiteral("VEFC Pressure"),
                  QStringLiteral("KPa"),
                  records,
                  &VEFCSensorMonitorRecord::sensorPressure);
    addMetricStat(statistics,
                  QStringLiteral("VEFC Temperature"),
                  QStringLiteral("C"),
                  records,
                  &VEFCSensorMonitorRecord::sensorTemperature);
    return statistics;
}

void VEFCSensorMonitorWidget::fillRecordTable(QTableWidget *table,
                                              const QVector<VEFCSensorMonitorRecord> &records,
                                              bool timeFirst)
{
    if (!table) {
        return;
    }

    table->clearContents();
    table->setRowCount(records.size());
    for (int row = 0; row < records.size(); ++row) {
        const VEFCSensorMonitorRecord &record = records.at(row);
        const QStringList values = timeFirst
            ? QStringList{
                  record.recordTimeString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                  record.qrCode,
                  formatDouble(record.gasPressure),
                  formatDouble(record.actualFlow),
                  formatDouble(record.sensorPressure),
                  formatDouble(record.sensorTemperature)}
            : QStringList{
                  record.qrCode,
                  record.recordTimeString(QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                  formatDouble(record.gasPressure),
                  formatDouble(record.actualFlow),
                  formatDouble(record.sensorPressure),
                  formatDouble(record.sensorTemperature)};

        for (int col = 0; col < values.size(); ++col) {
            table->setItem(row, col, new QTableWidgetItem(values.at(col)));
        }
    }
}

void VEFCSensorMonitorWidget::fillStatisticsTable(
    const QVector<VEFCSensorMonitor::DebugMetricStats> &statistics)
{
    if (!m_statisticsTable) {
        return;
    }

    m_statisticsTable->clearContents();
    m_statisticsTable->setRowCount(statistics.size());
    for (int row = 0; row < statistics.size(); ++row) {
        const VEFCSensorMonitor::DebugMetricStats &stat = statistics.at(row);
        const QStringList values{
            stat.name,
            QString::number(stat.count),
            stat.hasData ? formatDouble(stat.min) : QStringLiteral("N/A"),
            stat.hasData ? formatDouble(stat.max) : QStringLiteral("N/A"),
            stat.hasData ? formatDouble(stat.average) : QStringLiteral("N/A"),
            stat.unit
        };

        for (int col = 0; col < values.size(); ++col) {
            m_statisticsTable->setItem(row, col, new QTableWidgetItem(values.at(col)));
        }
    }
}

QString VEFCSensorMonitorWidget::formatDouble(double value)
{
    return QString::number(value, 'f', 2);
}
