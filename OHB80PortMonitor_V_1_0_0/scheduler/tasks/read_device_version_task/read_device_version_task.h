/*******************************************************************************************
 * @file read_device_version_task.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class ReadDeviceVersionTask
 * @brief 读取设备固件版本号和 UI 屏幕版本号的调度任务。
 *
 * 设计目标：
 *      1. 为 Device Info 界面提供统一的版本信息查询入口。
 *      2. 复用指令池和响应解析器，避免界面层直接处理 Modbus 协议细节。
 *      3. 在查询成功后同步刷新 ModbusTcpMaster 的版本号运行态缓存。
 *******************************************************************************************/
#ifndef READ_DEVICE_VERSION_TASK_H
#define READ_DEVICE_VERSION_TASK_H

#include "../../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QMetaType>
#include <QStringList>
#include <QTimer>
#include <QVector>

class ReadDeviceVersionTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 公共数据类型 ============================
    struct DeviceVersionInfo
    {
        QString qrCode;               // 设备 QRCode。
        QString firmwareVersion;      // 固件版本号显示值。
        QString uiScreenVersion;      // UI 屏幕版本号显示值。
        bool firmwareVersionOk;       // 固件版本号是否读取成功。
        bool uiScreenVersionOk;       // UI 屏幕版本号是否读取成功。

        DeviceVersionInfo()
            : firmwareVersionOk(false)
            , uiScreenVersionOk(false)
        {
        }

        bool allOk() const
        {
            return firmwareVersionOk && uiScreenVersionOk;
        }
    };

    // ============================ 构造函数 ============================
    explicit ReadDeviceVersionTask(const QVector<QString>& qrCodes,
                                   QObject* parent = nullptr);
    ~ReadDeviceVersionTask() override;

    // ============================ 基类相关接口 ============================
    void start() override;
    void stop() override;
    QString taskType() const override { return QStringLiteral("ReadDeviceVersionTask"); }

signals:
    // ---- 业务功能 ----
    void allFinished(bool allSuccess,
                     int successCount,
                     QList<ReadDeviceVersionTask::DeviceVersionInfo> results);

    void deviceRetrying(QString qrCode,
                        QString commandId,
                        int retryCount,
                        int maxRetry);

private slots:
    // ---- 业务功能 ----
    void onCommandFinished(ModbusCommand cmd, const QString& masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString& masterId);
    void onTimeout();

private:
    // ============================ 业务功能 ============================
    enum class VersionField
    {
        FirmwareVersion,
        UiScreenVersion
    };

    struct PendingCommand
    {
        QString qrCode;       // 所属设备 QRCode。
        VersionField field;   // 当前指令对应的版本字段。
    };

    void disconnectAll();
    void initializeDeviceResult(const QString& qrCode);
    void markDeviceSkipped(const QString& qrCode, const QString& reason);
    void markCommandFinished(const QString& qrCode, bool success);
    void finalizeDeviceIfNeeded(const QString& qrCode);
    void checkAllFinished();
    void forceFinish();
    void updateDeviceVersionResult(const QString& qrCode,
                                   VersionField field,
                                   const QString& version);
    void writeCommunicateLog(const QString& qrCode,
                             const ModbusCommand& cmd,
                             const QString& description) const;
    void writeDeviceSkipLog(const QString& qrCode,
                            const QString& commandId,
                            const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode,
                               const ModbusCommand& cmd,
                               bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    ILogger& deviceDetailLogger();

private:
    // ---- 状态成员 ----
    QVector<QString> m_qrCodes;                     // 本次任务的目标设备列表。
    QHash<qint64, PendingCommand> m_pendingMap;     // 指令 uuid 到待处理设备/字段的映射。
    QHash<QString, DeviceVersionInfo> m_resultMap;  // QRCode 到最终显示结果的映射。
    QHash<QString, int> m_deviceFinishedCount;      // 每台设备已完成的子指令数量。
    QHash<QString, int> m_deviceSuccessCount;       // 每台设备成功的子指令数量。
    QHash<QString, bool> m_deviceCompletedMap;      // 每台设备是否已完成汇总。
    QList<QMetaObject::Connection> m_connections;   // 本任务建立的指令信号连接。
    int m_totalDeviceCount = 0;                     // 本次任务的目标设备总数。
    int m_completedDeviceCount = 0;                // 已完成汇总的设备数量。
    int m_successCount = 0;                        // 两条版本指令都成功的设备数量。
    QStringList m_failedQrCodes;                   // 读取失败的设备列表。
    bool m_stopped = false;                        // 任务是否已被取消。
    bool m_allFinishedEmitted = false;             // 是否已发出完成信号。

    // ---- 功能模块成员 ----
    QTimer* m_timeoutTimer = nullptr;              // 整体超时定时器。
    ILogger m_deviceLogger;                        // 设备级详细日志记录器。
    bool m_loggerInitialized = false;              // 详细日志路径是否已初始化。
};

Q_DECLARE_METATYPE(ReadDeviceVersionTask::DeviceVersionInfo)
Q_DECLARE_METATYPE(QList<ReadDeviceVersionTask::DeviceVersionInfo>)

#endif // READ_DEVICE_VERSION_TASK_H
