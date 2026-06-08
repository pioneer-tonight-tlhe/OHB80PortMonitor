#include "cycliccommandissuer.h"
#include <QDebug>

// ============================================================
// CyclicCommandIssuer - 循环下发 Modbus 指令器实现
// ============================================================

CyclicCommandIssuer::CyclicCommandIssuer(ModbusCommandSender& sender, QObject* parent)
    : QObject(parent)
    , m_sender(sender)
{
    m_intervalTimer = new QTimer(this);
    m_intervalTimer->setSingleShot(true);
    connect(m_intervalTimer, &QTimer::timeout,
            this, &CyclicCommandIssuer::sendCurrentCommand);
    connect(&m_sender, &ModbusCommandSender::commandFinished,
            this, &CyclicCommandIssuer::onCommandFinished);
}

// ============================================================
// 公开方法 - 配置
// ============================================================

void CyclicCommandIssuer::setCommandQueue(const QList<ModbusCommand>& queue)
{
    m_commandQueue = queue;
    m_queueIds.clear();
    for (const auto& cmd : queue) m_queueIds.insert(cmd.id);
}

void CyclicCommandIssuer::setInterval(int intervalMs)
{
    m_intervalMs = qMax(0, intervalMs);
}

void CyclicCommandIssuer::setExecutionCount(int count)
{
    m_executionCount = qMax(0, count);
}

// ============================================================
// 公开方法 - 启停
// ============================================================

void CyclicCommandIssuer::start()
{
    if (m_commandQueue.isEmpty()) {
        qDebug() << "[CyclicCommandIssuer] 指令队列为空，无法启动";
        emit logMessage("[CyclicCommandIssuer] 指令队列为空，无法启动");
        return;
    }

    m_running = true;
    m_completedRounds = 0;
    m_failedCommands.clear();
    m_currentIndex = 0;
    m_pendingCount = 0;
    m_waitingForBusy = false;

    qDebug() << "[CyclicCommandIssuer] 启动循环下发"
             << "指令数:" << m_commandQueue.size()
             << "轮数:" << (m_executionCount == 0 ? "无限" : QString::number(m_executionCount))
             << "间隔:" << m_intervalMs << "ms";
    emit logMessage(QString("[CyclicCommandIssuer] 启动循环下发 指令数:%1 轮数:%2 间隔:%3ms")
                    .arg(m_commandQueue.size())
                    .arg(m_executionCount == 0 ? "无限" : QString::number(m_executionCount))
                    .arg(m_intervalMs));

    sendCurrentCommand();
}

void CyclicCommandIssuer::stop()
{
    m_running = false;
    m_waitingForBusy = false;
    m_intervalTimer->stop();
    qDebug() << "[CyclicCommandIssuer] 已停止";
    emit logMessage("[CyclicCommandIssuer] 已停止");
}

// ============================================================
// 私有方法 - 发送当前指令
// ============================================================

void CyclicCommandIssuer::sendCurrentCommand()
{
    // 繁忙等待中或已停止不发送
    if (!m_running || m_waitingForBusy) return;
    if (m_currentIndex >= m_commandQueue.size()) return;

    ModbusCommand cmd = m_commandQueue[m_currentIndex];
    cmd.resetState();

    qDebug() << "[CyclicCommandIssuer] 发送指令:" << cmd.id
             << "索引:" << m_currentIndex;
    emit logMessage(QString("[CyclicCommandIssuer] 发送指令:%1 索引:%2").arg(cmd.id).arg(m_currentIndex));

    m_currentIndex++;
    m_pendingCount++;
    m_sender.submit(cmd);

    // 若还有更多指令，按间隔定时触发下一次发送
    if (m_currentIndex < m_commandQueue.size()) {
        m_intervalTimer->start(m_intervalMs);
    }
}

// ============================================================
// 槽函数 - 处理 commandFinished 信号
// ============================================================

