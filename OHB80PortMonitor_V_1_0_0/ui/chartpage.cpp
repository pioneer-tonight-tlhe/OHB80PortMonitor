#include "chartpage.h"
#include "ui_chartpage.h"

#include "datamonitorchartplotmanager.h"
#include "foupofohbinfo.h"
#include "ohbdeviceconfig.h"
#include "scheduler/tasks/purge_task/purge_data_recorder.h"
#include "purgetaskconfig.h"
#include "qcustomplot.h"
#include "scheduler/scheduler.h"
#include "shareddata.h"

#include <QAbstractSpinBox>
#include <QBrush>
#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFrame>
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
    if (m_dataRecorder) {
        m_dataRecorder->stop();
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

    setupControls();
    setupGraphControls();
    setupChart();
    loadQRCodes();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(100);
    connect(m_refreshTimer, &QTimer::timeout, this, &ChartPage::refreshChartData);
    m_refreshTimer->start();

    setTaskRunning(false);
    updateControlStates();
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
    m_chart->yAxis->setRange(-20.0, 100.0);
    m_chart->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
}

void ChartPage::setupGraphControls()
{
    QFrame *graphControlsPanel = new QFrame(this);
    graphControlsPanel->setObjectName(QStringLiteral("graphControlsPanel"));
    graphControlsPanel->setStyleSheet(QStringLiteral(
        "QFrame#graphControlsPanel {"
        " background-color: #EDF3F6;"
        " border: 1px solid #C6D3DA;"
        " border-radius: 6px;"
        "}"));

    QHBoxLayout *graphControlsLayout = new QHBoxLayout(graphControlsPanel);
    graphControlsLayout->setContentsMargins(12, 8, 12, 8);
    graphControlsLayout->setSpacing(12);
    graphControlsLayout->addWidget(new QLabel(tr("Curves:"), graphControlsPanel));

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
        QCheckBox *checkBox = new QCheckBox(graphNames.at(graphIndex), graphControlsPanel);
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
    m_elapsedTimeCheckBox = new QCheckBox(tr("Curve append mode (seconds)"), graphControlsPanel);
    m_elapsedTimeCheckBox->setChecked(false);
    m_elapsedTimeCheckBox->setToolTip(
        tr("When checked, the X axis uses elapsed seconds; otherwise it displays hh:mm:ss."));
    connect(m_elapsedTimeCheckBox, &QCheckBox::stateChanged, this, [this, manager](int state) {
        if (manager) {
            manager->setXAxisMode(kPlotId, state);
        }
        resetRecordingSession();
    });
    graphControlsLayout->addWidget(m_elapsedTimeCheckBox);

    m_chartPauseButton = new QPushButton(tr("Pause chart"), graphControlsPanel);
    connect(m_chartPauseButton,
            &QPushButton::clicked,
            this,
            &ChartPage::toggleChartPaused);
    graphControlsLayout->addWidget(m_chartPauseButton);

    ui->verticalLayout->addWidget(graphControlsPanel);
}

