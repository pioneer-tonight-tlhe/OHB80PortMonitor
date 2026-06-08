#ifndef FIRMWAREUPDATEWIDGET_H
#define FIRMWAREUPDATEWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QSet>
#include <QHash>
#include <QDateTime>

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
        Idle = 0,       // 空闲
        Waiting,
        Updating,       // 升级中
        Success,        // 成功
        Failed          // 失败
    };

public:
    // 设置固件升级文件路径
    void setFirmwareFilePath(const QString &filePath);
    QString firmwareFilePath() const;

    // 设置截图存放目录
    void setCaptureDirectory(const QString &dirPath);
    QString captureDirectory() const;

public slots:
    void onAddDeviceForUpdate();        // 选中要升级的设备
    void onAddAllDevices();             // 添加所有设备到表格
    void onClear();                     // 清理表格中所有数据
    void onUpdateSelectedDevices();     // 开始升级选中的设备

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

    // 固件升级相关
    QString m_firmwareFilePath;  // 固件文件路径

    // 进度跟踪相关
    int m_totalDevices;          // 总设备数
    int m_completedDevices;      // 已完成设备数
    int m_successCount;          // 成功设备数
    int m_failCount;             // 失败设备数
    QSet<QString> m_completedDeviceIds;  // 已完成的设备ID集合

    // 升级时间记录
    QHash<QString, QDateTime> m_deviceUpgradeStartTimes;

    // 截图相关
    QString m_captureDirectory;  // 截图存放目录

    // ---- 性能优化 ----
    QHash<QString, int> m_qrcodeRowMap;   // qrcode → 行索引 O(1) 查找

    void initUI();
    void initLoggerWidget();
    void initTableWidget(QTableWidget *table, const QStringList &headers);
    void initTableWidgetSelectedDevices();
    void addDeviceToTable(ModbusTcpMaster *master);
    void updateTableHeight();
    void updateProgressBar();
    void resetProgress();

    // 状态相关辅助函数
    QString getStatusText(DeviceStatus status) const;
    QString getStatusStyle(DeviceStatus status) const;

    // 日志写入辅助
    void writeLog(const QString &level, const QString &qrcode,
                  const QString &phase, const QString &message);

    // 截图相关
    void captureTableWidgetScreenshot();
    bool ensureCaptureDirectoryExists();

    // 升级阶段转换
    static QString upgradeStateToPhase(FirmwareUpgrader::UpgradeState state);
};

#endif // FIRMWAREUPDATEWIDGET_H
