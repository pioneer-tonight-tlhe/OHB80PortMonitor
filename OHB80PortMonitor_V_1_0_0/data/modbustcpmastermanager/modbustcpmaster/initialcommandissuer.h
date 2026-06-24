/*******************************************************************************************
 * @file initialcommandissuer.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class InitialCommandIssuer
 * @brief 负责按轮次下发设备初始化 Modbus 指令并汇总初始化结果。
 *
 * 设计目标：
 *      1. 独立管理初始化队列，不依赖周期轮询执行器的全队列重发逻辑。
 *      2. 支持成功指令出队、失败指令进入下一轮，减少重复初始化下发。
 *      3. 统一汇总初始化失败原因，供调度层写入运行日志。
 *******************************************************************************************/
#ifndef INITIALCOMMANDISSUER_H
#define INITIALCOMMANDISSUER_H

#include "modbuscommand.h"
#include "modbuscommandsender.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QStringList>

class OHBDeviceConfigInfo;
class ModbusTcpMaster;

class InitialCommandIssuer : public QObject
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit InitialCommandIssuer(ModbusCommandSender& sender,
                                  const QString& masterId = QString(),
                                  ModbusTcpMaster* master = nullptr,
                                  QObject* parent = nullptr);

    InitialCommandIssuer(const InitialCommandIssuer&) = delete;
    InitialCommandIssuer& operator=(const InitialCommandIssuer&) = delete;

    // ============================ 队列配置 ============================
    void setCommandQueue(const QList<ModbusCommand>& queue);
    void setInterval(int intervalMs);
    void setExecutionCount(int count);

    // ============================ 轮次控制 ============================
    void start();
    void stop();

    // ============================ 状态查询 ============================
    bool isRunning() const { return m_running; }
    QList<ModbusCommand> commandQueue() const { return m_configuredCommandQueue; }

private:
    // ---- 队列配置 ----
    QList<ModbusCommand> buildConfiguredCommandQueue(const QList<ModbusCommand>& queue);
    void configureCommand(ModbusCommand& cmd, const OHBDeviceConfigInfo& deviceInfo);

    // ---- 轮次控制 ----
    bool validateSuccessfulCommand(ModbusCommand& cmd);
    bool handleCommandSuccess(ModbusCommand& cmd);
    void finishCurrentRoundIfNeeded();
    void startNextRound();
    void completeInitialization();
    void appendErrorMessage(const QString& message);
    QString commandInstanceKey(const ModbusCommand& cmd) const;

signals:
    // ---- 初始化结果 ----
    void finish(bool isOk, QStringList errorMsgList);

private slots:
    // ---- 指令下发 ----
    void sendCurrentCommand();
    void onCommandFinished(ModbusCommand cmd, QString masterId);

private:
    // ---- 依赖对象 ----
    ModbusCommandSender& m_sender;              // Modbus 指令发送器。
    ModbusTcpMaster* m_master = nullptr;        // 所属 Master，用于写入运行态信息。
    QTimer* m_intervalTimer = nullptr;          // 指令下发间隔定时器。

    // ---- 身份信息 ----
    QString m_masterId;                         // Master 设备 ID。

    // ---- 指令队列 ----
    QList<ModbusCommand> m_configuredCommandQueue;  // 已配置的完整初始化指令队列。
    QList<ModbusCommand> m_currentRoundQueue;       // 当前轮待下发指令队列。
    QList<ModbusCommand> m_nextRoundQueue;          // 下一轮待重试指令队列。
    QList<ModbusCommand> m_finalFailedCommands;     // 轮次用完后仍失败的指令队列。
    QHash<QString, ModbusCommand> m_pendingCommandMap; // 已提交但尚未完成的指令表。

    // ---- 运行状态 ----
    QStringList m_errorMsgList;                 // 初始化错误信息列表。
    int m_currentIndex = 0;                     // 当前轮已提交的指令索引。
    int m_intervalMs = 1000;                    // 指令下发间隔，单位 ms。
    int m_executionCount = 1;                   // 初始化队列总下发轮次。
    int m_completedRounds = 0;                  // 已完成的初始化轮次数。
    bool m_running = false;                     // 初始化器是否正在运行。
    bool m_stopRequested = false;               // 是否已请求停止。
};

#endif // INITIALCOMMANDISSUER_H
