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

/**
 * @brief 设置空闲净化参数任务
 *
 * 该任务用于向设备发送 Modbus 指令来设置空闲净化相关参数，包括：
 * - Enable: 启用/禁用空闲净化功能
 * - PurgeTime: 净化持续时间
 * - PurgeInterval: 净化间隔时间
 */
class SetIdlePurgeTask : public SchedulerTask
{
    Q_OBJECT

public:
    /**
     * @brief 空闲净化属性枚举
     */
    enum class IdlePurgeProperty {
        Enable,        ///< 启用/禁用空闲净化
        PurgeTime,     ///< 净化持续时间（秒）
        PurgeInterval  ///< 净化间隔时间（秒）
    };

    /**
     * @brief 构造函数
     * @param property 要设置的属性类型
     * @param value 要设置的值
     * @param parent 父对象
     */
    explicit SetIdlePurgeTask(IdlePurgeProperty property,
                              quint16 value,
                              QObject *parent = nullptr);
    ~SetIdlePurgeTask();

    /**
     * @brief 启动任务，向所有设备发送设置指令
     */
    void start() override;

    /**
     * @brief 停止任务
     */
    void stop() override;

    /**
     * @brief 返回任务类型标识
     * @return 任务类型字符串
     */
    QString taskType() const override { return "SetIdlePurgeTask"; }

    /**
     * @brief 将属性枚举转换为字符串
     * @param p 属性枚举
     * @return 属性名称字符串
     */
    static QString propertyToString(IdlePurgeProperty p);

signals:
    /**
     * @brief 任务完成信号
     * @param allSuccess 是否全部成功
     * @param successCount 成功的设备数量
     * @param failedQrCodes 失败的设备二维码列表
     * @param propertyName 属性名称
     * @param setValue 设置的值
     */
    void allFinished(bool allSuccess,
                     int successCount,
                     QStringList failedQrCodes,
                     QString propertyName,
                     quint16 setValue);

    /**
     * @brief 设备重试信号
     * @param qrCode 设备二维码
     * @param retryCount 当前重试次数
     * @param maxRetry 最大重试次数
     */
    void deviceRetrying(QString qrCode, int retryCount, int maxRetry);

private slots:
    /**
     * @brief Modbus 指令完成槽函数
     * @param cmd 指令对象
     * @param masterId 设备 ID
     */
    void onCommandFinished(ModbusCommand cmd, const QString &masterId);

    /**
     * @brief Modbus 指令超时重试槽函数
     * @param cmd 指令对象
     * @param masterId 设备 ID
     */
    void onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId);

private:
    /**
     * @brief 根据属性获取对应的 Modbus 指令 ID
     * @param p 属性枚举
     * @return Modbus 指令 ID
     */
    QString commandIdForProperty(IdlePurgeProperty p) const;

    /**
     * @brief 构建寄存器值字节数组
     * @param value 寄存器值
     * @return 字节数组
     */
    QByteArray buildRegisterValue(quint16 value) const;

    /**
     * @brief 将值转换为带单位的字符串
     * @param value 值
     * @return 带单位的字符串
     */
    QString valueWithUnit(quint16 value) const;

    void writeDeviceSkipLog(const QString& qrCode, const QString& commandId, const QString& reason);
    void writeDeviceCommandLog(const QString& qrCode, const ModbusCommand& cmd, bool success);
    QString commandFrameLogString(const ModbusCommand& cmd) const;
    QString deviceLogPath() const;
    static QString safeLogPathSegment(const QString& value);
    ILogger& deviceDetailLogger();

    /**
     * @brief 当前属性对应的日志子功能名称
     */
    QString subFunctionName() const;

    /**
     * @brief 断开所有信号连接
     */
    void disconnectAll();

    /**
     * @brief 检查所有设备是否完成
     */
    void checkAllFinished();

    /**
     * @brief 强制完成任务
     */
    void forceFinish();

    /**
     * @brief 记录失败设备到操作日志
     * @param opTask 操作任务
     * @param qrcode 设备二维码
     */
    void logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode);

private:
    IdlePurgeProperty m_property;  ///< 要设置的属性
    quint16 m_value;               ///< 要设置的值

    QHash<qint64, QString> m_pendingMap;           ///< 待处理指令 UUID -> 设备 ID 映射
    QList<QMetaObject::Connection> m_connections;  ///< 信号连接列表
    int m_totalCount = 0;                           ///< 总指令数
    QAtomicInt m_completedCount{0};                 ///< 已完成指令数（原子操作）
    bool m_stopped = false;                         ///< 是否已停止

    int m_successCount = 0;           ///< 成功设备数
    QStringList m_failedQrCodes;      ///< 失败设备二维码列表
    QStringList m_targetQrCodes;      ///< 目标设备二维码列表

    bool m_allFinishedEmitted = false;  ///< 是否已发送完成信号

    ILogger deviceLogger;               ///< 设备详细日志记录器（subFunction 级共享）
    bool m_loggerInitialized = false;   ///< deviceLogger 路径是否已设置
};

#endif // SET_IDLE_PURGE_TASK_H