void ChartPage::setupControls()
{
    QFrame *controlsPanel = new QFrame(this);
    controlsPanel->setObjectName(QStringLiteral("taskControlsPanel"));
    controlsPanel->setStyleSheet(QStringLiteral(
        "QFrame#taskControlsPanel {"
        " background-color: #E4EDF2;"
        " border: 1px solid #B8C9D3;"
        " border-radius: 6px;"
        "}"));

    QHBoxLayout *controlsLayout = new QHBoxLayout(controlsPanel);
    controlsLayout->setContentsMargins(12, 8, 12, 8);
    controlsLayout->setSpacing(8);

    QLabel *qrCodeLabel = new QLabel(tr("QRCode:"), controlsPanel);
    m_qrCodeCombo = new QComboBox(controlsPanel);
    m_qrCodeCombo->setMinimumWidth(140);

    m_recordButton = new QPushButton(tr("Start recording"), controlsPanel);
    m_startButton = new QPushButton(tr("Start task"), controlsPanel);
    m_stopButton = new QPushButton(tr("End task"), controlsPanel);
    QLabel *markerTimeLabel = new QLabel(tr("Marker time:"), controlsPanel);
    m_markerTimeSpinBox = new QDoubleSpinBox(controlsPanel);
    m_markerTimeSpinBox->setRange(0.0, 86400.0);
    m_markerTimeSpinBox->setDecimals(1);
    m_markerTimeSpinBox->setSingleStep(0.5);
    m_markerTimeSpinBox->setSuffix(tr(" s"));
    m_markerTimeSpinBox->setReadOnly(true);
    m_markerTimeSpinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_markerTimeSpinBox->setFixedWidth(100);
    m_markerButton = new QPushButton(tr("Mark"), controlsPanel);
    m_openRecordButton = new QPushButton(tr("Open record"), controlsPanel);
    m_statusLabel = new QLabel(controlsPanel);
    m_statusLabel->setMinimumWidth(220);

    controlsLayout->addWidget(qrCodeLabel);
    controlsLayout->addWidget(m_qrCodeCombo);
    controlsLayout->addSpacing(12);
    controlsLayout->addWidget(m_recordButton);
    controlsLayout->addWidget(m_startButton);
    controlsLayout->addWidget(m_stopButton);
    controlsLayout->addSpacing(12);
    controlsLayout->addWidget(markerTimeLabel);
    controlsLayout->addWidget(m_markerTimeSpinBox);
    controlsLayout->addWidget(m_markerButton);
    controlsLayout->addWidget(m_openRecordButton);
    controlsLayout->addStretch(1);
    controlsLayout->addWidget(m_statusLabel);

    ui->verticalLayout->addWidget(controlsPanel);

    connect(m_qrCodeCombo,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &ChartPage::onQRCodeChanged);
    connect(m_recordButton, &QPushButton::clicked, this, &ChartPage::toggleDataRecording);
    connect(m_startButton, &QPushButton::clicked, this, &ChartPage::startPurgeTask);
    connect(m_stopButton, &QPushButton::clicked, this, &ChartPage::requestStopPurgeTask);
    connect(m_markerButton, &QPushButton::clicked, this, &ChartPage::addTimeMarker);
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
    m_currentOutputDir.clear();
    const auto manager = DataMonitorChartPlotManager::getInstance();
    if (manager) {
        manager->clearAllGraphData(kPlotId);
    }
    resetRecordingSession();
}

