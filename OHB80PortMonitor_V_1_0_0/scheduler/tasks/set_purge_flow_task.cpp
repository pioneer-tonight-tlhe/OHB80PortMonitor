#include "set_purge_flow_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "app/applogger.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "loggermanager.h"

#include <QDebug>
#include <QDateTime>
#include <QTimer>

namespace {
constexpr const char *kCmdId          = "WritePurgeFlow";
constexpr int         kTotalTimeoutMs = 5000;
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

SetPurgeFlowTask::SetPurgeFlowTask(const QVector<QString> &qrcodes,
                                   int flowValue,
                                   QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_flowValue(flowValue)
{
    qDebug() << "[Scheduler][SetPurgeFlowTask] 创建任务: qrcodes=" << qrcodes
             << "flow=" << flowValue;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetPurgeFlowTask] 创建任务：设备数=%1 flow=%2")
            .arg(qrcodes.size()).arg(flowValue).toStdString());
}

SetPurgeFlowTask::~SetPurgeFlowTask()
{
    qDebug() << "[Scheduler][SetPurgeFlowTask] 任务销毁";
}

// ============================================================
// start()
// ============================================================

void SetPurgeFlowTask::start()
{
    setState(Running);
    m_stopped       = false;
    m_totalCount    = 0;
    m_completedCount.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_successCount  = 0;
    m_failedQrCodes.clear();
    m_allFinishedEmitted = false;

    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_flowValue);
        emit finished(false, "SetPurgeFlowTask: qrcode 列表为空");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetPurgeFlowTask] qrcode 列表为空");
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_flowValue);
        emit finished(false, QString("SetPurgeFlowTask: 指令 '%1' 不存在").arg(kCmdId));
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetPurgeFlowTask] 指令 '%1' 不存在").arg(kCmdId).toStdString());
        return;
    }

    // 流量 × 100 转寄存器值
    const qint64 raw = static_cast<qint64>(m_flowValue) * kRegisterScale;
    const quint16 regVal = static_cast<quint16>(qBound<qint64>(0, raw, 0xFFFF));
    const QByteArray regBytes = buildRegisterValue(regVal);

    qDebug() << "[Scheduler][SetPurgeFlowTask] flow=" << m_flowValue
             << "→ regVal=" << regVal;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetPurgeFlowTask] flow %1 → regVal %2")
            .arg(m_flowValue).arg(regVal).toStdString());

    // 写入运行日志：任务启动
    auto* opTaskStart = SharedData::getOperationDispatchTask();
    if (opTaskStart) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetPurgeFlow task started: flow=%1 for %2 devices")
                             .arg(m_flowValue).arg(m_qrcodes.size()), 0);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master || !master->isConnected() || !master->sender()) {
            qWarning() << "[Scheduler][SetPurgeFlowTask] 设备不可用，跳过:" << id;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][SetPurgeFlowTask] 设备 %1 不可用，跳过").arg(id).toStdString());
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        ModbusCommand cmd = pool->clone(kCmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetPurgeFlowTask] 克隆指令失败:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = regBytes;
        cmd.request.byteCount     = static_cast<quint8>(regBytes.size());

        if (cmd.request.functionCode == 0x06
            && cmd.request.rawBytes.size() >= 6
            && regBytes.size() >= 2) {
            cmd.request.rawBytes[4] = regBytes[0];
            cmd.request.rawBytes[5] = regBytes[1];
        }
        cmd.response.registerValue = regBytes;
        if (cmd.response.rawBytes.size() >= 6 && regBytes.size() >= 2) {
            cmd.response.rawBytes[4] = regBytes[0];
            cmd.response.rawBytes[5] = regBytes[1];
        }

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetPurgeFlowTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        m_pendingMap[cmd.uuid] = id;
        m_totalCount++;

        qDebug() << "[Scheduler][SetPurgeFlowTask] 向设备" << id << "发送 WritePurgeFlow regVal=" << regVal;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetPurgeFlowTask] 向设备 %1 发送 WritePurgeFlow regVal=%2")
                .arg(id).arg(regVal).toStdString());

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        forceFinish();
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &SetPurgeFlowTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

void SetPurgeFlowTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "SetPurgeFlowTask: 任务被取消");
}

void SetPurgeFlowTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;
    m_pendingMap.remove(cmd.uuid);

    // 写入通讯日志
    {
        const QString sentTimeStr = cmd.sentMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-");
        int execStatus = 3;
        if (cmd.received)          execStatus = 0;
        else if (cmd.timedOut)     execStatus = 1;
        else if (cmd.sendCount > 1) execStatus = 2;
        const int retryCount = qMax(0, cmd.sendCount - 1);
        QString description;
        if (execStatus != 0) {
            description = cmd.errorMessage;
        } else {
            QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
            if (!parsedData.isEmpty()) {
                QStringList parts;
                for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it)
                    parts << QString("%1=%2").arg(it.key(), it.value().toString());
                description = parts.join(", ");
            }
        }
        if (description.isEmpty()) {
            description = QStringLiteral("OK");
        }
        if (LogDB::CommunicateLogDBCon *db = LogDB::DatabaseManager::instance().communicateLogCon()) {
            const QString respTimeStr = cmd.responseMs > 0
                ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                : QString();
            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, masterId,
                             execStatus, retryCount,
                             cmd.request.rawBytes, cmd.response.rawBytes, description);
        }
    }

    const bool success = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    if (success) {
        ++m_successCount;
        qDebug() << "[Scheduler][SetPurgeFlowTask] 设备" << masterId << "设置成功";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetPurgeFlowTask] 设备 %1 设置成功").arg(masterId).toStdString());
    } else {
        m_failedQrCodes.append(masterId);
        qWarning() << "[Scheduler][SetPurgeFlowTask] 设备" << masterId
                   << "设置失败 timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetPurgeFlowTask] 设备 %1 设置失败: timedOut=%2 checksumError=%3 deviceBusy=%4")
                .arg(masterId).arg(cmd.timedOut).arg(cmd.checksumError).arg(cmd.deviceBusy).toStdString());
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, masterId);
        }
    }

    checkAllFinished();
}

void SetPurgeFlowTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;
    forceFinish();
}

void SetPurgeFlowTask::onTimeout()
{
    qWarning() << "[Scheduler][SetPurgeFlowTask] 超时，剩余" << m_pendingMap.size() << "台设备未响应";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][SetPurgeFlowTask] 超时，剩余 %1 台设备未响应")
            .arg(m_pendingMap.size()).toStdString());
    forceFinish();
}

void SetPurgeFlowTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();

    // 超时仍未响应的设备：补登失败日志
    auto* opTaskPending = SharedData::getOperationDispatchTask();
    for (const QString &qr : m_pendingMap.values()) {
        m_failedQrCodes.append(qr);
        if (opTaskPending) {
            logFailedDevice(opTaskPending, qr);
        }
    }
    m_pendingMap.clear();

    const bool allSuccess = m_failedQrCodes.isEmpty();
    setState(allSuccess ? Finished : Failed);

    // 写入运行日志：任务汇总
    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        if (allSuccess) {
            const QString desc = QString("SetPurgeFlow flow=%1 task completed: %2 devices succeeded")
                  .arg(m_flowValue).arg(m_successCount);
            opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
        } else {
            const QString desc = QString("SetPurgeFlow flow=%1 task finished: %2 succeeded, %3 failed")
                  .arg(m_flowValue).arg(m_successCount).arg(m_failedQrCodes.count());
            opTaskEnd->log(OperationDispatchTask::MsgType::Error, desc, 0);
        }
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes, m_flowValue);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetPurgeFlowTask: flow=%1 设置完成（%2 台）").arg(m_flowValue).arg(m_successCount)
                      : QString("SetPurgeFlowTask: flow=%1 完成，%2 台成功，%3 台失败")
                            .arg(m_flowValue).arg(m_successCount).arg(m_failedQrCodes.count()));

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][SetPurgeFlowTask] 任务结束: flow=%1 %2 台成功，%3 台失败")
            .arg(m_flowValue).arg(m_successCount).arg(m_failedQrCodes.count()).toStdString());
}

void SetPurgeFlowTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("SetPurgeFlow flow=%1 task failed: device %2")
        .arg(m_flowValue).arg(qrcode);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetPurgeFlowTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetPurgeFlowTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}
