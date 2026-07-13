#include "chartpage.h"
#include "ui_chartpage.h"

#include "datamonitorchartplotmanager.h"
#include "foupofohbinfo.h"
#include "ohbdeviceconfig.h"
#include "purgetaskconfig.h"
#include "qcustomplot.h"
#include "scheduler/scheduler.h"
#include "shareddata.h"

#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QTimer>
#include <QUrl>

const QString ChartPage::kPlotId = QStringLiteral("purgeTaskPlot");

ChartPage::ChartPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChartPage)
{
    ui->setupUi(this);
    setupPage();
}

ChartPage::~ChartPage()
{
    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }

    const auto manager = DataMonitorChartPlotManager::getInstance();
    if (manager && manager->containsPlot(kPlotId)) {
        manager->unregisterPlot(kPlotId);
    }

    delete ui;
}

void ChartPage::setupPage()
{
    if (ui->label) {
        ui->verticalLayout->removeWidget(ui->label);
        delete ui->label;
        ui->label = nullptr;
    }

    ui->verticalLayout->setContentsMargins(12, 12, 12, 12);
    ui->verticalLayout->setSpacing(10);

    setupChart();
    setupGraphControls();
    setupControls();
    loadQRCodes();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(100);
    connect(m_refreshTimer, &QTimer::timeout, this, &ChartPage::refreshChartData);
    m_refreshTimer->start();

    setTaskRunning(false);
    updateStatusText(tr("Ready"));
}

