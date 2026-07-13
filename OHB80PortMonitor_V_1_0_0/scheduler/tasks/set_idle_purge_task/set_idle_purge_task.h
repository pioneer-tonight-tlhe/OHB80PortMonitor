/*******************************************************************************************
 * @file set_idle_purge_task.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class SetIdlePurgeTask
 * @brief 负责向设备下发 Idle Purge 参数，并在任务结束时统一持久化本地配置。
 *
 * 设计目标：
 *      1. 统一封装 Idle Purge 配置项的指令下发、结果汇总和重试状态反馈。
 *      2. 无论设备写入是否成功，都由调度任务层统一更新本地配置文件，保证配置态独立持久化。
 *      3. 为运行日志、通讯日志和界面结果反馈提供一致的任务级输出。
 *******************************************************************************************/
#ifndef SET_IDLE_PURGE_TASK_H
#define SET_IDLE_PURGE_TASK_H

#include "../../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QAtomicInt>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QString>
#include <QStringList>

class OperationDispatchTask;

class SetIdlePurgeTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 公共数据类型 ============================
    enum class IdlePurgeProperty {
        Enable,
        PurgeTime,
        PurgeInterval
    };

    // ============================ 构造函数 ============================
    explicit SetIdlePurgeTask(IdlePurgeProperty property,
                              quint16 value,
                              const QString &targetQrCode,
                              QObject *parent = nullptr);
    ~SetIdlePurgeTask() override;

    // ============================ 基类相关接口 ============================
    void start() override;
    void stop() override;
    QString taskType() const override { return "SetIdlePurgeTask"; }

    // ============================ 业务功能 ============================
    static QString propertyToString(IdlePurgeProperty property);

private:
    // ============================ 指令构建 ============================
    QString getCommandIdForProperty(IdlePurgeProperty property) const;
    QByteArray buildRegisterValue(quint16 value) const;
    QString getValueText(quint16 value) const;

    // ============================ 日志记录 ============================
    void writeDeviceSkipLog(const QString &qrCode, const QString &commandId, const QString &reason);
    void writeDeviceCommandLog(const QString &qrCode, const ModbusCommand &cmd, bool success);
    QString buildCommandFrameLogString(const ModbusCommand &cmd) const;
    QString getDeviceLogPath() const;
    static QString buildSafeLogPathSegment(const QString &value);
    ILogger &deviceDetailLogger();
    QString getSubFunctionName() const;

    // ============================ 任务流程 ============================
    void disconnectAll();
    void checkAllFinished();
    void forceFinish();
    bool persistConfig(QString *errorMessage = nullptr);

    // ============================ 结果处理 ============================
    void logFailedDevice(OperationDispatchTask *opTask, const QString &qrCode);

signals:
    // ---- 结果通知 ----
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     QString propertyName,
                     quint16 setValue);
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    // ---- 指令回调 ----
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);

private:
    // ---- 任务状态成员 ----
    IdlePurgeProperty m_property;
    quint16 m_value;
    QString m_targetQrCode;
    QHash<qint64, QString> m_pendingMap;
    QList<QMetaObject::Connection> m_connections;
    int m_totalCount = 0;
    QAtomicInt m_completedCount{0};
    bool m_stopped = false;
    int m_successCount = 0;
    QStringList m_failedQrCodes;
    bool m_allFinishedEmitted = false;

    // ---- 日志成员 ----
    ILogger m_deviceLogger;
    bool m_loggerInitialized = false;
};

#endif // SET_IDLE_PURGE_TASK_H
