#include "read_vefc_flow_unit_medium_status_task.h"
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
#include <QMetaType>
#include <QTimer>

namespace {
constexpr const char *kCmdId          = "ReadVEFCFlowUnitAndMediumStatus";
constexpr int         kTotalTimeoutMs = 5000;
} // namespace

ReadVEFCFlowUnitAndMediumStatusTask::ReadVEFCFlowUnitAndMediumStatusTask(
    const QVector<QString> &qrcodes, QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
{
    qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 创建任务: qrcodes=" << qrcodes;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 创建任务：设备数=%1")
            .arg(qrcodes.size()).toStdString());
}

ReadVEFCFlowUnitAndMediumStatusTask::~ReadVEFCFlowUnitAndMediumStatusTask()
{
    qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 任务销毁";
}

void ReadVEFCFlowUnitAndMediumStatusTask::start()
{
    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_completedCount.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_resultMap.clear();
    m_allFinishedEmitted = false;

    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {});
        emit finished(false, "ReadVEFCFlowUnitAndMediumStatusTask: qrcode 列表为空");
        // 写入运行日志：失败原因
        auto* opTask = SharedData::getOperationDispatchTask();
        if (opTask) {
            opTask->log(OperationDispatchTask::MsgType::Warn,
                       "ReadVEFCStatus task failed: device identification failed", 0);
        }
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {});
        emit finished(false, QString("ReadVEFCFlowUnitAndMediumStatusTask: 指令 '%1' 不存在").arg(kCmdId));
        // 写入运行日志：失败原因
        auto* opTask = SharedData::getOperationDispatchTask();
        if (opTask) {
            opTask->log(OperationDispatchTask::MsgType::Warn,
                       "ReadVEFCStatus task failed: feature not implemented", 0);
        }
        return;
    }

    // 预初始化每台设备的结果（默认 commFailed=true，待响应再覆盖）
    for (const QString &id : m_qrcodes) {
        DeviceStatus st;
        st.qrcode     = id;
        st.commFailed = true;
        m_resultMap.insert(id, st);
    }

    // 写入运行日志：任务启动
    auto* opTaskStart = SharedData::getOperationDispatchTask();
    if (opTaskStart) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("ReadVEFCStatus task started: %1 devices").arg(m_qrcodes.size()), 0);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master || !master->isConnected() || !master->sender()) {
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备不可用，跳过:" << id;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备 %1 不可用").arg(id).toStdString());
            // commFailed 已经为 true，无需更改
            continue;
        }

        ModbusCommandSender *sender = master->sender();
        ModbusCommand cmd = pool->clone(kCmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 克隆指令失败:" << id;
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &ReadVEFCFlowUnitAndMediumStatusTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        m_pendingMap[cmd.uuid] = id;
        m_totalCount++;

        qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 向设备" << id << "发送 ReadVEFCFlowUnitAndMediumStatus";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 向设备 %1 发送读取指令").arg(id).toStdString());

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        // 没有任何设备能发送指令 → 直接 forceFinish
        forceFinish();
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &ReadVEFCFlowUnitAndMediumStatusTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

void ReadVEFCFlowUnitAndMediumStatusTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "ReadVEFCFlowUnitAndMediumStatusTask: 任务被取消");
}

void ReadVEFCFlowUnitAndMediumStatusTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
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

    DeviceStatus &st = m_resultMap[masterId];
    const bool ok = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    if (!ok) {
        st.commFailed = true;
        qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备" << masterId
                   << "通信失败 timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备 %1 通信失败").arg(masterId).toStdString());
    } else {
        const QByteArray &reg = cmd.response.registerValue;
        if (reg.size() >= 2) {
            const quint8 hi = static_cast<quint8>(reg[0]);
            const quint8 lo = static_cast<quint8>(reg[1]);
            st.commFailed = false;
            st.unitRaw    = hi;
            st.mediumRaw  = lo;
            st.unitOk     = (hi == 0);
            st.mediumOk   = (lo == 0);

            qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备" << masterId
                     << "结果 hi=" << hi << "lo=" << lo
                     << "unitOk=" << st.unitOk << "mediumOk=" << st.mediumOk;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
                QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备 %1 hi=%2 lo=%3 unitOk=%4 mediumOk=%5")
                    .arg(masterId).arg(hi).arg(lo).arg(st.unitOk).arg(st.mediumOk).toStdString());
        } else {
            st.commFailed = true;
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备" << masterId
                       << "响应寄存器长度不足:" << reg.size();
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 设备 %1 响应寄存器长度不足").arg(masterId).toStdString());
        }
    }

    checkAllFinished();
}