void ChartPage::refreshChartData()
{
    if (m_chartPaused) {
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

    if (manager->refreshGraphs(kPlotId,
                               QVector<double>{foup->inletPressure(),
                                               foup->negativePressure(),
                                               foup->inletFlow(),
                                               foup->RH(),
                                               foup->temperature()})) {
        if (m_isRecording && m_recordingElapsedTimer.isValid()) {
            m_recordedDurationSeconds = m_recordingElapsedTimer.elapsed() / 1000.0;
            if (!m_hasRecordedSamples) {
                m_hasRecordedSamples = true;
                updateControlStates();
            }
        }
    }
}

void ChartPage::toggleDataRecording()
{
    if (m_isRecording) {
        m_recordedDurationSeconds = m_recordingElapsedTimer.elapsed() / 1000.0;
        if (m_dataRecorder) {
            m_dataRecorder->stop();
        }
        m_isRecording = false;
        updateControlStates();
        updateStatusText(tr("Data recording stopped at %1 s")
                             .arg(m_recordedDurationSeconds, 0, 'f', 1));
        return;
    }

    const QString qrCode = selectedQRCode();
    if (qrCode.isEmpty()) {
        QMessageBox::warning(this, tr("Data recording"), tr("Please select an OHB device."));
        return;
    }

    const auto manager = DataMonitorChartPlotManager::getInstance();
    if (!manager || !manager->clearAllGraphData(kPlotId)) {
        QMessageBox::warning(this, tr("Data recording"), tr("Unable to initialize the chart."));
        return;
    }

    if (m_dataRecorder) {
        m_dataRecorder->stop();
        m_dataRecorder->deleteLater();
        m_dataRecorder.clear();
    }

    PurgeDataRecorder *recorder = new PurgeDataRecorder(qrCode, this);
    connect(recorder, &PurgeDataRecorder::recordingFailed,
            this, [this, recorder](const QString &message, const QString &outputDir) {
        if (m_dataRecorder != recorder) {
            return;
        }

        m_recordedDurationSeconds = m_recordingElapsedTimer.isValid()
            ? m_recordingElapsedTimer.elapsed() / 1000.0
            : m_recordedDurationSeconds;
        m_isRecording = false;
        if (!outputDir.isEmpty()) {
            m_currentOutputDir = outputDir;
        }
        updateControlStates();
        updateStatusText(message);
        QMessageBox::warning(this, tr("Data recording"), message);
    });

    QString errorMessage;
    if (!recorder->start(&errorMessage)) {
        recorder->deleteLater();
        QMessageBox::warning(this, tr("Data recording"), errorMessage);
        return;
    }

    m_dataRecorder = recorder;
    m_currentOutputDir = recorder->outputDir();
    m_recordingStartEpochSeconds = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    m_recordingElapsedTimer.restart();
    m_recordedDurationSeconds = 0.0;
    m_hasRecordedSamples = false;
    m_isRecording = true;
    m_chartPaused = false;
    if (m_markerTimeSpinBox) {
        m_markerTimeSpinBox->setValue(0.0);
    }

    updateControlStates();
    updateStatusText(tr("Recording data: QRCode=%1").arg(qrCode));
}

void ChartPage::toggleChartPaused()
{
    m_chartPaused = !m_chartPaused;
    updateControlStates();
    updateStatusText(m_chartPaused ? tr("Chart paused") : tr("Chart resumed"));
}

void ChartPage::addTimeMarker()
{
    if (!m_markerTimeSpinBox || !m_hasRecordedSamples) {
        return;
    }

    const auto manager = DataMonitorChartPlotManager::getInstance();
    double markerX = 0.0;
    if (!manager
        || !manager->addVerticalMarkerAtLatestX(
            kPlotId,
            QPen(QColor(QStringLiteral("#FFD200")), 3, Qt::SolidLine),
            &markerX)) {
        QMessageBox::warning(this, tr("Chart marker"), tr("Unable to add the chart marker."));
        return;
    }

    const bool elapsedMode = m_elapsedTimeCheckBox && m_elapsedTimeCheckBox->isChecked();
    const double markerSeconds = qMax(0.0,
                                      elapsedMode
                                          ? markerX
                                          : markerX - m_recordingStartEpochSeconds);
    m_markerTimeSpinBox->setValue(markerSeconds);
    updateStatusText(tr("Marker added at %1 s").arg(markerSeconds, 0, 'f', 1));
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
    task->setOutputDir(m_currentOutputDir);
    m_runningTask = task;

    connect(task, &PurgeTask::outputDirectoryReady, this, [this](const QString &outputDir) {
        m_currentOutputDir = outputDir;
    });
    connect(task, &PurgeTask::purgeStageStarted, this,
            [this](int stageNo, const QString &stageName, int durationSeconds) {
        if (m_dataRecorder && m_dataRecorder->isRecording()) {
            m_dataRecorder->setCurrentStage(stageNo, stageName);
        }
        updateStatusText(tr("Stage %1: %2 (%3 s)")
                             .arg(stageNo)
                             .arg(stageName)
                             .arg(durationSeconds));
    });
    connect(task, &PurgeTask::purgeStageFinished, this,
            [this](int, const QString &) {
        if (m_dataRecorder && m_dataRecorder->isRecording()) {
            m_dataRecorder->clearCurrentStage();
        }
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
    if (m_dataRecorder && m_dataRecorder->isRecording()) {
        m_dataRecorder->clearCurrentStage();
    }
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
    m_taskRunning = running;
    updateControlStates();
}

void ChartPage::updateControlStates()
{
    const bool hasQRCode = m_qrCodeCombo && m_qrCodeCombo->count() > 0;
    if (m_qrCodeCombo) {
        m_qrCodeCombo->setEnabled(!m_isRecording && !m_taskRunning);
    }
    if (m_recordButton) {
        m_recordButton->setEnabled(hasQRCode && (m_isRecording || !m_taskRunning));
        m_recordButton->setText(m_isRecording ? tr("Stop recording") : tr("Start recording"));
    }
    if (m_startButton) {
        m_startButton->setEnabled(!m_taskRunning && hasQRCode);
    }
    if (m_stopButton) {
        m_stopButton->setEnabled(m_taskRunning);
    }
    if (m_markerTimeSpinBox) {
        m_markerTimeSpinBox->setEnabled(m_hasRecordedSamples);
    }
    if (m_markerButton) {
        m_markerButton->setEnabled(m_hasRecordedSamples);
    }
    if (m_elapsedTimeCheckBox) {
        m_elapsedTimeCheckBox->setEnabled(!m_isRecording);
    }
    if (m_chartPauseButton) {
        m_chartPauseButton->setText(m_chartPaused ? tr("Resume chart") : tr("Pause chart"));
    }
    if (m_openRecordButton) {
        m_openRecordButton->setEnabled(!m_taskRunning && isValidRecordDirectory(m_currentOutputDir));
    }
}

void ChartPage::resetRecordingSession()
{
    if (m_dataRecorder) {
        m_dataRecorder->stop();
        m_dataRecorder->deleteLater();
        m_dataRecorder.clear();
    }
    m_isRecording = false;
    m_chartPaused = false;
    m_hasRecordedSamples = false;
    m_recordingElapsedTimer.invalidate();
    m_recordingStartEpochSeconds = 0.0;
    m_recordedDurationSeconds = 0.0;
    if (m_markerTimeSpinBox) {
        m_markerTimeSpinBox->setValue(0.0);
    }
    updateControlStates();
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
