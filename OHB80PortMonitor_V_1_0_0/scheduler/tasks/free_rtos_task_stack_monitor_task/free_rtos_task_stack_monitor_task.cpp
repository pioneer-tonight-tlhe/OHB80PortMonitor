#include "free_rtos_task_stack_monitor_task.h"

#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"

#include <QMetaType>
#include <QTimer>

namespace {
constexpr const char *kCommandId = "ReadFreeRTOSTaskMinStackWaterLevels";
}

FreeRTOSTaskStackMonitorTask::FreeRTOSTaskStackMonitorTask(QObject *parent)
    : SchedulerTask(parent)
    , m_logger("scheduler/free_rtos_task_stack_monitor_task/summary")
{
    qRegisterMetaType<QVector<int>>("QVector<int>");
}

void FreeRTOSTaskStackMonitorTask::start()
{
    if (!m_periodTimer) {
        m_periodTimer = new QTimer(this);
        m_periodTimer->setInterval(PollIntervalMs);
        connect(m_periodTimer, &QTimer::timeout,
                this, &FreeRTOSTaskStackMonitorTask::onPeriodTimeout);
    }

    m_stopped = false;
    setState(Running);

    if (!m_periodTimer->isActive()) {
        m_periodTimer->start();
    }

    m_logger.info("[start] FreeRTOS min-stack monitor task started, pollIntervalMs={}", PollIntervalMs);
    pollAllDevices();
}

void FreeRTOSTaskStackMonitorTask::stop()
{
    m_stopped = true;

    if (m_periodTimer && m_periodTimer->isActive()) {
        m_periodTimer->stop();
    }

    disconnectAll();
    m_pendingMap.clear();
    m_roundRunning = false;
    m_totalCount = 0;
    m_completedCount = 0;

    setState(Cancelled);
    m_logger.info("[stop] FreeRTOS min-stack monitor task stopped");
    emit finished(false, QStringLiteral("FreeRTOSTaskStackMonitorTask stopped"));
}

void FreeRTOSTaskStackMonitorTask::onPeriodTimeout()
{
    if (m_stopped || state() != Running) {
        return;
    }

    pollAllDevices();
}

void FreeRTOSTaskStackMonitorTask::pollAllDevices()
{
    if (m_roundRunning) {
        m_logger.warn("[pollAllDevices] previous round is still running, skip this trigger, pending={}", m_pendingMap.size());
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(QString::fromLatin1(kCommandId))) {
        m_logger.error("[pollAllDevices] command template is missing, command={}", kCommandId);
        return;
    }

    disconnectAll();
    m_pendingMap.clear();
    m_totalCount = 0;
    m_completedCount = 0;
    m_roundRunning = true;

    const QStringList masterIds = mgr.masterIds();
    m_logger.info("[pollAllDevices] start polling all devices, deviceCount={}", masterIds.size());

    for (const QString &masterId : masterIds) {
        ModbusTcpMaster *master = mgr.getMaster(masterId);
        if (!master) {
            m_logger.warn("[pollAllDevices] skip device because master is missing, qrcode={}", masterId.toStdString());
            continue;
        }

        if (!master->isConnected()) {
            m_logger.warn("[pollAllDevices] skip device because it is disconnected, qrcode={}", masterId.toStdString());
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            m_logger.warn("[pollAllDevices] skip device because sender is null, qrcode={}", masterId.toStdString());
            continue;
        }

        ModbusCommand cmd = pool->clone(QString::fromLatin1(kCommandId));
        if (!cmd.isValid()) {
            m_logger.warn("[pollAllDevices] skip device because command clone failed, qrcode={}", masterId.toStdString());
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;

        m_connections.append(connect(sender, &ModbusCommandSender::commandFinished,
                                     this, &FreeRTOSTaskStackMonitorTask::onCommandFinished,
                                     Qt::QueuedConnection));
        m_connections.append(connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                     this, &FreeRTOSTaskStackMonitorTask::onCommandTimeoutRetry,
                                     Qt::QueuedConnection));

        m_pendingMap.insert(cmd.uuid, masterId);
        ++m_totalCount;

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        m_roundRunning = false;
        m_logger.warn("[pollAllDevices] no online device accepted this command");
        return;
    }

    m_logger.info("[pollAllDevices] round submitted deviceCount={}", m_totalCount);
}

void FreeRTOSTaskStackMonitorTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) {
        return;
    }

    if (!m_pendingMap.contains(cmd.uuid)) {
        return;
    }

    const QString qrCode = m_pendingMap.take(cmd.uuid);
    const bool commandOk = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    if (!commandOk) {
        m_logger.warn("[onCommandFinished] qrcode={} read failed timedOut={} checksumError={} deviceBusy={} error={}",
                      qrCode.toStdString(),
                      cmd.timedOut,
                      cmd.checksumError,
                      cmd.deviceBusy,
                      cmd.errorMessage.toStdString());
    } else {
        const QVector<int> values = parseStackWaterLevels(cmd.response.registerValue);
        if (values.size() == 4) {
            m_logger.info("[onCommandFinished] qrcode={} read success {}",
                          qrCode.toStdString(),
                          formatStackValues(values).toStdString());
            emit stackWaterLevelsUpdated(values);
        } else {
            m_logger.warn("[onCommandFinished] qrcode={} invalid response payload length, payloadBytes={}",
                          qrCode.toStdString(),
                          cmd.response.registerValue.size());
        }
    }

    ++m_completedCount;
    if (m_completedCount >= m_totalCount) {
        finishCurrentRound();
    }
}

void FreeRTOSTaskStackMonitorTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped || !m_pendingMap.contains(cmd.uuid)) {
        return;
    }

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    const int retryCount = qMax(0, cmd.sendCount - 1);
    m_logger.warn("[onCommandTimeoutRetry] qrcode={} command={} retry={}/{}",
                  qrCode.toStdString(),
                  cmd.id.toStdString(),
                  retryCount,
                  cmd.maxRetryCount);
}

void FreeRTOSTaskStackMonitorTask::disconnectAll()
{
    for (const QMetaObject::Connection &connection : qAsConst(m_connections)) {
        QObject::disconnect(connection);
    }
    m_connections.clear();
}

void FreeRTOSTaskStackMonitorTask::finishCurrentRound()
{
    disconnectAll();
    m_roundRunning = false;
    m_logger.info("[finishCurrentRound] polling round finished completed={} total={}", m_completedCount, m_totalCount);
}

QVector<int> FreeRTOSTaskStackMonitorTask::parseStackWaterLevels(const QByteArray &payload)
{
    QVector<int> values;
    if (payload.size() < 8) {
        return values;
    }

    values.reserve(4);
    for (int i = 0; i < 4; ++i) {
        const int offset = i * 2;
        const quint16 value = (static_cast<quint8>(payload.at(offset)) << 8)
                            | static_cast<quint8>(payload.at(offset + 1));
        values.append(static_cast<int>(value));
    }
    return values;
}

QString FreeRTOSTaskStackMonitorTask::formatStackValues(const QVector<int> &values)
{
    if (values.size() != 4) {
        return QStringLiteral("invalid task stack data");
    }

    return QStringLiteral("Task1=%1 words(%2 bytes), Task2=%3 words(%4 bytes), Task3=%5 words(%6 bytes), Task4=%7 words(%8 bytes)")
        .arg(values.at(0)).arg(values.at(0) * 4)
        .arg(values.at(1)).arg(values.at(1) * 4)
        .arg(values.at(2)).arg(values.at(2) * 4)
        .arg(values.at(3)).arg(values.at(3) * 4);
}