void ReadVEFCFlowUnitAndMediumStatusTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;
    forceFinish();
}

void ReadVEFCFlowUnitAndMediumStatusTask::onTimeout()
{
    qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 超时，剩余" << m_pendingMap.size() << "台设备未响应";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 超时，剩余 %1 台设备未响应")
            .arg(m_pendingMap.size()).toStdString());

    // 写入运行日志：超时原因
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        QStringList failedDevices;
        for (const QString &qr : m_pendingMap.values()) {
            failedDevices << qr;
        }
        const QString desc = QString("ReadVEFCStatus task timeout: %1 devices did not respond (%2)")
            .arg(failedDevices.size()).arg(failedDevices.join(", "));
        opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
    }

    forceFinish();
}

void ReadVEFCFlowUnitAndMediumStatusTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();

    // 仍在 pending 的设备全部记为 commFailed
    for (const QString &qr : m_pendingMap.values()) {
        if (m_resultMap.contains(qr)) m_resultMap[qr].commFailed = true;
    }
    m_pendingMap.clear();

    // 按构造顺序输出
    QList<DeviceStatus> results;
    int successCount = 0;
    for (const QString &id : m_qrcodes) {
        if (m_resultMap.contains(id)) {
            const DeviceStatus &st = m_resultMap[id];
            results.append(st);
            if (st.allOk()) ++successCount;
        }
    }

    const bool allSuccess = (successCount == m_qrcodes.size());
    setState(allSuccess ? Finished : Failed);

    emit allFinished(allSuccess, successCount, results);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("ReadVEFCFlowUnitAndMediumStatusTask: %1 台全部通过").arg(successCount)
                      : QString("ReadVEFCFlowUnitAndMediumStatusTask: %1/%2 台通过")
                            .arg(successCount).arg(m_qrcodes.size()));

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] 任务结束: %1/%2 台通过")
            .arg(successCount).arg(m_qrcodes.size()).toStdString());

    // 写入运行日志：任务汇总
    auto* opTaskEnd = SharedData::getOperationDispatchTask();
    if (opTaskEnd) {
        if (allSuccess) {
            const QString desc = QString("ReadVEFCStatus task completed: %1/%2 devices succeeded")
                .arg(successCount).arg(m_qrcodes.size());
            opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
        } else {
            const int failCount = m_qrcodes.size() - successCount;
            const QString desc = QString("ReadVEFCStatus task finished: %1 succeeded, %2 failed")
                .arg(successCount).arg(failCount);
            opTaskEnd->log(OperationDispatchTask::MsgType::Error, desc, 0);
            // 每个失败设备单独写一条日志
            for (const QString &id : m_qrcodes) {
                if (m_resultMap.contains(id)) {
                    const DeviceStatus &st = m_resultMap[id];
                    if (!st.allOk()) {
                        logFailedDevice(opTaskEnd, id, st);
                    }
                }
            }
        }
    }
}

void ReadVEFCFlowUnitAndMediumStatusTask::logFailedDevice(OperationDispatchTask* opTask, const QString& id, const DeviceStatus& st)
{
    QString reason;
    if (st.unitRaw == 0 && st.mediumRaw == 0) {
        reason = "communication failed";
    } else {
        QStringList issues;
        if (!st.unitOk) issues << "unit abnormal";
        if (!st.mediumOk) issues << "medium abnormal";
        reason = issues.join(", ");
    }
    const QString desc = QString("ReadVEFCStatus task failed: device %1 (%2)").arg(id, reason);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

void ReadVEFCFlowUnitAndMediumStatusTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}
