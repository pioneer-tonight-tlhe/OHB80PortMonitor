#ifndef FIRMWARE_UPGRADE_TASK_H
#define FIRMWARE_UPGRADE_TASK_H

#include "../../scheduler_task.h"
#include "modbustcpmastermanager/modbustcpmaster/firmwareupgrader.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QStringList>
#include <QHash>
#include <string>

class BinFileReader;
class ModbusTcpMaster;

class FirmwareUpgradeTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit FirmwareUpgradeTask(const QStringList &deviceIds,
                                 const QString &binFilePath,
                                 QObject *parent = nullptr);
    ~FirmwareUpgradeTask() override;

    // SchedulerTask 接口
    void start() override;
    void stop() override;
    QString taskType() const override { return "FirmwareUpgradeTask"; }

signals:
    // 单个设备升级进度（0-100），用于更新表格进度条
    void deviceProgress(const QString &deviceId, int percent);

    // 单个设备状态日志，用于显示到日志组件
    void deviceStateLog(const QString &deviceId,
                        FirmwareUpgrader::UpgradeState state,
                        const QString &logMessage,
                        const QByteArray &frame);

    // 单个设备升级完毕，用于最终结论显示
    void deviceFinished(const QString &deviceId, bool success, const QString &message);

    // 总体完成进度（已完成设备数 / 总设备数），用于刷新 progressBar
    void allProgress(int completed, int total);

private slots:
    void onBinFileReadFinished(bool success, const QString &errorMsg);
    void onUpgraderStateChanged(const QString &masterId,
                                FirmwareUpgrader::UpgradeState state,
                                const QString &logMessage,
                                const QByteArray &frame);
    void onUpgraderProgress(const QString &masterId, int percent);
    void onUpgraderFinished(const QString &masterId,
                            bool success,
                            FirmwareUpgrader::UpgradeState state,
                            const QString &errorMessage);
    void onWriteQRCodeFinished(ModbusCommand cmd, const QString &masterId);

private:
    void submitWriteQRCode(const QString &masterId);
    void startUpgrading();

    QStringList   m_deviceIds;
    QString       m_binFilePath;
    BinFileReader *m_binFileReader = nullptr;

    // deviceId → upgrader 映射（用于信号槽中反查设备）
    QHash<QString, FirmwareUpgrader*> m_upgraderMap;

    // 待处理的 WriteQRCode 指令：uuid -> masterId
    QHash<qint64, QString> m_writeQRCodePendingMap;
    QList<QMetaObject::Connection> m_qrCodeConnections;

    int  m_totalCount    = 0;
    int  m_finishedCount = 0;
    bool m_stopped       = false;

    const std::string m_taskLogPath = "scheduler/firmware_upgrade_task";
};

#endif // FIRMWARE_UPGRADE_TASK_H
