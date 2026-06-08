#include "read_vefc_flow_unit_medium_status_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "usermanager/usermanager.h"

#include <QDateTime>
#include <QDebug>
#include <QMetaType>
#include <QtGlobal>
#include <QVariantMap>

namespace {
constexpr const char *kCmdId          = "ReadVEFCFlowUnitAndMediumStatus";

QString bytesToHexWithCrc(const QByteArray& bytes, const QByteArray& crc)
{
    QStringList hexList;
    for (int i = 0; i < bytes.size(); ++i) {
        hexList << QString("%1")
            .arg(static_cast<quint8>(bytes[i]), 2, 16, QLatin1Char('0'))
            .toUpper();
    }
    if (crc.size() >= 2) {
        hexList << QString("%1")
            .arg(static_cast<quint8>(crc[0]), 2, 16, QLatin1Char('0'))
            .toUpper();
        hexList << QString("%1")
            .arg(static_cast<quint8>(crc[1]), 2, 16, QLatin1Char('0'))
            .toUpper();
    }
    return hexList.isEmpty() ? QStringLiteral("无") : hexList.join(QStringLiteral(" "));
}
} // namespace

ReadVEFCFlowUnitAndMediumStatusTask::ReadVEFCFlowUnitAndMediumStatusTask(
    const QVector<QString> &qrcodes, QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
    , deviceLogger("scheduler/read_vefc_flow_unit_medium_status_task/detail")
{
    qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] create task: qrcodes=" << qrcodes;
}

ReadVEFCFlowUnitAndMediumStatusTask::~ReadVEFCFlowUnitAndMediumStatusTask()
{
    qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] destroy task";
}

void ReadVEFCFlowUnitAndMediumStatusTask::start()
{
    disconnectAll();

    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_completedCount.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_resultMap.clear();
    m_allFinishedEmitted = false;

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    if (m_qrcodes.isEmpty()) {
        const QStringList masterIds = mgr.masterIds();
        m_qrcodes = QVector<QString>(masterIds.begin(), masterIds.end());
    }

    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {});
        emit finished(false, QStringLiteral("ReadVEFCFlowUnitAndMediumStatusTask: 没有找到目标设备"));
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            opTask->log(OperationDispatchTask::MsgType::Warn,
                        QStringLiteral("读取 VEFC 流量单位/介质状态失败: 没有找到目标设备"), 0);
        }
        return;
    }

    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains(kCmdId)) {
        setState(Failed);
        emit allFinished(false, 0, {});
        emit finished(false, QStringLiteral("ReadVEFCFlowUnitAndMediumStatusTask: 指令 '%1' 不存在").arg(kCmdId));
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            opTask->log(OperationDispatchTask::MsgType::Warn,
                        QStringLiteral("读取 VEFC 流量单位/介质状态失败: 指令不存在"), 0);
        }
        return;
    }

    for (const QString &id : m_qrcodes) {
        DeviceStatus st;
        st.qrcode = id;
        st.commFailed = true;
        m_resultMap.insert(id, st);
    }

    if (auto* opTaskStart = SharedData::getOperationDispatchTask()) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QStringLiteral("读取 VEFC 流量单位/介质状态任务开始: %1 台设备").arg(m_qrcodes.size()), 0);
    }

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master) {
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] master not found, skip:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("Master 不存在"));
            continue;
        }
        if (!master->isConnected()) {
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] device disconnected, skip:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("设备未连接"));
            continue;
        }
        ModbusCommandSender *sender = master->sender();
        if (!sender) {
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] sender is null, skip:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("Sender 为空"));
            continue;
        }

        ModbusCommand cmd = pool->clone(kCmdId);
        if (!cmd.isValid()) {
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] clone command failed:" << id;
            writeDeviceSkipLog(id, kCmdId, QStringLiteral("克隆指令失败"));
            continue;
        }

        cmd.module = CommandModule::BusinessCommandIssuer;

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &ReadVEFCFlowUnitAndMediumStatusTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        auto retryConn = connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                 this, &ReadVEFCFlowUnitAndMediumStatusTask::onCommandTimeoutRetry,
                                 Qt::QueuedConnection);
        m_connections.append(retryConn);

        m_pendingMap[cmd.uuid] = id;
        ++m_totalCount;

        qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] send to device"
                 << id << kCmdId;

        QMetaObject::invokeMethod(sender, [sender, cmd]() {
            sender->submit(cmd);
        }, Qt::QueuedConnection);
    }

    if (m_totalCount == 0) {
        forceFinish();
        return;
    }
}

