#ifndef FIRMWAREUPDATEWIDGET_H
#define FIRMWAREUPDATEWIDGET_H

#include <QDateTime>
#include <QByteArray>
#include <QHash>
#include <QSet>
#include <QTableWidget>
#include <QWidget>
#include <QStringList>

#include "modbustcpmastermanager/modbustcpmaster/firmwareupgrader.h"

class ModbusTcpMaster;

namespace Ui {
class FirmwareUpdateWidget;
}

class FirmwareUpdateWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FirmwareUpdateWidget(QWidget *parent = nullptr);
    ~FirmwareUpdateWidget();

    enum DeviceStatus {
        Idle = 0,
        Waiting,
        Updating,
        Success,
        Failed
    };

public:
    void setFirmwareFilePath(const QString &filePath);
    QString firmwareFilePath() const;

    void setCaptureDirectory(const QString &dirPath);
    QString captureDirectory() const;
    void setInteractiveEnabled(bool enabled);

    void setSelectedDevices(const QStringList &deviceIds);
    void prepareMonitoredRound(const QStringList &deviceIds);
    void applyMonitoredDeviceProgress(const QString &qrcode, int percent);
    void applyMonitoredDeviceStateLog(const QString &qrcode,
                                      FirmwareUpgrader::UpgradeState state,
                                      const QString &logMessage,
                                      const QByteArray &frame);
    void applyMonitoredDeviceFinished(const QString &qrcode, bool success, const QString &message);
    void updateMonitoredRoundProgress(int completed, int total);
    QString captureCurrentScreenshot(const QString &fileNamePrefix = QString());
    bool saveCurrentScreenshot(const QString &filePath);

public slots:
    void onAddDeviceForUpdate();
    void onAddAllDevices();
    void onClear();
    void onUpdateSelectedDevices();

private slots:
    void onTaskDeviceProgress(const QString &qrcode, int percent);
    void onTaskDeviceStateLog(const QString &qrcode,
                              FirmwareUpgrader::UpgradeState state,
                              const QString &logMessage,
                              const QByteArray &frame);
    void onTaskDeviceFinished(const QString &qrcode, bool success, const QString &message);
    void onTaskAllProgress(int completed, int total);

private:
    Ui::FirmwareUpdateWidget *ui;

    QString m_firmwareFilePath;

    int m_totalDevices;
    int m_completedDevices;
    int m_successCount;
    int m_failCount;
    QSet<QString> m_completedDeviceIds;

    QHash<QString, QDateTime> m_deviceUpgradeStartTimes;
    QString m_captureDirectory;
    QHash<QString, int> m_qrcodeRowMap;
    bool m_interactiveEnabled = true;
    bool m_bulkUpdatingDeviceTable = false;

    void initUI();
    void initLoggerWidget();
    void initTableWidget(QTableWidget *table, const QStringList &headers);
    void initTableWidgetSelectedDevices();
    void addDeviceToTable(ModbusTcpMaster *master);
    void addDeviceToTable(const QString &qrCode);
    QStringList selectedDeviceIdsInTable() const;
    void updateTableHeight();
    void updateProgressBar();
    void resetProgress();
    void resetDeviceRows();
    void prepareDevicesForUpgrade(const QStringList &deviceIds);
    void handleDeviceProgress(const QString &qrcode, int percent);
    void handleDeviceStateLog(const QString &qrcode,
                              FirmwareUpgrader::UpgradeState state,
                              const QString &logMessage,
                              const QByteArray &frame);
    void handleDeviceFinished(const QString &qrcode, bool success, const QString &message);
    void handleAllProgress(int completed, int total, bool showCompletionDialog);

    QString getStatusText(DeviceStatus status) const;
    QString getStatusStyle(DeviceStatus status) const;

    void writeLog(const QString &level, const QString &qrcode,
                  const QString &phase, const QString &message);

    QString captureTableWidgetScreenshot(const QString &fileNamePrefix = QString());
    bool saveTableWidgetScreenshot(const QString &filePath);
    bool ensureCaptureDirectoryExists();

    static QString upgradeStateToPhase(FirmwareUpgrader::UpgradeState state);
};

#endif // FIRMWAREUPDATEWIDGET_H
