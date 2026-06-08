#ifndef VEFC_SENSOR_MONITOR_TASK_H
#define VEFC_SENSOR_MONITOR_TASK_H

#include "../../scheduler_task.h"
#include "classes/vefcsensormonitorrecord.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "vefc_sensor_monitor_task_logger.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QString>
#include <QTimer>
#include <QVector>

/**
 * @brief VEFC 传感器监控常驻任务。
 *
 * 主要流程：
 * 1. start() 注册所有设备的 ModbusCommandSender 信号，并启动 1 分钟周期定时器。
 * 2. 每轮开始时记录统一 roundId 和 recordTimestamp。
 * 3. 对每台设备从 SharedData::FoupOfOHBInfo 读取当前进气压力和实际流量。
 * 4. 对每台在线设备下发 ReadVEFCPressure 和 ReadVEFCTemperature 两条业务指令。
 * 5. 两条指令都成功解析后生成 VEFCSensorMonitorRecord。
 *    任一指令失败、超时或解析失败时，本轮该设备不生成记录，避免用默认值/旧值污染统计数据。
 * 6. 本轮结束时仅将成功生成的记录通过 VEFCSensorMonitorDBCon::insertRecords() 批量写入数据库。
 *
 * 日志约定：
 * - summary 日志：scheduler/vefc_sensor_monitor_task/summary
 * - 子设备日志：scheduler/vefc_sensor_monitor_task/devices，所有设备明细统一写入一个文件
 */
class VEFCSensorMonitorTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit VEFCSensorMonitorTask(QObject* parent = nullptr);
    ~VEFCSensorMonitorTask();

    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return QStringLiteral("VEFCSensorMonitorTask"); }
    bool isPersistent() const override { return true; }
    bool isRecurring() const override { return true; }
    int intervalMs() const override { return kIntervalMs; }

private slots:
    void onIntervalTick();
    void onCommandFinished(ModbusCommand cmd, const QString& masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString& masterId);

private:
    enum class SensorCommandType {
        Pressure,
        Temperature
    };

    struct PendingCommand {
        QString qrCode;
        SensorCommandType type = SensorCommandType::Pressure;
    };

    struct DeviceRoundState {
        QString qrCode;
        // 只有传感器压力和传感器温度两条指令都成功时，此记录才允许写入数据库。
        VEFCSensorMonitorRecord record;
        bool pressureFinished = false;
        bool temperatureFinished = false;
        bool pressureOk = false;
        bool temperatureOk = false;
        bool skipped = false;
        QString failReason;
    };

    static constexpr int kIntervalMs = 60 * 1000;
    static constexpr const char* kReadPressureCmdId = "ReadVEFCPressure";
    static constexpr const char* kReadTemperatureCmdId = "ReadVEFCTemperature";

    void initLogger();
    void connectAllSenders();
    void disconnectAllSenders();

    void startRound();
    void finishRoundIfReady();
    void submitDeviceCommands(const QString& qrCode);
    bool submitCommand(const QString& qrCode, SensorCommandType type, const char* commandId);
    void skipDevice(const QString& qrCode, const QString& reason);
    void completeCommand(const QString& qrCode, SensorCommandType type, const ModbusCommand& cmd);
    void failCommand(const QString& qrCode, SensorCommandType type, const ModbusCommand& cmd, const QString& reason);

    QString commandTypeName(SensorCommandType type) const;
    QString commandFailureReason(const ModbusCommand& cmd) const;
    QString commandRequestFrame(const ModbusCommand& cmd) const;
    QString commandResponseFrame(const ModbusCommand& cmd) const;
    QString roundIdFromTimestamp(qint64 timestamp) const;

    QTimer* m_timer = nullptr;
    VEFCSensorMonitorTaskLogger* m_logger = nullptr;
    QList<QMetaObject::Connection> m_connections;

    bool m_stopped = false;
    bool m_roundActive = false;
    QString m_roundId;
    qint64 m_roundTimestamp = 0;
    qint64 m_startedMs = 0;
    int m_roundIndex = 0;

    QHash<qint64, PendingCommand> m_pendingCommands;
    QHash<QString, DeviceRoundState> m_deviceStates;
};

#endif // VEFC_SENSOR_MONITOR_TASK_H
