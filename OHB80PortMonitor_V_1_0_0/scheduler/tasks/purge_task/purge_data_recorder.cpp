#include "purge_data_recorder.h"

#include "appconfig.h"
#include "csvfilewriter.h"
#include "foupofohbinfo.h"
#include "shareddata.h"

#include <QDateTime>
#include <QDir>
#include <QTimer>

PurgeDataRecorder::PurgeDataRecorder(const QString &qrCode, QObject *parent)
    : QObject(parent)
    , m_qrCode(qrCode.trimmed())
{
    m_sampleTimer = new QTimer(this);
    m_sampleTimer->setInterval(1000);
    m_sampleTimer->setTimerType(Qt::PreciseTimer);
    connect(m_sampleTimer, &QTimer::timeout, this, &PurgeDataRecorder::recordSample);
}

bool PurgeDataRecorder::start(QString *errorMessage)
{
    if (m_recording) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }

    if (m_qrCode.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeDataRecorder: QRCode is empty");
        }
        return false;
    }

    if (!SharedData::getFoupByQRCode(m_qrCode)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeDataRecorder: shared OHB data not found, QRCode=%1")
                                .arg(m_qrCode);
        }
        return false;
    }

    if (!prepareOutputDirectory(errorMessage)) {
        return false;
    }

    clearCurrentStage();
    m_recording = true;
    m_sampleTimer->start();
    emit recordingStarted(m_outputDir);

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void PurgeDataRecorder::stop()
{
    if (!m_recording) {
        return;
    }

    m_sampleTimer->stop();
    m_recording = false;
    clearCurrentStage();
    emit recordingStopped(m_outputDir);
}

bool PurgeDataRecorder::isRecording() const
{
    return m_recording;
}

QString PurgeDataRecorder::qrCode() const
{
    return m_qrCode;
}

QString PurgeDataRecorder::outputDir() const
{
    return m_outputDir;
}

void PurgeDataRecorder::setCurrentStage(int stageNo, const QString &stageName)
{
    if (!m_recording || stageNo <= 0) {
        return;
    }

    if (m_currentStageNo > 0) {
        clearCurrentStage();
        if (!m_recording) {
            return;
        }
    }

    const QString stageCsvPath = QDir(m_outputDir)
                                     .filePath(QStringLiteral("stage_%1.csv").arg(stageNo));
    QString errorMessage;
    if (!CsvFileWriter::ensureFileWithHeader(stageCsvPath, csvHeaders(), &errorMessage)) {
        fail(QStringLiteral("PurgeDataRecorder: failed to prepare stage CSV: %1")
                 .arg(errorMessage));
        return;
    }

    m_currentStageNo = stageNo;
    m_currentStageName = stageName.trimmed().isEmpty()
        ? QStringLiteral("Stage %1").arg(stageNo)
        : stageName.trimmed();
    m_currentStageCsvPath = stageCsvPath;

    if (!appendRecord(QStringLiteral("%1 Start").arg(m_currentStageName),
                      true,
                      &errorMessage)) {
        fail(QStringLiteral("PurgeDataRecorder: failed to write stage start record: %1")
                 .arg(errorMessage));
    }
}

void PurgeDataRecorder::clearCurrentStage()
{
    if (!m_recording || m_currentStageNo <= 0) {
        resetCurrentStage();
        return;
    }

    QString errorMessage;
    const QString stageEndName = QStringLiteral("%1 End").arg(m_currentStageName);
    if (!appendRecord(stageEndName, true, &errorMessage)) {
        resetCurrentStage();
        fail(QStringLiteral("PurgeDataRecorder: failed to write stage end record: %1")
                 .arg(errorMessage));
        return;
    }

    resetCurrentStage();
}

void PurgeDataRecorder::recordSample()
{
    if (!m_recording) {
        return;
    }

    QString errorMessage;
    if (!appendRecord(m_currentStageName, true, &errorMessage)) {
        fail(QStringLiteral("PurgeDataRecorder: failed to write CSV record: %1")
                 .arg(errorMessage));
    }
}

bool PurgeDataRecorder::prepareOutputDirectory(QString *errorMessage)
{
    const QDateTime now = QDateTime::currentDateTime();
    const QString baseDir = QDir(AppConfig::getInstance().getRootDir())
                                .filePath(QStringLiteral("OHB_%1_Monitor_Graph").arg(m_qrCode));
    const QString dateDir = QDir(baseDir).filePath(now.toString(QStringLiteral("yyyyMMdd")));
    const QString qrDir = QDir(dateDir).filePath(m_qrCode);
    m_outputDir = QDir(qrDir).filePath(now.toString(QStringLiteral("HHmmss")));

    QDir dir;
    if (!dir.mkpath(m_outputDir)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PurgeDataRecorder: failed to create output directory: %1")
                                .arg(m_outputDir);
        }
        return false;
    }

    m_allStageCsvPath = QDir(m_outputDir).filePath(QStringLiteral("stage_all.csv"));
    return CsvFileWriter::ensureFileWithHeader(m_allStageCsvPath, csvHeaders(), errorMessage);
}

bool PurgeDataRecorder::appendRecord(const QString &stageName,
                                     bool includeCurrentStageFile,
                                     QString *errorMessage)
{
    const QStringList headers = csvHeaders();
    const QStringList row = currentCsvRow(stageName);
    if (!CsvFileWriter::appendRow(m_allStageCsvPath, headers, row, errorMessage)) {
        return false;
    }

    if (includeCurrentStageFile && !m_currentStageCsvPath.isEmpty()
        && !CsvFileWriter::appendRow(m_currentStageCsvPath, headers, row, errorMessage)) {
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QStringList PurgeDataRecorder::csvHeaders() const
{
    return {
        QStringLiteral("timestamp"),
        QStringLiteral("qr_code"),
        QStringLiteral("stage_name"),
        QStringLiteral("inlet_pressure（Mpa）"),
        QStringLiteral("negative_pressure（Kpa）"),
        QStringLiteral("inlet_flow（L/Min）"),
        QStringLiteral("humidity（%）"),
        QStringLiteral("temperature（℃）"),
        QStringLiteral("foup_in")
    };
}

QStringList PurgeDataRecorder::currentCsvRow(const QString &stageName) const
{
    const FoupOfOHBInfo *foup = SharedData::getFoupByQRCode(m_qrCode);
    return {
        QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
        m_qrCode,
        stageName,
        foup ? formatNumber(foup->inletPressure()) : QString(),
        foup ? formatNumber(foup->negativePressure()) : QString(),
        foup ? formatNumber(foup->inletFlow()) : QString(),
        foup ? formatNumber(foup->RH()) : QString(),
        foup ? formatNumber(foup->temperature()) : QString(),
        foup ? QString::number(foup->foupIn() ? 1 : 0) : QString()
    };
}

QString PurgeDataRecorder::formatNumber(double value, int precision) const
{
    return QString::number(value, 'f', precision);
}

void PurgeDataRecorder::resetCurrentStage()
{
    m_currentStageNo = -1;
    m_currentStageName.clear();
    m_currentStageCsvPath.clear();
}

void PurgeDataRecorder::fail(const QString &message)
{
    m_sampleTimer->stop();
    m_recording = false;
    resetCurrentStage();
    emit recordingFailed(message, m_outputDir);
}
