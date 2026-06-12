#ifndef INIT_CHECK_TASK_H
#define INIT_CHECK_TASK_H

#include "../../scheduler_task.h"
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>


// 初始化检查任务
// 监听所有 ModbusTcpMaster 的 InitialCommandIssuer::finished 信号，
// 汇总检查是否存在失败的初始化指令。
class InitCheckTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit InitCheckTask(QObject *parent = nullptr);
    ~InitCheckTask();

    void start() override;
    void stop() override;
    QString taskType() const override { return "InitCheckTask"; }

signals:
    // 全部设备初始化完成后的汇总信号
    void allFinished(bool allSuccess, int successCount, int failCount,
                     const QStringList &failedMasterIds);

private slots:
    // 单个设备初始化完成回调（masterId 通过 lambda 捕获传入）
    void onInitialFinished(QList<ModbusCommand> failedCommands, const QString &masterId);

private:
    void disconnectAll();
    void checkAllFinished();

    int m_totalCount = 0;
    int m_completedCount = 0;
    bool m_stopped = false;

    int m_successCount = 0;
    QStringList m_failedMasterIds;
    QHash<QString, QList<ModbusCommand>> m_failedCommandsMap;
    QList<QMetaObject::Connection> m_connections;

    const std::string m_taskLogPath = "scheduler/init_check_task";
};

#endif // INIT_CHECK_TASK_H
