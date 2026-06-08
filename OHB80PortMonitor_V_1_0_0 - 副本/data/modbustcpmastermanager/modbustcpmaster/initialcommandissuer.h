#ifndef INITIALCOMMANDISSUER_H
#define INITIALCOMMANDISSUER_H

#include "cycliccommandissuer.h"

// ============================================================
// InitialCommandIssuer - 初始指令下发器
//
// 继承 CyclicCommandIssuer，固定执行 1 轮。
// 指令的超时时间和重试次数由指令自身属性控制。
// 全部指令完成后发遁 finished 信号并自动销毁。
//
// 使用方法：
//   1. 构造时传入 ModbusCommandSender 引用
//   2. 调用 setCommandQueue() / setInterval()
//   3. 调用 start() 启动
//   4. 连接 finished 信号，对象完成后自动销毁
// ============================================================
class InitialCommandIssuer : public CyclicCommandIssuer
{
    Q_OBJECT

public:
    explicit InitialCommandIssuer(ModbusCommandSender& sender, const QString& masterId = QString(), QObject* parent = nullptr);

    // 禁用拷贝和赋值（引用成员不支持）
    InitialCommandIssuer(const InitialCommandIssuer&) = delete;
    InitialCommandIssuer& operator=(const InitialCommandIssuer&) = delete;

signals:
    // 全部指令处理完毕，参数为失败指令列表（全部成功则为空）
    void finished(QList<ModbusCommand> failedCommands);

private slots:
    void onRoundComplete(QList<ModbusCommand> failedCommands);
    void onLogMessage(QString message);

private:
    QString& m_masterId;                // Master 设备 ID（引用）
};

#endif // INITIALCOMMANDISSUER_H
