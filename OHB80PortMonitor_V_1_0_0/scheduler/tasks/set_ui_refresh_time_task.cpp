#include "set_ui_refresh_time_task.h"
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
#include "usermanager/usermanager.h"

#include <QDebug>
#include <QDateTime>
#include <QTimer>

namespace {
constexpr const char *kCmdId          = "WriteUIRefreshTime";
constexpr int         kTotalTimeoutMs = 5000;
constexpr int         kPayloadBytes   = 6;
} // namespace

SetUIRefreshTimeTask::SetUIRefreshTimeTask(const QVector<QString> &qrcodes,
                                           int logoSec,
                                           int paramTotalSec,
                                           int paramSwitchSec,
                                           QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , m_logoSec(logoSec)
    , m_paramTotalSec(paramTotalSec)
    , m_paramSwitchSec(paramSwitchSec)
{
    qDebug() << "[Scheduler][SetUIRefreshTimeTask] 创建任务: qrcodes=" << qrcodes
             << "logoSec=" << logoSec << "paramTotalSec=" << paramTotalSec
             << "paramSwitchSec=" << paramSwitchSec;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetUIRefreshTimeTask] 创建任务：设备数=%1 logoSec=%2 paramTotalSec=%3 paramSwitchSec=%4")
            .arg(qrcodes.size()).arg(logoSec).arg(paramTotalSec).arg(paramSwitchSec).toStdString());
}

SetUIRefreshTimeTask::~SetUIRefreshTimeTask()
{
    qDebug() << "[Scheduler][SetUIRefreshTimeTask] 任务销毁";
}

void SetUIRefreshTimeTask::start()
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
        emit allFinished(false, 0, {}, m_logoSec, m_paramTotalSec, m_paramSwitchSec);
        emit finished(false, "SetUIRefreshTimeTask: qrcode 列表为空");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetUIRefreshTimeTask] qrcode 列表为空");
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_logoSec, m_paramTotalSec, m_paramSwitchSec);
        emit finished(false, QString("SetUIRefreshTimeTask: 指令 '%1' 不存在").arg(kCmdId));
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetUIRefreshTimeTask] 指令 '%1' 不存在").arg(kCmdId).toStdString());
        return;
    }

    const QByteArray payload = buildPayload();

    qDebug() << "[Scheduler][SetUIRefreshTimeTask] 数据 payload =" << payload.toHex(' ').toUpper();
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetUIRefreshTimeTask] payload=%1").arg(QString(payload.toHex(' ').toUpper())).toStdString());

    // 写入运行日志：任务启动
    auto* opTaskStart = SharedData::getOperationDispatchTask();
    if (opTaskStart) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetUIRefreshTime task started: logo=%1s total=%2s switch=%3s for %4 devices")
                             .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec).arg(m_qrcodes.size()), 0);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master || !master->isConnected() || !master->sender()) {
            qWarning() << "[Scheduler][SetUIRefreshTimeTask] 设备不可用，跳过:" << id;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][SetUIRefreshTimeTask] 设备 %1 不可用，跳过").arg(id).toStdString());
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        ModbusCommand cmd = pool->clone(kCmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][SetUIRefreshTimeTask] 克隆指令失败:" << id;
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;
        cmd.request.registerValue = payload;
        cmd.request.byteCount     = static_cast<quint8>(payload.size());

        // FC 0x10 rawBytes: [SlaveAddr][FC][AddrHi][AddrLo][CntHi][CntLo][ByteCnt][data...][CRC..]
        // 数据起始偏移 = 7
        if (cmd.request.functionCode == 0x10
            && cmd.request.rawBytes.size() >= 7 + kPayloadBytes
            && payload.size() == kPayloadBytes) {
            for (int i = 0; i < kPayloadBytes; ++i) {
                cmd.request.rawBytes[7 + i] = payload[i];
            }
        }

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetUIRefreshTimeTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        m_pendingMap[cmd.uuid] = id;
        m_totalCount++;

        qDebug() << "[Scheduler][SetUIRefreshTimeTask] 向设备" << id << "发送 WriteUIRefreshTime payload="
                 << payload.toHex(' ').toUpper();
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetUIRefreshTimeTask] 向设备 %1 发送 payload=%2")
                .arg(id).arg(QString(payload.toHex(' ').toUpper())).toStdString());

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
                this, &SetUIRefreshTimeTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

void SetUIRefreshTimeTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "SetUIRefreshTimeTask: 任务被取消");
}

void SetUIRefreshTimeTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
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
                             cmd.request.rawBytes, cmd.response.rawBytes, description,
                             UserPermission::Engineer);
        }
    }

    const bool success = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    if (success) {
        ++m_successCount;
        qDebug() << "[Scheduler][SetUIRefreshTimeTask] 设备" << masterId << "设置成功";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetUIRefreshTimeTask] 设备 %1 设置成功").arg(masterId).toStdString());
    } else {
        m_failedQrCodes.append(masterId);
        qWarning() << "[Scheduler][SetUIRefreshTimeTask] 设备" << masterId
                   << "设置失败 timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetUIRefreshTimeTask] 设备 %1 设置失败: timedOut=%2 checksumError=%3 deviceBusy=%4")
                .arg(masterId).arg(cmd.timedOut).arg(cmd.checksumError).arg(cmd.deviceBusy).toStdString());
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            logFailedDevice(opTask, masterId);
        }
    }

    checkAllFinished();
}

void SetUIRefreshTimeTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;
    forceFinish();
}

void SetUIRefreshTimeTask::onTimeout()
{
    qWarning() << "[Scheduler][SetUIRefreshTimeTask] 超时，剩余" << m_pendingMap.size() << "台设备未响应";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][SetUIRefreshTimeTask] 超时，剩余 %1 台设备未响应")
            .arg(m_pendingMap.size()).toStdString());
    forceFinish();
}

void SetUIRefreshTimeTask::forceFinish()
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
            const QString desc = QString("SetUIRefreshTime logo=%1s total=%2s switch=%3s task completed: %4 devices succeeded")
                  .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec).arg(m_successCount);
            opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
        } else {
            const QString desc = QString("SetUIRefreshTime logo=%1s total=%2s switch=%3s task finished: %4 succeeded, %5 failed")
                  .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec)
                  .arg(m_successCount).arg(m_failedQrCodes.count());
            opTaskEnd->log(OperationDispatchTask::MsgType::Error, desc, 0);
        }
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes,
                     m_logoSec, m_paramTotalSec, m_paramSwitchSec);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetUIRefreshTimeTask: logo=%1s total=%2s switch=%3s 设置完成（%4 台）")
                            .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec).arg(m_successCount)
                      : QString("SetUIRefreshTimeTask: logo=%1s total=%2s switch=%3s %4 台成功，%5 台失败")
                            .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec)
                            .arg(m_successCount).arg(m_failedQrCodes.count()));

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][SetUIRefreshTimeTask] 任务结束: logo=%1s total=%2s switch=%3s %4 台成功，%5 台失败")
            .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec)
            .arg(m_successCount).arg(m_failedQrCodes.count()).toStdString());
}

void SetUIRefreshTimeTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("SetUIRefreshTime logo=%1s total=%2s switch=%3s task failed: device %4")
        .arg(m_logoSec).arg(m_paramTotalSec).arg(m_paramSwitchSec).arg(qrcode);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetUIRefreshTimeTask::buildPayload() const
{
    const quint16 logo    = static_cast<quint16>(qBound(0, m_logoSec,        0xFFFF));
    const quint16 total   = static_cast<quint16>(qBound(0, m_paramTotalSec,  0xFFFF));
    const quint16 sw      = static_cast<quint16>(qBound(0, m_paramSwitchSec, 0xFFFF));

    QByteArray bytes(kPayloadBytes, 0);
    bytes[0] = static_cast<char>((logo  >> 8) & 0xFF);
    bytes[1] = static_cast<char>(logo        & 0xFF);
    bytes[2] = static_cast<char>((total >> 8) & 0xFF);
    bytes[3] = static_cast<char>(total       & 0xFF);
    bytes[4] = static_cast<char>((sw    >> 8) & 0xFF);
    bytes[5] = static_cast<char>(sw          & 0xFF);
    return bytes;
}

void SetUIRefreshTimeTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}
