#ifndef PURGE_DATA_RECORDER_H
#define PURGE_DATA_RECORDER_H

#include <QObject>
#include <QString>
#include <QStringList>

class QTimer;

class PurgeDataRecorder : public QObject
{
    Q_OBJECT

public:
    explicit PurgeDataRecorder(const QString &qrCode, QObject *parent = nullptr);

    bool start(QString *errorMessage = nullptr);
    void stop();

    bool isRecording() const;
    QString qrCode() const;
    QString outputDir() const;

public slots:
    void setCurrentStage(int stageNo, const QString &stageName);
    void clearCurrentStage();

signals:
    void recordingStarted(const QString &outputDir);
    void recordingStopped(const QString &outputDir);
    void recordingFailed(const QString &message, const QString &outputDir);

private slots:
    void recordSample();

private:
    bool prepareOutputDirectory(QString *errorMessage);
    bool appendRecord(const QString &stageName,
                      bool includeCurrentStageFile,
                      QString *errorMessage);
    QStringList csvHeaders() const;
    QStringList currentCsvRow(const QString &stageName) const;
    QString formatNumber(double value, int precision = 3) const;
    void resetCurrentStage();
    void fail(const QString &message);

private:
    QString m_qrCode;
    QString m_outputDir;
    QString m_allStageCsvPath;
    QString m_currentStageCsvPath;
    QString m_currentStageName;
    int m_currentStageNo = -1;
    QTimer *m_sampleTimer = nullptr;
    bool m_recording = false;
};

#endif // PURGE_DATA_RECORDER_H
