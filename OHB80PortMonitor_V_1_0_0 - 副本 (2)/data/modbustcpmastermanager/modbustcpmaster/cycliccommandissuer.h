#ifndef CYCLICCOMMANDISSUER_H
#define CYCLICCOMMANDISSUER_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QTimer>
#include "modbuscommand.h"

#include "modbuscommandsender.h"

// ============================================================
// CyclicCommandIssuer - 循环下发 Modbus 指令器
//
// 按顺序逐条下发指令队列中的指令，通过发送器的
// commandFinished 信号异步获取指令执行结果。
//
// 流程：定时器每隔 intervalMs 触发一次发送，不等待 commandFinished
//   - 响应异步收集，全部响应到达后发射 roundFinished
//   - 成功/失败（超时/校验错误）：记录结果，继续定时发送
//   - 设备繁忙：暂停定时器，等下一个非繁忙 commandFinished 信号后恢复发送
//
// 每轮完成后发射 roundFinished(失败列表)；
// 全部轮次完毕后发射 allRoundsFinished()。
// ============================================================
class CyclicCommandIssuer : public QObject
{
    Q_OBJECT

public:
    explicit CyclicCommandIssuer(ModbusCommandSender& sender, QObject* parent = nullptr);

    // 设置待执行的指令队列
    void setCommandQueue(const QList<ModbusCommand>& queue);

    // 设置指令间发送间隔（ms，默认 1000）
    void setInterval(int intervalMs);

    // 设置执行轮数（0 = 无限循环，默认 1）
    void setExecutionCount(int count);

    // 启动循环下发
    void start();

    // 停止循环下发
    void stop();

    // 是否正在运行
    bool isRunning() const { return m_running; }

    // 获取当前指令队列
    QList<ModbusCommand> commandQueue() const { return m_commandQueue; }

signals:
    // 完成一轮指令下发（参数：本轮失败的指令列表）
    void roundFinished(QList<ModbusCommand> failedCommands);

    // 全部轮次执行完毕
    void allRoundsFinished();

    // 某条指令成功完成（received=true）
    void commandSucceeded(ModbusCommand cmd);

    // 日志消息信号（参数：日志消息）
    void logMessage(QString message);

private slots:
    // 处理 ModbusCommandSender::commandFinished 信号
    void onCommandFinished(ModbusCommand cmd);

private:
    // 按定时器节拍发送下一条指令
    void sendCurrentCommand();

    // 检查本轮是否已全部发送且响应已收齐，若是则发射 roundFinished
    void checkRoundComplete();

    ModbusCommandSender& m_sender;

    QList<ModbusCommand> m_commandQueue;        // 待执行指令队列
    QList<ModbusCommand> m_failedCommands;      // 本轮失败指令队列

    int  m_currentIndex         = 0;            // 已发送的指令数（发送时递增）
    int  m_pendingCount         = 0;            // 已发送但尚未收到响应的指令数
    int  m_intervalMs           = 1000;         // 指令间发送间隔（ms）
    int  m_executionCount       = 1;            // 执行轮数（0=无限）
    int  m_completedRounds      = 0;            // 已完成轮数

    bool m_running              = false;        // 运行状态
    bool m_waitingForBusy       = false;        // 是否因设备繁忙而暂停定时器

    QSet<QString> m_queueIds;                   // 当前队列中所有指令的 ID 集合
    QTimer* m_intervalTimer     = nullptr;      // 发送间隔定时器（单次触发）

protected:
    // 供子类访问发送器（配置超时、重试次数等）
    ModbusCommandSender& senderRef() { return m_sender; }
};

#endif // CYCLICCOMMANDISSUER_H
