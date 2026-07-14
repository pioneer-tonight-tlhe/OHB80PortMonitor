#ifndef CHARTPAGE_H
#define CHARTPAGE_H

#include "scheduler/tasks/purge_task/purge_task.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QTimer;
class QCustomPlot;
class PurgeDataRecorder;

namespace Ui {
class ChartPage;
}

class ChartPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChartPage(QWidget *parent = nullptr);
    ~ChartPage();

private slots:
    void onQRCodeChanged();
    void refreshChartData();
    void toggleDataRecording();
    void toggleChartPaused();
    void addTimeMarker();
    void startPurgeTask();
    void requestStopPurgeTask();
    void openRecordDirectory();
    void onPurgeTaskFinished(bool success, const QString &message, const QString &outputDir);

private:
    void setupPage();
    void setupChart();
    void setupGraphControls();
    void setupControls();
    void loadQRCodes();
    void setTaskRunning(bool running);
    void updateControlStates();
    void resetRecordingSession();
    void updateStatusText(const QString &text);
    QString selectedQRCode() const;
    bool isValidRecordDirectory(const QString &dirPath) const;
    void saveChartSnapshot(const QString &dirPath);

private:
    static const QString kPlotId;

    Ui::ChartPage *ui;
    QCustomPlot *m_chart = nullptr;
    QVector<QCheckBox *> m_graphVisibilityChecks;
    QCheckBox *m_elapsedTimeCheckBox = nullptr;
    QComboBox *m_qrCodeCombo = nullptr;
    QPushButton *m_recordButton = nullptr;
    QPushButton *m_chartPauseButton = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_markerButton = nullptr;
    QPushButton *m_openRecordButton = nullptr;
    QDoubleSpinBox *m_markerTimeSpinBox = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QPointer<PurgeTask> m_runningTask;
    QPointer<PurgeDataRecorder> m_dataRecorder;
    QString m_currentOutputDir;
    QElapsedTimer m_recordingElapsedTimer;
    double m_recordingStartEpochSeconds = 0.0;
    double m_recordedDurationSeconds = 0.0;
    bool m_isRecording = false;
    bool m_chartPaused = false;
    bool m_taskRunning = false;
    bool m_hasRecordedSamples = false;
};

#endif // CHARTPAGE_H
