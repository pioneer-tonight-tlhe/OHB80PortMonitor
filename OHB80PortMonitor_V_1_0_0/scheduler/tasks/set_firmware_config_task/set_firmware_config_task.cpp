#include "set_firmware_config_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/firmwareupgrader.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "firmwareconfig.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "loggermanager.h"
#include "defer/defer.h"

#include <QDebug>
#include <QStringList>

SetFirmwareConfigTask::SetFirmwareConfigTask(QObject *parent)
    : SchedulerTask(parent)
{
    qDebug() << "=============================SetFirmwareConfigTask start============================";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                                      "[SetFirmwareConfigTask][ctor] task created");
    LoggerManager::getInstance()->flush(m_taskLogPath);
}

SetFirmwareConfigTask::~SetFirmwareConfigTask()
{
    qDebug() << "=============================SetFirmwareConfigTask end=============================";
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                                      "[SetFirmwareConfigTask][dtor] task destroyed");
    LoggerManager::getInstance()->flush(m_taskLogPath);
}

void SetFirmwareConfigTask::setPrepareTimeout(int ms) { m_prepareTimeout = ms; }
void SetFirmwareConfigTask::setWaitingTime(int ms) { m_waitingTime = ms; }
void SetFirmwareConfigTask::setSendInterval(int ms) { m_sendInterval = ms; }
void SetFirmwareConfigTask::setTransferTimeout(int ms) { m_transferTimeout = ms; }
void SetFirmwareConfigTask::setPostTransferWaitTime(int ms) { m_postTransferWaitTime = ms; }

void SetFirmwareConfigTask::start()
{
    Tool::Defer defer([this]() { LoggerManager::getInstance()->flush(m_taskLogPath); });

    setState(Running);

    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                                      "[SetFirmwareConfigTask][start] task started");

    if (auto *opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QStringLiteral("SetFirmwareConfig task started"),
                         0);
    }

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    const QStringList masterIds = manager.masterIds();

    int appliedCount = 0;
    for (const QString &masterId : masterIds) {
        ModbusTcpMaster *master = manager.getMaster(masterId);
        if (!master) {
            continue;
        }

        FirmwareUpgrader *upgrader = master->firmwareUpgrader();
        if (!upgrader) {
            continue;
        }

        if (m_prepareTimeout.has_value()) {
            upgrader->setPrepareTimeout(m_prepareTimeout.value());
        }
        if (m_waitingTime.has_value()) {
            upgrader->setWaitingTime(m_waitingTime.value());
        }
        if (m_sendInterval.has_value()) {
            upgrader->setSendInterval(m_sendInterval.value());
        }
        if (m_transferTimeout.has_value()) {
            upgrader->setTransferTimeout(m_transferTimeout.value());
        }
        if (m_postTransferWaitTime.has_value()) {
            upgrader->setPostTransferWaitTime(m_postTransferWaitTime.value());
        }

        qDebug() << "[SetFirmwareConfigTask] applied config to" << masterId;
        LoggerManager::getInstance()->log(
            m_taskLogPath,
            Level::INFO,
            QString("[SetFirmwareConfigTask][start] applied runtime config to %1")
                .arg(masterId)
                .toStdString());
        ++appliedCount;
    }

    QString persistErrorMessage;
    const bool persistSuccess = persistConfig(&persistErrorMessage);

    if (persistSuccess) {
        if (m_prepareTimeout.has_value()) {
            emit prepareTimeoutApplied();
        }
        if (m_waitingTime.has_value()) {
            emit waitingTimeApplied();
        }
        if (m_sendInterval.has_value()) {
            emit sendIntervalApplied();
        }
        if (m_transferTimeout.has_value()) {
            emit transferTimeoutApplied();
        }
        if (m_postTransferWaitTime.has_value()) {
            emit postTransferWaitTimeApplied();
        }
    }

    setState(persistSuccess ? Finished : Failed);
    emit finished(persistSuccess,
                  persistSuccess
                      ? QString("Firmware config applied to %1 devices").arg(appliedCount)
                      : QString("Firmware config persistence failed: %1").arg(persistErrorMessage));

    LoggerManager::getInstance()->log(
        m_taskLogPath,
        persistSuccess ? Level::INFO : Level::WARN,
        persistSuccess
            ? QString("[SetFirmwareConfigTask][start] completed, applied to %1 devices")
                  .arg(appliedCount)
                  .toStdString()
            : QString("[SetFirmwareConfigTask][start] persistence failed: %1")
                  .arg(persistErrorMessage)
                  .toStdString());

    if (auto *opTaskEnd = SharedData::getOperationDispatchTask()) {
        opTaskEnd->log(persistSuccess ? OperationDispatchTask::MsgType::Message
                                      : OperationDispatchTask::MsgType::Error,
                       persistSuccess
                           ? QString("SetFirmwareConfig task completed: applied to %1 devices").arg(appliedCount)
                           : QString("SetFirmwareConfig task finished: config persistence failed (%1)")
                                 .arg(persistErrorMessage),
                       0);
    } else {
        qWarning() << "[Scheduler][SetFirmwareConfigTask] OperationDispatchTask is null";
    }
}

void SetFirmwareConfigTask::stop()
{
    Tool::Defer defer([this]() { LoggerManager::getInstance()->flush(m_taskLogPath); });

    setState(Cancelled);
    emit finished(false, QStringLiteral("SetFirmwareConfigTask: cancelled"));
    LoggerManager::getInstance()->log(m_taskLogPath, Level::INFO,
                                      "[SetFirmwareConfigTask][stop] task cancelled");
}

bool SetFirmwareConfigTask::persistConfig(QString *errorMessage)
{
    FirmwareConfig &config = FirmwareConfig::getInstance();
    QStringList failedKeys;

    if (m_prepareTimeout.has_value() && !config.setPrepareCmdTimeoutMs(m_prepareTimeout.value())) {
        failedKeys.append(QStringLiteral("PrepareCmdTimeoutTimeMs"));
    }
    if (m_waitingTime.has_value() && !config.setWaitingForEquipmentReadyMs(m_waitingTime.value())) {
        failedKeys.append(QStringLiteral("WaitingForEquipmentReadyTimeMs"));
    }
    if (m_sendInterval.has_value() && !config.setSendIntervalForDataMs(m_sendInterval.value())) {
        failedKeys.append(QStringLiteral("SendIntervalForDataTimeMs"));
    }
    if (m_transferTimeout.has_value() && !config.setTransferResponseTimeoutMs(m_transferTimeout.value())) {
        failedKeys.append(QStringLiteral("TransferResponseTimeoutTimeMs"));
    }
    if (m_postTransferWaitTime.has_value() && !config.setPostTransferWaitMs(m_postTransferWaitTime.value())) {
        failedKeys.append(QStringLiteral("PostTransferWaitTimeMs"));
    }

    if (errorMessage) {
        if (failedKeys.isEmpty()) {
            errorMessage->clear();
        } else {
            *errorMessage = QString("write firmware.ini failed, keys=%1").arg(failedKeys.join(", "));
        }
    }

    return failedKeys.isEmpty();
}
