#include "set_firmware_config_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/firmwareupgrader.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "loggermanager.h"
#include "defer/defer.h"

#include <QDebug>

SetFirmwareConfigTask::SetFirmwareConfigTask(QObject *parent)
    : SchedulerTask(parent)
{
    qDebug() << "=============================SetFirmwareConfigTask 调度任务开始=============================";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[SetFirmwareConfigTask][构造] 任务创建");
    LoggerManager::getInstance()->flush(m_taskLogPath);
}

SetFirmwareConfigTask::~SetFirmwareConfigTask()
{
    qDebug() << "=============================SetFirmwareConfigTask 调度任务结束=============================";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[SetFirmwareConfigTask][析构] 任务销毁");
    LoggerManager::getInstance()->flush(m_taskLogPath);
}

void SetFirmwareConfigTask::setPrepareTimeout(int ms)       { m_prepareTimeout       = ms; }
void SetFirmwareConfigTask::setWaitingTime(int ms)          { m_waitingTime          = ms; }
void SetFirmwareConfigTask::setSendInterval(int ms)         { m_sendInterval         = ms; }
void SetFirmwareConfigTask::setTransferTimeout(int ms)      { m_transferTimeout      = ms; }
void SetFirmwareConfigTask::setPostTransferWaitTime(int ms) { m_postTransferWaitTime = ms; }

void SetFirmwareConfigTask::start()
{
    Tool::Defer defer([this]() { LoggerManager::getInstance()->flush(m_taskLogPath); });
    
    setState(Running);

    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[SetFirmwareConfigTask][start] 任务开始");

    // 写入运行日志：任务启动
    if (auto* opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QStringLiteral("SetFirmwareConfig task started"), 0);
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    const QStringList ids = mgr.masterIds();

    int appliedCount = 0;
    for (const QString &id : ids) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) continue;
        FirmwareUpgrader *upgrader = master->firmwareUpgrader();
        if (!upgrader) continue;

        if (m_prepareTimeout.has_value())       upgrader->setPrepareTimeout(m_prepareTimeout.value());
        if (m_waitingTime.has_value())          upgrader->setWaitingTime(m_waitingTime.value());
        if (m_sendInterval.has_value())         upgrader->setSendInterval(m_sendInterval.value());
        if (m_transferTimeout.has_value())      upgrader->setTransferTimeout(m_transferTimeout.value());
        if (m_postTransferWaitTime.has_value()) upgrader->setPostTransferWaitTime(m_postTransferWaitTime.value());

        qDebug() << "[SetFirmwareConfigTask] Applied config to" << id;
        LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
            QString("[SetFirmwareConfigTask][start] 应用配置到设备 %1").arg(id).toStdString());
        appliedCount++;
    }

    if (m_prepareTimeout.has_value())       emit prepareTimeoutApplied();
    if (m_waitingTime.has_value())          emit waitingTimeApplied();
    if (m_sendInterval.has_value())         emit sendIntervalApplied();
    if (m_transferTimeout.has_value())      emit transferTimeoutApplied();
    if (m_postTransferWaitTime.has_value()) emit postTransferWaitTimeApplied();

    setState(Finished);
    emit finished(true, "Firmware config applied");
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        QString("[SetFirmwareConfigTask][start] 固件升级参数配置完成，应用到 %1 个设备").arg(appliedCount).toStdString());

    // 写入运行日志：任务完成
    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        opTaskEnd->log(OperationDispatchTask::MsgType::Message,
                       QString("SetFirmwareConfig task completed: applied to %1 devices").arg(appliedCount), 0);
    } else {
        qWarning() << "[Scheduler][SetFirmwareConfigTask] OperationDispatchTask is null, cannot write summary log";
    }
}

void SetFirmwareConfigTask::stop()
{
    Tool::Defer defer([this]() { LoggerManager::getInstance()->flush(m_taskLogPath); });
    
    setState(Cancelled);
    emit finished(false, "SetFirmwareConfigTask: 任务被取消");
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
        "[SetFirmwareConfigTask][stop] 任务被取消");
}
