#ifndef MONITOR_DATA_TASK_H
#define MONITOR_DATA_TASK_H

#include "../../scheduler_task.h"
#include "monitor_data_task_logger.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QMap>
#include <QString>
#include <QVariantMap>

class CommunicationRecorder;
class FoupOfOHBInfo;
class AlarmDispatchTask;
enum class AlarmType : int;
enum class AlarmSource : int;

/**
 * @brief 设备实时监控数据采集任务。
 *
 * 主要流程：
 * 1. start() 初始化任务状态、统计计数和设备日志记录器，然后遍历所有 ModbusTcpMaster。
 * 2. 为每个设备的 PeriodicCommandSender 连接 commandCompleted 信号，持续接收周期监控指令结果。
 * 3. onCommandCompleted() 先把指令提交给 CommunicationRecorder 做 UI/数据库节流，再按指令结果写设备日志。
 * 4. 指令成功后通过 CommandResponseParser 解析响应数据，并由 updateFoupInfo() 按 commandId 分发处理。
 * 5. ReadFoupStatus 更新 FOUP 实时数据、维护 FOUP 在位状态，并把设备状态位转换为告警发生/恢复事件。
 * 6. ReadIdlePurgeAll 更新空闲吹扫聚合状态；单项 IdlePurge 读取指令不再处理。
 * 7. MonitorDataTask 只向 AlarmDispatchTask 上报告警事件，不直接修改 FOUP 的 UI 告警缓存。
 * 8. stop() 断开周期发送器信号、停止采集器，并在 summary 日志中写入本轮监控统计。
 *
 * 日志约定：
 * - summary 日志：scheduler/monitor_data_task/summary，记录任务生命周期和汇总统计。
 * - 设备日志：scheduler/monitor_data_task/<QRCode>，记录单设备失败帧、解析失败、FOUP 状态变化和告警变化。
 * - 成功、失败和解析失败通讯帧都会写入对应设备日志，便于按 QRCode 追踪监控数据。
 */
class MonitorDataTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit MonitorDataTask(QObject *parent = nullptr);
    ~MonitorDataTask();

    Q_INVOKABLE void start() override;
    Q_INVOKABLE void stop() override;
    QString taskType() const override { return "MonitorDataTask"; }
    bool isPersistent() const override { return true; }

signals:
    // 通讯完成信号：指令执行完成后发出，携带指令信息、设备 ID 和描述
    void communicationCompleted(ModbusCommand cmd, QString masterId, QString description);

private slots:

    // 采集器指令筛选完毕信号
    void onCommunicationRecorded(ModbusCommand cmd, const QString& masterId);

    // 指令发送器发送完毕信号
    void onCommandCompleted(ModbusCommand cmd, const QString& masterId);

private:
    // 更新 FOUP 信息（根据指令 ID 分发到不同的处理方法）
    void updateFoupInfo(const QString& masterId, const QString& commandId, const QVariantMap& data);

    // 处理 ReadFoupStatus 指令：更新 FOUP 状态数据并处理告警
    void handleReadFoupStatus(FoupOfOHBInfo* foup, const QString& masterId, const QVariantMap& data);

    // 处理 ReadIdlePurge 相关指令：更新空闲吹扫状态
    // 只处理 ReadIdlePurgeAll 聚合读取指令，单项 IdlePurge 读取指令已废弃。
    void handleReadIdlePurgeAll(FoupOfOHBInfo* foup, const QVariantMap& data);

    // 根据 FOUP out/in 变化维护开始时间和 purge 计时，并在状态变化时写入设备日志。
    void updateFoupPresenceState(FoupOfOHBInfo* foup, const QString& masterId);

    // 对 FOUP 在位和 IdlePurge 禁用场景做通用状态修正。
    void normalizeFoupRuntimeState(FoupOfOHBInfo* foup);

    // 将 ReadFoupStatus 中的设备状态位转换为告警发生/恢复事件。
    void reportDeviceStatusAlarms(const QString& masterId, const QVariantMap& data);

    // 只负责向 AlarmDispatchTask 上报告警发生或恢复，不直接修改 UI 告警缓存。
    void reportDeviceAlarmState(AlarmDispatchTask* alarmTask,
                                bool active,
                                AlarmType alarmType,
                                AlarmSource alarmSource,
                                const QString& masterId,
                                const QString& description);

    // 设备通讯日志辅助方法：成功、失败、解析失败都会写入对应设备日志。
    void logCommandFailure(const QString& masterId, const ModbusCommand& cmd);
    void logCommandParseFailed(const QString& masterId, const ModbusCommand& cmd);
    void logCommandSuccess(const QString& masterId, const ModbusCommand& cmd, const QVariantMap& data);
    QString commandFailureReason(const ModbusCommand& cmd) const;
    QString commandRequestFrame(const ModbusCommand& cmd) const;
    QString commandResponseFrame(const ModbusCommand& cmd) const;
    QString parsedDataText(const QVariantMap& data) const;
    QString alarmTypeName(AlarmType alarmType) const;

    int m_totalCount = 0;
    bool m_stopped = false;
    qint64 m_startedMs = 0;
    qint64 m_commandTotalCount = 0;
    qint64 m_commandSuccessCount = 0;
    qint64 m_commandFailedCount = 0;
    qint64 m_commandParseFailedCount = 0;

    // 成功帧写入设备日志；如后续需要降低日志量，可将该开关改为 false。
    bool m_logSuccessCommandFrame = true;

    // 记录每台设备每类告警的上一次状态，只在状态变化时写告警日志。
    QMap<QString, bool> m_lastAlarmStates;

    // ============================================================
    // 通讯指令采集器（节流器）
    // ============================================================
    CommunicationRecorder* m_recorder = nullptr;


    // ============================================================
    // 日志接口
    // ============================================================
    // 初始化
    void initMonitorDataTaskLogger();

    // 集中管理 summary 日志和子设备日志映射，任务流程代码只负责写入内容。
    MonitorDataTaskLogger* m_logger;
};

#endif // MONITOR_DATA_TASK_H