void ChartPage::setupChart()
{
    m_chart = new QCustomPlot(this);
    m_chart->setMinimumHeight(320);
    ui->verticalLayout->addWidget(m_chart, 1);

    const auto manager = DataMonitorChartPlotManager::getInstance();
    if (!manager) {
        return;
    }

    if (manager->containsPlot(kPlotId)) {
        manager->unregisterPlot(kPlotId);
    }

    if (!manager->registerPlot(kPlotId, m_chart)) {
        return;
    }

    const QList<ChartGraph::Config> graphConfigs = {
        {0, tr("Inlet pressure"), QStringLiteral(" MPa"),
         QPen(QColor(QStringLiteral("#0072B2")), 2, Qt::SolidLine), QCPGraph::lsLine, true, 2},
        {1, tr("Outlet pressure"), QStringLiteral(" kPa"),
         QPen(QColor(QStringLiteral("#E69F00")), 2, Qt::DashLine), QCPGraph::lsLine, true, 2},
        {2, tr("Inlet flow"), QStringLiteral(" L/Min"),
         QPen(QColor(QStringLiteral("#009E73")), 2, Qt::SolidLine), QCPGraph::lsLine, true, 2},
        {3, tr("Relative humidity"), QStringLiteral(" %"),
         QPen(QColor(QStringLiteral("#CC79A7")), 2, Qt::DashDotLine), QCPGraph::lsLine, true, 2},
        {4, tr("Temperature"), QStringLiteral(" C"),
         QPen(QColor(QStringLiteral("#D55E00")), 2, Qt::DotLine), QCPGraph::lsLine, true, 2}
    };

    for (const ChartGraph::Config &config : graphConfigs) {
        manager->addGraph(kPlotId, config);
    }

    m_chart->setBackground(QBrush(QColor(QStringLiteral("#DCE6EC"))));
    m_chart->axisRect()->setBackground(QBrush(QColor(QStringLiteral("#F7FAFC"))));
    m_chart->xAxis->grid()->setPen(QPen(QColor(QStringLiteral("#CDD8DF")), 1, Qt::DashLine));
    m_chart->yAxis->grid()->setPen(QPen(QColor(QStringLiteral("#CDD8DF")), 1, Qt::DashLine));
    m_chart->yAxis->setLabel(tr("Realtime value"));
    m_chart->yAxis->setRange(-20.0, 210.0);
    m_chart->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void ChartPage::setupGraphControls()
{
    QHBoxLayout *graphControlsLayout = new QHBoxLayout;
    graphControlsLayout->setSpacing(12);
    graphControlsLayout->addWidget(new QLabel(tr("Curves:"), this));

    const QStringList graphNames = {
        tr("Inlet pressure"),
        tr("Outlet pressure"),
        tr("Inlet flow"),
        tr("Relative humidity"),
        tr("Temperature")
    };
    const QStringList graphColors = {
        QStringLiteral("#0072B2"),
        QStringLiteral("#B77700"),
        QStringLiteral("#007F5F"),
        QStringLiteral("#A64E87"),
        QStringLiteral("#B54C00")
    };

    const auto manager = DataMonitorChartPlotManager::getInstance();
    m_graphVisibilityChecks.reserve(graphNames.size());
    for (int graphIndex = 0; graphIndex < graphNames.size(); ++graphIndex) {
        QCheckBox *checkBox = new QCheckBox(graphNames.at(graphIndex), this);
        checkBox->setChecked(true);
        checkBox->setStyleSheet(QStringLiteral("QCheckBox { color: %1; font-weight: 600; }")
                                    .arg(graphColors.at(graphIndex)));
        connect(checkBox, &QCheckBox::toggled, this, [manager, graphIndex](bool checked) {
            if (manager) {
                manager->hideGraph(kPlotId, graphIndex, !checked);
            }
        });

        m_graphVisibilityChecks.append(checkBox);
        graphControlsLayout->addWidget(checkBox);
    }

    graphControlsLayout->addStretch(1);
    m_appendDataCheckBox = new QCheckBox(tr("Append curve data"), this);
    m_appendDataCheckBox->setChecked(true);
    m_appendDataCheckBox->setToolTip(tr("Uncheck to pause adding new points to all curves."));
    graphControlsLayout->addWidget(m_appendDataCheckBox);

    ui->verticalLayout->addLayout(graphControlsLayout);
}

void ChartPage::setupControls()
{
    QHBoxLayout *controlsLayout = new QHBoxLayout;
    controlsLayout->setSpacing(8);

    QLabel *qrCodeLabel = new QLabel(tr("QRCode:"), this);
    m_qrCodeCombo = new QComboBox(this);
    m_qrCodeCombo->setMinimumWidth(140);

    m_startButton = new QPushButton(tr("Start task"), this);
    m_stopButton = new QPushButton(tr("End task"), this);
    m_openRecordButton = new QPushButton(tr("Open record"), this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setMinimumWidth(220);

    controlsLayout->addWidget(qrCodeLabel);
    controlsLayout->addWidget(m_qrCodeCombo);
    controlsLayout->addSpacing(12);
    controlsLayout->addWidget(m_startButton);
    controlsLayout->addWidget(m_stopButton);
    controlsLayout->addWidget(m_openRecordButton);
    controlsLayout->addStretch(1);
    controlsLayout->addWidget(m_statusLabel);

    ui->verticalLayout->addLayout(controlsLayout);

    connect(m_qrCodeCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &ChartPage::onQRCodeChanged);
    connect(m_startButton, &QPushButton::clicked, this, &ChartPage::startPurgeTask);
    connect(m_stopButton, &QPushButton::clicked, this, &ChartPage::requestStopPurgeTask);
    connect(m_openRecordButton, &QPushButton::clicked, this, &ChartPage::openRecordDirectory);
}

void ChartPage::loadQRCodes()
{
    if (!m_qrCodeCombo) {
        return;
    }

    const QString defaultQRCode = PurgeTaskConfig::getInstance().readDefaultQRCode().trimmed();
    const QVector<QString> configuredQRCodes = OHBDeviceConfig::getInstance().readQRCodes();
    QSet<QString> addedQRCodes;

    m_qrCodeCombo->blockSignals(true);
    m_qrCodeCombo->clear();

    for (const QString &qrCodeValue : configuredQRCodes) {
        const QString qrCode = qrCodeValue.trimmed();
        if (qrCode.isEmpty() || addedQRCodes.contains(qrCode)) {
            continue;
        }

        addedQRCodes.insert(qrCode);
        m_qrCodeCombo->addItem(qrCode, qrCode);
    }

    if (m_qrCodeCombo->count() == 0 && !defaultQRCode.isEmpty()) {
        m_qrCodeCombo->addItem(defaultQRCode, defaultQRCode);
    }

    const int defaultIndex = m_qrCodeCombo->findData(defaultQRCode);
    m_qrCodeCombo->setCurrentIndex(defaultIndex >= 0 ? defaultIndex : 0);
    m_qrCodeCombo->blockSignals(false);
    onQRCodeChanged();
}

void ChartPage::onQRCodeChanged()
{
    const auto manager = DataMonitorChartPlotManager::getInstance();
    if (manager) {
        manager->clearAllGraphData(kPlotId);
    }
}

void ChartPage::refreshChartData()
{
    if (!m_appendDataCheckBox || !m_appendDataCheckBox->isChecked()) {
        return;
    }

    const QString qrCode = selectedQRCode();
    if (qrCode.isEmpty()) {
        return;
    }

    const FoupOfOHBInfo *foup = SharedData::getFoupByQRCode(qrCode);
    if (!foup) {
        return;
    }

    const auto manager = DataMonitorChartPlotManager::getInstance();
    if (!manager) {
        return;
    }

    manager->refreshGraphs(kPlotId,
                           QVector<double>{foup->inletPressure(),
                                           foup->negativePressure(),
                                           foup->inletFlow(),
                                           foup->RH(),
                                           foup->temperature()});
}

void ChartPage::startPurgeTask()
{
    if (m_runningTask) {
        return;
    }

    const QString qrCode = selectedQRCode();
    if (qrCode.isEmpty()) {
        QMessageBox::warning(this, tr("Purge task"), tr("Please select an OHB device."));
        return;
    }

    PurgeTaskDefinition definition = PurgeTaskConfig::getInstance().readTaskDefinition(qrCode);
    QString errorMessage;
    if (!definition.isValid(&errorMessage)) {
        QMessageBox::warning(this, tr("Purge task"), errorMessage);
        return;
    }

    PurgeTask *task = new PurgeTask(definition);
    m_runningTask = task;
    m_currentOutputDir.clear();

    connect(task, &PurgeTask::outputDirectoryReady, this, [this](const QString &outputDir) {
        m_currentOutputDir = outputDir;
    });
    connect(task, &PurgeTask::purgeStageStarted, this,
            [this](int stageNo, const QString &stageName, int durationSeconds) {
        updateStatusText(tr("Stage %1: %2 (%3 s)")
                             .arg(stageNo)
                             .arg(stageName)
                             .arg(durationSeconds));
    });
    connect(task, &PurgeTask::purgeActionFinished, this,
            [this](int stageNo,
                   int actionNo,
                   const QString &commandId,
                   bool success,
                   const QString &message) {
        if (!success) {
            updateStatusText(tr("Stage %1 action %2 failed: %3 (%4)")
                                 .arg(stageNo)
                                 .arg(actionNo)
                                 .arg(commandId, message));
        }
    });
    connect(task, &PurgeTask::purgeFinished,
            this, &ChartPage::onPurgeTaskFinished,
            Qt::QueuedConnection);

    setTaskRunning(true);
    updateStatusText(tr("Waiting for purge task to start..."));

    const QString taskId = Scheduler::instance()->submitTask(task);
    if (taskId.isEmpty()) {
        m_runningTask.clear();
        task->deleteLater();
        setTaskRunning(false);
        updateStatusText(tr("Failed to submit purge task"));
        QMessageBox::warning(this, tr("Purge task"), tr("Failed to submit the purge task."));
    }
}

void ChartPage::requestStopPurgeTask()
{
    if (!m_runningTask) {
        return;
    }

    m_stopButton->setEnabled(false);
    updateStatusText(tr("Stopping purge task..."));
    QMetaObject::invokeMethod(m_runningTask.data(), "stop", Qt::QueuedConnection);
}

void ChartPage::openRecordDirectory()
{
    if (!isValidRecordDirectory(m_currentOutputDir)) {
        m_openRecordButton->setEnabled(false);
        QMessageBox::warning(this,
                             tr("Purge record"),
                             tr("No valid purge record directory is available."));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_currentOutputDir))) {
        QMessageBox::warning(this,
                             tr("Purge record"),
                             tr("Unable to open the purge record directory."));
    }
}

