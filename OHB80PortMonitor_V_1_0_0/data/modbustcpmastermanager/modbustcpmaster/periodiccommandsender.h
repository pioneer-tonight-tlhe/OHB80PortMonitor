#ifndef PERIODICCOMMANDSENDER_H
#define PERIODICCOMMANDSENDER_H

#include "cycliccommandissuer.h"

// ============================================================
// PeriodicCommandSender - 定时循环指令发送器
//
// 继承 CyclicCommandIssuer，执行轮次为无限循环。
// 指令的超时时间和重试次数由指令自身属性控制。
// 连续失败轮次达到阈値时发遁 disconnectDevice 信号。
//
// 使用方法：
//   1. 构造时传入 ModbusCommandSender 引用
//   2. 调用 setCommandQueue() / setInterval()
//   3. 调用 start() 启动
//   4. 连接 commandCompleted / disconnectDevice 信号
// ============================================================
class PeriodicCommandSender : public CyclicCommandIssuer
{
    Q_OBJECT

public:
    static constexpr int MAX_CONSECUTIVE_FAILURES = 10;

    explicit PeriodicCommandSender(ModbusCommandSender& sender, const QString& masterId = QString(), QObject* parent = nullptr);

    // 禁用拷贝和赋值（引用成员不支持）
    PeriodicCommandSender(const PeriodicCommandSender&) = delete;
    PeriodicCommandSender& operator=(const PeriodicCommandSender&) = delete;

signals:
    // 指令成功完成（携带 Master ID 方便调度层使用）
    void commandCompleted(ModbusCommand cmd, QString masterId);

    // 连续失败轮次达到阈値，请求断开设备
    void disconnectDevice();

private slots:
    void onRoundComplete(QList<ModbusCommand> failedCommands);
    void onLogMessage(QString message);

private:
    QString& m_masterId;                // Master 设备 ID（引用）
    int m_consecutiveFailRounds = 0;    // 连续失败轮次计数器 
};

#endif // PERIODICCOMMANDSENDER_H
