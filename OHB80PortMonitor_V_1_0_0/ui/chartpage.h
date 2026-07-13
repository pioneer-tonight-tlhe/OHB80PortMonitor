#ifndef CHARTPAGE_H
#define CHARTPAGE_H

#include "scheduler/tasks/purge_task/purge_task.h"

#include <QPointer>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QTimer;
class QCustomPlot;

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
    void updateStatusText(const QString &text);
    QString selectedQRCode() const;
    bool isValidRecordDirectory(const QString &dirPath) const;
    void saveChartSnapshot(const QString &dirPath);

private:
    static const QString kPlotId;

    Ui::ChartPage *ui;
    QCustomPlot *m_chart = nullptr;
    QVector<QCheckBox *> m_graphVisibilityChecks;
    QCheckBox *m_appendDataCheckBox = nullptr;
    QComboBox *m_qrCodeCombo = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_openRecordButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QPointer<PurgeTask> m_runningTask;
    QString m_currentOutputDir;
};

#endif // CHARTPAGE_H