void ChartPage::onPurgeTaskFinished(bool success,
                                    const QString &message,
                                    const QString &outputDir)
{
    m_runningTask.clear();
    if (!outputDir.trimmed().isEmpty()) {
        m_currentOutputDir = outputDir;
    }

    setTaskRunning(false);
    const bool hasValidRecord = isValidRecordDirectory(m_currentOutputDir);
    m_openRecordButton->setEnabled(hasValidRecord);

    if (success) {
        if (hasValidRecord) {
            saveChartSnapshot(m_currentOutputDir);
        }
        updateStatusText(tr("Purge task finished"));
        QMessageBox::information(this, tr("Purge task"), tr("Purge task finished successfully."));
        return;
    }

    updateStatusText(tr("Purge task failed: %1").arg(message));
    QMessageBox::warning(this,
                         tr("Purge task"),
                         tr("Purge task did not finish successfully.\n%1").arg(message));
}

void ChartPage::setTaskRunning(bool running)
{
    if (m_qrCodeCombo) {
        m_qrCodeCombo->setEnabled(!running);
    }
    if (m_startButton) {
        m_startButton->setEnabled(!running && m_qrCodeCombo && m_qrCodeCombo->count() > 0);
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(running);
    }
    if (m_openRecordButton) {
        m_openRecordButton->setEnabled(!running && isValidRecordDirectory(m_currentOutputDir));
    }
}

void ChartPage::updateStatusText(const QString &text)
{
    if (m_statusLabel) {
        m_statusLabel->setText(text);
        m_statusLabel->setToolTip(text);
    }
}

QString ChartPage::selectedQRCode() const
{
    if (!m_qrCodeCombo || m_qrCodeCombo->currentIndex() < 0) {
        return QString();
    }

    const QString qrCode = m_qrCodeCombo->currentData().toString().trimmed();
    return qrCode.isEmpty() ? m_qrCodeCombo->currentText().trimmed() : qrCode;
}

bool ChartPage::isValidRecordDirectory(const QString &dirPath) const
{
    const QDir dir(dirPath);
    if (dirPath.trimmed().isEmpty() || !dir.exists()) {
        return false;
    }

    QFile allStageFile(dir.filePath(QStringLiteral("stage_all.csv")));
    if (!allStageFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    allStageFile.readLine();
    return !allStageFile.readLine().trimmed().isEmpty();
}

void ChartPage::saveChartSnapshot(const QString &dirPath)
{
    if (!m_chart || !QDir(dirPath).exists()) {
        return;
    }

    const QString snapshotPath = QDir(dirPath).filePath(QStringLiteral("chart_snapshot.png"));
    m_chart->grab().save(snapshotPath, "PNG");
}