void ReadVEFCFlowUnitAndMediumStatusTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, QStringLiteral("ReadVEFCFlowUnitAndMediumStatusTask: 任务已取消"));
}

void ReadVEFCFlowUnitAndMediumStatusTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;
    const QString qrCode = m_pendingMap.take(cmd.uuid);

    {
        const QString sentTimeStr = cmd.sentMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-");
        int execStatus = 3;
        if (cmd.received) execStatus = 0;
        else if (cmd.timedOut) execStatus = 1;
        else if (cmd.sendCount > 1) execStatus = 2;

        const int retryCount = qMax(0, cmd.sendCount - 1);
        QString description;
        if (execStatus != 0) {
            description = cmd.errorMessage;
        } else {
            QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
            if (!parsedData.isEmpty()) {
                QStringList parts;
                for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                    parts << QString("%1=%2").arg(it.key(), it.value().toString());
                }
                description = parts.join(QStringLiteral(", "));
            }
        }
        if (description.isEmpty()) {
            description = QStringLiteral("OK");
        }

        if (LogDB::CommunicateLogDBCon *db = LogDB::DatabaseManager::instance().communicateLogCon()) {
            const QString respTimeStr = cmd.responseMs > 0
                ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                : QString();

            QByteArray requestWithCrc = cmd.request.rawBytes;
            if (cmd.request.crc.size() >= 2) {
                requestWithCrc.append(cmd.request.crc);
            }

            QByteArray responseWithCrc = cmd.response.rawBytes;
            if (cmd.response.crc.size() >= 2) {
                responseWithCrc.append(cmd.response.crc);
            }

            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, qrCode,
                             execStatus, retryCount,
                             requestWithCrc, responseWithCrc, description,
                             UserPermission::Engineer);
        }
    }

    DeviceStatus &st = m_resultMap[qrCode];
    st.qrcode = qrCode;

    const bool commandOk = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;
    if (!commandOk) {
        st.commFailed = true;
        qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] device failed:" << qrCode
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
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
            qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] device result:" << qrCode
                     << "unitRaw=" << hi << "mediumRaw=" << lo
                     << "unitOk=" << st.unitOk << "mediumOk=" << st.mediumOk;
        } else {
            st.commFailed = true;
            qWarning() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] registerValue too short:"
                       << qrCode << reg.size();
        }
    }

    writeDeviceCommandLog(qrCode, cmd, st.allOk());
    checkAllFinished();
}

void ReadVEFCFlowUnitAndMediumStatusTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const QString qrCode = m_pendingMap.value(cmd.uuid);
    const int retryCount = qMax(0, cmd.sendCount - 1);
    const int maxRetry = cmd.maxRetryCount;

    qDebug() << "[Scheduler][ReadVEFCFlowUnitAndMediumStatusTask] command timeout retry:"
             << qrCode << retryCount << "/" << maxRetry;

    emit deviceRetrying(qrCode, retryCount, maxRetry);
}

void ReadVEFCFlowUnitAndMediumStatusTask::checkAllFinished()
{
    const int done = m_completedCount.fetchAndAddOrdered(1) + 1;
    if (done < m_totalCount) return;
    forceFinish();
}

