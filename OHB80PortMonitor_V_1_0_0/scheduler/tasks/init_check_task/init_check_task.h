/*******************************************************************************************
 * @file init_check_task.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class InitCheckTask
 * @brief 汇总所有设备初始化指令执行结果并转发初始化检查状态。
 *
 * 设计目标：
 *      1. 统一监听各个 ModbusTcpMaster 的初始化完成信号。
 *      2. 汇总设备初始化成功、失败数量和失败设备列表。
 *      3. 将初始化错误信息逐条写入运行日志，便于现场定位。
 *******************************************************************************************/
#ifndef INIT_CHECK_TASK_H
#define INIT_CHECK_TASK_H

#include "../../scheduler_task.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QString>
#include <QStringList>

class InitCheckTask : public SchedulerTask
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit InitCheckTask(QObject* parent = nullptr);
    ~InitCheckTask();

    // ============================ 基类相关接口 ============================
    void start() override;
    void stop() override;
    QString taskType() const override { return "InitCheckTask"; }

private:
    // ---- 初始化检查 ----
    void disconnectAll();
    void checkAllFinished();

signals:
    // ---- 初始化检查 ----
    void allFinished(bool allSuccess, int successCount, int failCount,
                     const QStringList& failedMasterIds);

private slots:
    // ---- 初始化检查 ----
    void onInitialFinished(bool isOk, QStringList errorMsgList, const QString& masterId);

private:
    // ---- 统计状态 ----
    int m_totalCount = 0;                         // 需要监听的设备总数。
    int m_completedCount = 0;                     // 已完成初始化回调的设备数量。
    int m_successCount = 0;                       // 初始化成功设备数量。
    bool m_stopped = false;                       // 任务是否已停止。

    // ---- 初始化结果 ----
    QStringList m_failedMasterIds;                // 初始化失败的 Master ID 列表。
    QHash<QString, QStringList> m_errorMsgMap;    // 各设备初始化错误信息表。

    // ---- 信号连接 ----
    QList<QMetaObject::Connection> m_connections; // 初始化完成信号连接列表。

    // ---- 日志配置 ----
    const std::string m_taskLogPath = "scheduler/init_check_task"; // 任务日志路径。
};

#endif // INIT_CHECK_TASK_H
