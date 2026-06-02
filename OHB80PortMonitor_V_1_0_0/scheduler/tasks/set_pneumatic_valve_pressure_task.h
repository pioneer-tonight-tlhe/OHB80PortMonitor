#ifndef SET_PNEUMATIC_VALVE_PRESSURE_TASK_H
#define SET_PNEUMATIC_VALVE_PRESSURE_TASK_H

#include "../scheduler_task.h"
#include "ilogger.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QAtomicInt>
#include <QHash>
#include <QList>
#include <QStringList>
#include <QVector>


class OperationDispatchTask;

// ====================================================================
// SetPneumaticValvePressureTask - 设置气控阀压力调度任务
//
//   接口参数：
//     qrcodes      - 目标设备 QRCode 列表（master ID 列表）
//     pressureBar  - 压力值，单位 bar（设备侧寄存器值 = pressureBar * kRegisterScale）
//
//   内部实现：
//     直接操作 ModbusTcpMasterManager + ModbusCommandSender。
//     监听底层 commandFinished 和 commandTimeoutRetry，不再使用任务层整体超时。
//
//   信号 allFinished：
//     allSuccess     - 是否全部成功
//     successCount   - 成功设备数
//     failedQrCodes  - 失败设备 QRCode 列表
//     pressureBar    - 设置的压力值（bar）
// ====================================================================
class SetPneumaticValvePressureTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SetPneumaticValvePressureTask(const QVector<QString> &qrcodes,
                                           double pressureBar,
                                           QObject *parent = nullptr);
    ~SetPneumaticValvePressureTask();

    // SchedulerTask 接口
    void start() override;
    void stop()  override;
    QString taskType() const override { return "SetPneumaticValvePressureTask"; }

    // 寄存器值倍率：pressureBar * kRegisterScale 写入设备
    static constexpr int kRegisterScale = 1000;

signals:
    // 全部设备执行完毕信号
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     double pressureBar);

    // 设备重试信号（通知 UI 显示重试状态）
    void deviceRetrying(const QString &qrCode, int retryCount, int maxRetry);

private slots:
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);

private:
    QByteArray buildRegisterValue(quint16 value) const;
    void disconnectAll();
    void checkAllFinished();
    void forceFinish();
    void logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode);

    void writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    static QString safeLogPathSegment(const QString& value);
    ILogger& deviceDetailLogger();
    QString subFunctionName() const;

private:
    QVector<QString>  m_qrcodes;
    double            m_pressureBar;

    // 执行状态
    QHash<qint64, QString>         m_pendingMap;      // uuid -> qrCode
    QList<QMetaObject::Connection> m_connections;
    int        m_totalCount = 0;
    QAtomicInt m_completedCount{0};
    bool       m_stopped = false;

    // 汇总结果
    int         m_successCount = 0;
    QStringList m_failedQrCodes;
    QStringList m_targetQrCodes;

    bool    m_allFinishedEmitted = false;

    ILogger deviceLogger;               ///< 设备详细日志记录器（subFunction 级共享）
    bool m_loggerInitialized = false;   ///< deviceLogger 路径是否已设置
};

#endif // SET_PNEUMATIC_VALVE_PRESSURE_TASK_H
