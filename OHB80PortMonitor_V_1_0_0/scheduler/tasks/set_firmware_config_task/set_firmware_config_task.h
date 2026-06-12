#ifndef SET_FIRMWARE_CONFIG_TASK_H
#define SET_FIRMWARE_CONFIG_TASK_H

#include "../../scheduler_task.h"

#include <optional>


class SetFirmwareConfigTask : public SchedulerTask
{
    Q_OBJECT

public:
    explicit SetFirmwareConfigTask(QObject *parent = nullptr);
    ~SetFirmwareConfigTask();

    // 参数设置方法（只有被调用的参数才会被应用到所有设备）
    void setPrepareTimeout(int ms);
    void setWaitingTime(int ms);
    void setSendInterval(int ms);
    void setTransferTimeout(int ms);
    void setPostTransferWaitTime(int ms);

    // SchedulerTask 接口
    void start() override;
    void stop() override;
    QString taskType() const override { return "SetFirmwareConfigTask"; }

signals:
    // 各参数应用完成信号
    void prepareTimeoutApplied();
    void waitingTimeApplied();
    void sendIntervalApplied();
    void transferTimeoutApplied();
    void postTransferWaitTimeApplied();

private:
    std::optional<int> m_prepareTimeout;
    std::optional<int> m_waitingTime;
    std::optional<int> m_sendInterval;
    std::optional<int> m_transferTimeout;
    std::optional<int> m_postTransferWaitTime;

    const std::string m_taskLogPath = "scheduler/set_firmware_config_task";
};

#endif // SET_FIRMWARE_CONFIG_TASK_H