void ReadVEFCFlowUnitAndMediumStatusTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    disconnectAll();

    for (const QString &qr : m_pendingMap.values()) {
        if (m_resultMap.contains(qr)) {
            m_resultMap[qr].commFailed = true;
        }
    }
    m_pendingMap.clear();

    QList<DeviceStatus> results;
    int successCount = 0;
    for (const QString &id : m_qrcodes) {
        if (!m_resultMap.contains(id)) continue;

        const DeviceStatus &st = m_resultMap[id];
        results.append(st);
        if (st.allOk()) {
            ++successCount;
        }
    }

    const bool allSuccess = (successCount == m_qrcodes.size());
    setState(allSuccess ? Finished : Failed);

    emit allFinished(allSuccess, successCount, results);
    emit finished(allSuccess,
                  allSuccess
                      ? QStringLiteral("ReadVEFCFlowUnitAndMediumStatusTask: %1 台全部通过").arg(successCount)
                      : QStringLiteral("ReadVEFCFlowUnitAndMediumStatusTask: %1/%2 台通过")
                            .arg(successCount).arg(m_qrcodes.size()));

    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        if (allSuccess) {
            const QString desc = QStringLiteral("读取 VEFC 流量单位/介质状态任务完成: %1/%2 台通过")
                .arg(successCount).arg(m_qrcodes.size());
            opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
        } else {
            const int failCount = m_qrcodes.size() - successCount;
            const QString desc = QStringLiteral("读取 VEFC 流量单位/介质状态任务结束: %1 台通过, %2 台失败")
                .arg(successCount).arg(failCount);
            opTaskEnd->log(OperationDispatchTask::MsgType::Error, desc, 0);
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

void ReadVEFCFlowUnitAndMediumStatusTask::logFailedDevice(OperationDispatchTask* opTask,
                                                          const QString& id,
                                                          const DeviceStatus& st)
{
    QString reason;
    if (st.commFailed) {
        reason = QStringLiteral("通信失败");
    } else {
        QStringList issues;
        if (!st.unitOk) {
            issues << QStringLiteral("流量单位配置异常(raw=%1)").arg(st.unitRaw);
        }
        if (!st.mediumOk) {
            issues << QStringLiteral("介质配置异常(raw=%1)").arg(st.mediumRaw);
        }
        reason = issues.join(QStringLiteral(", "));
    }

    const QString desc = QStringLiteral("[QRCode:%1]: 读取 VEFC 流量单位/介质状态失败 (%2)")
        .arg(id, reason);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

void ReadVEFCFlowUnitAndMediumStatusTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections)) {
        QObject::disconnect(conn);
    }
    m_connections.clear();
}

void ReadVEFCFlowUnitAndMediumStatusTask::writeDeviceSkipLog(const QString& qrCode,
                                                             const QString& commandId,
                                                             const QString& reason)
{
    deviceDetailLogger().info(
        QString("[ReadVEFCFlowUnitAndMediumStatusTask][QRCode:%1] 跳过下发\n指令: %2\n原因: %3")
            .arg(qrCode)
            .arg(commandId)
            .arg(reason)
            .toStdString());
}

void ReadVEFCFlowUnitAndMediumStatusTask::writeDeviceCommandLog(const QString& qrCode,
                                                                const ModbusCommand& cmd,
                                                                bool success)
{
    const std::string msg = QString("[QRCode:%1] %2")
        .arg(qrCode)
        .arg(commandFrameLogString(cmd))
        .toStdString();
    if (success) {
        deviceDetailLogger().info(msg);
    } else {
        deviceDetailLogger().warn(msg);
    }
}

QString ReadVEFCFlowUnitAndMediumStatusTask::commandFrameLogString(const ModbusCommand& cmd) const
{
    const bool commandOk = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;
    QString responseFrame;
    if (commandOk) {
        responseFrame = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
    } else {
        QStringList failureReasons;
        if (cmd.timedOut) failureReasons << QStringLiteral("超时");
        if (cmd.checksumError) failureReasons << QStringLiteral("校验错误");
        if (cmd.deviceBusy) failureReasons << QStringLiteral("设备忙");
        if (!cmd.errorMessage.isEmpty()) failureReasons << cmd.errorMessage;

        const QString failureText = failureReasons.isEmpty()
            ? QStringLiteral("失败")
            : failureReasons.join(QStringLiteral(", "));
        if (!cmd.received) {
            responseFrame = failureText;
        } else {
            const QString frameText = bytesToHexWithCrc(cmd.response.rawBytes, cmd.response.crc);
            responseFrame = frameText == QStringLiteral("无")
                ? failureText
                : QString("%1, %2").arg(failureText, frameText);
        }
    }

    return QString("[ReadVEFCFlowUnitAndMediumStatusTask] 指令下发完成\n"
                   "指令: %1\n"
                   "请求帧: %2\n"
                   "响应帧: %3")
        .arg(cmd.id)
        .arg(bytesToHexWithCrc(cmd.request.rawBytes, cmd.request.crc))
        .arg(responseFrame);
}

QString ReadVEFCFlowUnitAndMediumStatusTask::subFunctionName() const
{
    return QStringLiteral("read_vefc_flow_unit_medium_status");
}

QString ReadVEFCFlowUnitAndMediumStatusTask::deviceLogPath() const
{
    return QStringLiteral("scheduler/read_vefc_flow_unit_medium_status_task/%1")
        .arg(subFunctionName());
}

ILogger& ReadVEFCFlowUnitAndMediumStatusTask::deviceDetailLogger()
{
    if (!m_loggerInitialized) {
        deviceLogger.set_log_file(deviceLogPath().toStdString());
        m_loggerInitialized = true;
    }
    return deviceLogger;
}