void CyclicCommandIssuer::onCommandFinished(ModbusCommand cmd)
{
    if (!m_running) return;

    bool isOurCmd = m_queueIds.contains(cmd.id);

    // ---- 繁忙等待阶段：等非繁忙 commandFinished 后再恢复发送 ----
    if (m_waitingForBusy) {
        if (isOurCmd) {
            m_pendingCount--;
            if (cmd.deviceBusy) {
                // 本队列指令仍繁忙：回退索引，等恢复后重试
                m_currentIndex--;
                qDebug() << "[CyclicCommandIssuer] 繁忙等待中 - 指令仍繁忙，回退重试:" << cmd.id;
                emit logMessage(QString("[CyclicCommandIssuer] 繁忙等待中 - 指令仍繁忙，回退重试:%1").arg(cmd.id));
            } else {
                // 本队列指令已完成（非繁忙）：记录结果，设备已空闲
                if (cmd.received) {
                    qDebug() << "[CyclicCommandIssuer] 繁忙等待中 - 指令成功:" << cmd.id;
                    emit logMessage(QString("[CyclicCommandIssuer] 繁忙等待中 - 指令成功:%1").arg(cmd.id));
                } else {
                    m_failedCommands.append(cmd);
                    qDebug() << "[CyclicCommandIssuer] 繁忙等待中 - 指令失败:" << cmd.id;
                    emit logMessage(QString("[CyclicCommandIssuer] 繁忙等待中 - 指令失败:%1").arg(cmd.id));
                }
                qDebug() << "[CyclicCommandIssuer] 繁忙已解除（本队列），恢复发送";
                emit logMessage("[CyclicCommandIssuer] 繁忙已解除（本队列），恢复发送");
                m_waitingForBusy = false;
                m_currentIndex < m_commandQueue.size()
                    ? m_intervalTimer->start(m_intervalMs)
                    : checkRoundComplete();
            }
        } else {
            // 外部指令完成
            if (!cmd.deviceBusy) {
                qDebug() << "[CyclicCommandIssuer] 繁忙已解除（外部信号），恢复发送";
                emit logMessage("[CyclicCommandIssuer] 繁忙已解除（外部信号），恢复发送");
                m_waitingForBusy = false;
                m_currentIndex < m_commandQueue.size()
                    ? m_intervalTimer->start(m_intervalMs)
                    : checkRoundComplete();
            } else {
                qDebug() << "[CyclicCommandIssuer] 繁忙等待中 - 外部繁忙信号，继续等待";
                emit logMessage("[CyclicCommandIssuer] 繁忙等待中 - 外部繁忙信号，继续等待");
            }
        }
        return;
    }

    // ---- 正常运行：过滤非本队列指令 ----
    if (!isOurCmd) return;

    m_pendingCount--;

    // ---- 设备繁忙：回退索引，停定时器，等设备空闲后重试同一条指令 ----
    if (cmd.deviceBusy) {
        qDebug() << "[CyclicCommandIssuer] 设备繁忙，等待重试 -" << cmd.id;
        emit logMessage(QString("[CyclicCommandIssuer] 设备繁忙，等待重试 - %1").arg(cmd.id));
        m_intervalTimer->stop();
        m_currentIndex--;  // 回退：恢复后重试同一条指令
        m_waitingForBusy = true;
        return;
    }

    // ---- 成功 ----
    if (cmd.received) {
        qDebug() << "[CyclicCommandIssuer] 指令成功:" << cmd.id;
        emit logMessage(QString("[CyclicCommandIssuer] 指令成功:%1").arg(cmd.id));
        emit commandSucceeded(cmd);
    } else {
        // 超时 / 校验错误 / 其他失败
        qDebug() << "[CyclicCommandIssuer] 指令失败:" << cmd.id
                 << "原因:" << cmd.errorMessage
                 << "发送次数:" << cmd.sendCount;
        emit logMessage(QString("[CyclicCommandIssuer] 指令失败:%1 原因:%2 发送次数:%3")
                .arg(cmd.id).arg(cmd.errorMessage).arg(cmd.sendCount));
        m_failedCommands.append(cmd);
    }

    checkRoundComplete();
}

// ============================================================
// 私有方法 - 检查本轮是否完成
// ============================================================

void CyclicCommandIssuer::checkRoundComplete()
{
    // 仍有指令待发送，或仍有响应未收到，本轮未完成
    if (m_currentIndex < m_commandQueue.size()) return;
    if (m_pendingCount > 0) return;

    // ---- 一轮完成 ----
    m_completedRounds++;
    qDebug() << "[CyclicCommandIssuer] 第" << m_completedRounds << "轮完成"
             << "失败:" << m_failedCommands.size() << "条";
    emit logMessage(QString("[CyclicCommandIssuer] 第%1轮完成 失败:%2条")
            .arg(m_completedRounds).arg(m_failedCommands.size()));
    emit roundFinished(m_failedCommands);

    // 全部轮次执行完毕
    if (m_executionCount > 0 && m_completedRounds >= m_executionCount) {
        m_running = false;
        qDebug() << "[CyclicCommandIssuer] 全部轮次执行完毕";
        emit logMessage("[CyclicCommandIssuer] 全部轮次执行完毕");
        emit allRoundsFinished();
        return;
    }

    // 准备下一轮
    m_failedCommands.clear();
    m_currentIndex = 0;
    m_pendingCount = 0;
    m_waitingForBusy = false;
    m_intervalTimer->start(m_intervalMs);
}
