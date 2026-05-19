#ifndef COMMUNICATION_RECORDER_H
#define COMMUNICATION_RECORDER_H

#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QObject>
#include <QMap>
#include <QString>
#include <QTimer>

/**
 * @brief 通讯记录采集器（节流器）
 *
 * 作用：对 Modbus 通讯指令的上报频率进行节流，减轻 UI 日志写入压力。
 *
 * 范围：
 *  - 仅对 THROTTLED_COMMAND_ID 指令（默认 "ReadFoupStatus"）进行节流上报；
 *  - 其它指令通过 submitCommand() 立即全量发射 shouldEmit 信号。
 *
 * 节流策略（仅作用于 THROTTLED_COMMAND_ID）：
 *  - 当 FoupOfOHBInfo::foupIn == true（设备工作中）：每 1s 上报一次最新指令
 *  - 当 FoupOfOHBInfo::foupIn == false（设备空闲）：每 3s 上报一次最新指令
 *
 * 实现：
 *  - 内部使用 QMap<QString, int> 记录每个设备累积的毫秒数
 *  - QTimer 每 1s 触发一次，累加计数器
 *  - 计数器达到阈值时发射 shouldEmit 信号，并重置计数器
 *  - submitCommand() 对目标指令只存储最新值并参与节流；其他指令直接转发
 *
 * 使用：
 *  - 创建实例，调用 start() 启动定时器
 *  - 外部接收到指令时调用 submitCommand()
 *  - 连接 shouldEmit 信号到目标槽函数
 */
class CommunicationRecorder : public QObject
{
    Q_OBJECT

public:
    explicit CommunicationRecorder(QObject* parent = nullptr);
    ~CommunicationRecorder();

    // 启动定时器
    void start();

    // 停止定时器
    void stop();

public slots:
    /**
     * @brief 提交一条指令给采集器（由外部调用）
     * @param cmd 指令对象
     * @param masterId 设备 qrCode
     */
    void submitCommand(const ModbusCommand& cmd, const QString& masterId);

signals:
    /**
     * @brief 通知外部可以发射采集信号了（达到节流阈值）
     * @param cmd 最新一条指令
     * @param masterId 设备 qrCode
     */
    void shouldEmit(ModbusCommand cmd, QString masterId);

private slots:
    void onTick();

private:
    // 节流常量（ms）
    static constexpr int TICK_INTERVAL_MS      = 1000;  // 定时器间隔
    static constexpr int THRESHOLD_FOUP_IN_MS  = 1000;  // Foup 存在时阈值 1s
    static constexpr int THRESHOLD_FOUP_OUT_MS = 3000;  // Foup 空闲时阈值 3s

    // 仅针对该指令进行节流（对应 ModbusTcpMasterConfig.xml 中 Command id）
    static constexpr const char* THROTTLED_COMMAND_ID = "ReadFoupStatus";

    QTimer* m_timer = nullptr;
    QMap<QString, int>           m_counterMs;   // qrCode -> 累积毫秒数
    QMap<QString, ModbusCommand> m_latestCmd;   // qrCode -> 最新指令
};

#endif // COMMUNICATION_RECORDER_H
