#include "network_status_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbusconnecter.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "app/shareddata.h"
#include "app/alarmtype.h"
#include "classes/foupofohbinfo.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "usermanager/usermanager.h"

#include <QDateTime>

namespace {
QString statusToString(ModbusConnecter::ConnectionStatus status)
{
    switch (status) {
        case ModbusConnecter::ConnectionStatus::Disconnected: return "已断开";
        case ModbusConnecter::ConnectionStatus::Connecting:   return "连接中";
        case ModbusConnecter::ConnectionStatus::Connected:    return "已连接";
        case ModbusConnecter::ConnectionStatus::Error:        return "错误";
    }
    return "未知";
}

QString masterEndpoint(ModbusTcpMaster* master)
{
    return master ? QString("%1:%2").arg(master->ip()).arg(master->port()) : QStringLiteral("-");
}
} // namespace

NetworkStatusTask::NetworkStatusTask(QObject *parent)
    : SchedulerTask(parent)
{
    m_logger = new NetworkStatusTaskLogger(true, true);
}

NetworkStatusTask::~NetworkStatusTask()
{
    if (m_logger) {
        delete m_logger;
        m_logger = nullptr;
    }
}

void NetworkStatusTask::start()
{
    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_lastStatusMap.clear();
    m_offlineReportedMap.clear();
    m_connections.clear();

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    const QStringList ids = manager.masterIds();
    m_logger->summaryInfo("start", QString("网络状态任务启动，设备总数=%1").arg(ids.size()));

    // 在启动设备前，先创建并启动初始化检查任务
    m_initCheckTask = new InitCheckTask(this);
    connect(m_initCheckTask, &InitCheckTask::allFinished,
            this, &NetworkStatusTask::onInitCheckFinished,
            Qt::QueuedConnection);
    m_initCheckTask->start();
    m_logger->summaryInfo("start", "初始化检查任务已启动");

    // 启动所有 ModbusTcpMaster
    int autoReconnectStartedCount = 0;
    QStringList autoReconnectFailedIds;
    for (const QString &id : ids) {
        ModbusTcpMaster *master = manager.getMaster(id);
        if (manager.startMaster(id, ModbusConnecter::ConnectionMode::AutoReconnect)) {
            ++autoReconnectStartedCount;
        } else {
            autoReconnectFailedIds << QString("%1(%2)").arg(id, masterEndpoint(master));
        }
    }
    m_logger->summaryInfo("start",
        QString("已批量启动自动重连，成功=%1/%2，失败=%3")
            .arg(autoReconnectStartedCount)
            .arg(ids.size())
            .arg(autoReconnectFailedIds.size()));
    if (!autoReconnectFailedIds.isEmpty()) {
        m_logger->summaryWarn("start",
            QString("自动重连启动失败设备=%1").arg(autoReconnectFailedIds.join(",")));
    }

    int initialConnectedCount = 0;
    int initialConnectingCount = 0;
    int initialDisconnectedCount = 0;
    int initialErrorCount = 0;
    for (const QString &id : ids) {
        ModbusTcpMaster *master = manager.getMaster(id);
        if (!master) {
            m_logger->deviceWarn(id, "start", "ModbusTcpMaster 为空，跳过监听");
            continue;
        }

        const QString ipPortStr = masterEndpoint(master);

        ModbusConnecter *connecter = master->connector();
        if (!connecter) {
            m_logger->deviceWarn(id, "start",
                QString("ModbusConnecter 为空，跳过监听，IP端口=%1").arg(ipPortStr));
            continue;
        }

        // 记录初始状态，并立即根据当前状态同步告警
        // （部分设备可能在信号连接之前就已完成连接，需在此初始化，避免信号丢失）
        ModbusConnecter::ConnectionStatus currentStatus = connecter->getStatus();
        m_lastStatusMap[id] = currentStatus;
        m_offlineReportedMap[id] =
            (currentStatus == ModbusConnecter::ConnectionStatus::Disconnected
             || currentStatus == ModbusConnecter::ConnectionStatus::Error);

        switch (currentStatus) {
            case ModbusConnecter::ConnectionStatus::Connected:
                ++initialConnectedCount;
                break;
            case ModbusConnecter::ConnectionStatus::Connecting:
                ++initialConnectingCount;
                break;
            case ModbusConnecter::ConnectionStatus::Disconnected:
                ++initialDisconnectedCount;
                break;
            case ModbusConnecter::ConnectionStatus::Error:
                ++initialErrorCount;
                break;
        }

        // NetworkStatusTask 只负责上报/恢复离线告警，不直接写 foup->hasAlarm/alarmId。
        // 设备最终是否标红由 AlarmDispatchTask 根据所有 active 告警统一判断。
        QString initialConnectedLogContext;
        if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
            const int alarmType = static_cast<int>(AlarmType::DeviceOffline);
            const int alarmSource = static_cast<int>(AlarmSource::Device);
            if (currentStatus == ModbusConnecter::ConnectionStatus::Connected) {
                dispatcher->submitResolve(alarmType, alarmSource, id);
                initialConnectedLogContext =
                    QString("初始状态=已连接，IP端口=%1，离线告警恢复=已提交 alarmType=%2 alarmSource=%3")
                        .arg(ipPortStr)
                        .arg(alarmType)
                        .arg(alarmSource);
            } else if (currentStatus == ModbusConnecter::ConnectionStatus::Disconnected
                       || currentStatus == ModbusConnecter::ConnectionStatus::Error) {
                const QString qrCodePrefix = QStringLiteral("[qrcode: %1] ").arg(id);
                dispatcher->submitAlarm(alarmType, alarmSource, id, qrCodePrefix + QStringLiteral("Device Offline"));
                m_logger->deviceWarn(id, "start",
                    QString("初始状态异常，提交离线告警，状态=%1 alarmType=%2 alarmSource=%3")
                        .arg(statusToString(currentStatus)).arg(alarmType).arg(alarmSource));
            }
        } else {
            m_logger->summaryWarn("start", "AlarmDispatchTask 为空，无法同步初始离线告警状态");
            if (currentStatus == ModbusConnecter::ConnectionStatus::Connected) {
                initialConnectedLogContext =
                    QString("初始状态=已连接，IP端口=%1，离线告警恢复=未提交 AlarmDispatchTask为空")
                        .arg(ipPortStr);
            }
        }

        // 监听连接状态变更信号
        auto conn = connect(connecter, &ModbusConnecter::statusChanged,
                            this, &NetworkStatusTask::onStatusChanged,
                            Qt::QueuedConnection);
        m_connections.append(conn);
        m_totalCount++;

        // 设备在信号挂接前可能已经完成连接（异步连接竞态），此时不会再触发 statusChanged 信号
        // 需要主动触发 WriteQRCode 下发，避免初始已连接设备的指令丢失
        if (currentStatus == ModbusConnecter::ConnectionStatus::Connected) {
            QMetaObject::invokeMethod(this, [this, id, initialConnectedLogContext]() {
                if (m_stopped) return;
                submitWriteQRCode(id, initialConnectedLogContext);
            }, Qt::QueuedConnection);
        }
    }

    if (m_totalCount == 0) {
        setState(Failed);
        emit finished(false, "NetworkStatusTask: 没有可监听的 ModbusConnecter");
        m_logger->summaryWarn("start", "没有可监听的 ModbusConnecter");
        return;
    }

    m_logger->summaryInfo("start",
        QString("初始连接状态汇总：已连接=%1，连接中=%2，已断开=%3，错误=%4")
            .arg(initialConnectedCount)
            .arg(initialConnectingCount)
            .arg(initialDisconnectedCount)
            .arg(initialErrorCount));
    m_logger->summaryInfo("start",
        QString("网络状态任务启动完成，监听设备数=%1").arg(m_totalCount));
    emit progress(0, QString("开始监控 %1 个设备的网络连接状态").arg(m_totalCount));
}

void NetworkStatusTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "网络状态监控任务被取消");
    m_logger->summaryInfo("stop", "网络状态任务已停止");
}

void NetworkStatusTask::onStatusChanged(ModbusConnecter::ConnectionStatus status, const QString &masterId)
{
    if (m_stopped) return;

    // 获取上一次状态
    ModbusConnecter::ConnectionStatus lastStatus = m_lastStatusMap.value(masterId, ModbusConnecter::ConnectionStatus::Disconnected);
    m_lastStatusMap[masterId] = status;

    // 状态未改变则跳过
    if (status == lastStatus) return;

    // 发射状态变更信号供外部使用
    emit statusChanged(status, masterId);

    const bool isOfflineStatus =
        (status == ModbusConnecter::ConnectionStatus::Disconnected
         || status == ModbusConnecter::ConnectionStatus::Error);
    const bool offlineAlreadyReported = m_offlineReportedMap.value(masterId, false);

    // 自动重连期间会反复出现 Error -> Connecting -> Error。
    // 离线会话已经上报后，重复的重连过渡状态不再写设备日志，也不重复提交离线告警。
    if (offlineAlreadyReported
        && (isOfflineStatus || status == ModbusConnecter::ConnectionStatus::Connecting)) {
        return;
    }

    // 获取 IP 和端口信息
    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    ModbusTcpMaster *master = manager.getMaster(masterId);
    const QString ipPortStr = masterEndpoint(master);

    // 当连接断开或出错时，设置告警
    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(masterId);
    if (!foup) {
        m_logger->deviceWarn(masterId, "onStatusChanged",
            QString("未找到对应的 FoupOfOHBInfo，IP端口=%1").arg(ipPortStr));
    }

    if (status == ModbusConnecter::ConnectionStatus::Connected) {
        m_offlineReportedMap[masterId] = false;
        QString connectedLogContext =
            QString("连接状态变化: %1 -> %2，IP端口=%3")
                .arg(statusToString(lastStatus), statusToString(status), ipPortStr);

        // 只恢复 DeviceOffline 告警；如果设备仍有其它 active 告警，AlarmDispatchTask 会继续保持 UI 标红。
        if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
            const int alarmType   = static_cast<int>(AlarmType::DeviceOffline);
            const int alarmSource = static_cast<int>(AlarmSource::Device);
            dispatcher->submitResolve(alarmType, alarmSource, masterId);
            connectedLogContext += QString("，离线告警恢复=已提交 alarmType=%1 alarmSource=%2")
                .arg(alarmType)
                .arg(alarmSource);
        } else {
            connectedLogContext += QStringLiteral("，离线告警恢复=未提交 AlarmDispatchTask为空");
        }

        // 连接成功后下发 WriteQRCode 指令
        submitWriteQRCode(masterId, connectedLogContext);
    } else {
        m_logger->deviceInfo(masterId, "onStatusChanged",
            QString("连接状态变化: %1 -> %2，IP端口=%3")
                .arg(statusToString(lastStatus), statusToString(status), ipPortStr));

        if (foup) {
            foup->setStartTime(QTime(0, 0, 0));
        }

        // 仅在最终离线（Disconnected / Error）状态上报，避免 Connecting 过渡状态误触发；
        // AlarmDispatchTask 内部已对相同 alarmId 做去重，重复提交不会产生冗余记录。
        if (status == ModbusConnecter::ConnectionStatus::Disconnected
            || status == ModbusConnecter::ConnectionStatus::Error) {
            m_offlineReportedMap[masterId] = true;

            if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
                QString qrCodePrefix = QStringLiteral("[qrcode: %1] ").arg(masterId);
                dispatcher->submitAlarm(
                    static_cast<int>(AlarmType::DeviceOffline),
                    static_cast<int>(AlarmSource::Device),
                    masterId,
                    qrCodePrefix + QStringLiteral("Device Offline"));
                m_logger->deviceWarn(masterId, "onStatusChanged",
                    QString("连接异常，提交离线告警，状态=%1 alarmType=%2 alarmSource=%3")
                        .arg(statusToString(status))
                        .arg(static_cast<int>(AlarmType::DeviceOffline))
                        .arg(static_cast<int>(AlarmSource::Device)));
            } else {
                m_logger->deviceWarn(masterId, "onStatusChanged",
                    QString("连接异常，但 AlarmDispatchTask 为空，无法提交离线告警，状态=%1")
                        .arg(statusToString(status)));
            }
        } else {
            m_logger->deviceInfo(masterId, "onStatusChanged",
                QString("连接处于过渡状态，暂不上报离线告警，状态=%1").arg(statusToString(status)));
        }
    }
}

void NetworkStatusTask::onInitCheckFinished(bool allSuccess, int successCount, int failCount,
                                             const QStringList &failedMasterIds)
{
    m_logger->summaryInfo("onInitCheckFinished",
        QString("初始化检查完成，总数=%1，成功=%2，失败=%3，失败设备=%4")
            .arg(successCount + failCount)
            .arg(successCount)
            .arg(failCount)
            .arg(failedMasterIds.join(",")));

    // 释放 InitCheckTask
    if (m_initCheckTask) {
        m_initCheckTask->deleteLater();
        m_initCheckTask = nullptr;
    }

    // 转发汇总信号给外部
    emit allInitFinished(allSuccess, successCount, failCount, failedMasterIds);
}

void NetworkStatusTask::disconnectAll()
{
    const int connectionCount = m_connections.size();
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
    m_logger->summaryInfo("disconnectAll",
        QString("断开所有连接状态监听，连接数=%1").arg(connectionCount));
}

void NetworkStatusTask::submitWriteQRCode(const QString &masterId, const QString &connectedLogContext)
{
    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    ModbusTcpMaster *master = mgr.getMaster(masterId);
    if (!master || !master->sender()) {
        logWriteQRCodeSubmitFailure(masterId, connectedLogContext, "master 或 sender 不可用");
        return;
    }

    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains("WriteQRCode")) {
        logWriteQRCodeSubmitFailure(masterId, connectedLogContext, "指令池缺少 WriteQRCode");
        return;
    }

    ModbusCommand cmd = pool->clone("WriteQRCode");
    if (!cmd.isValid()) {
        logWriteQRCodeSubmitFailure(masterId, connectedLogContext, "指令克隆失败");
        return;
    }

    cmd.module = CommandModule::BusinessCommandIssuer;

    // 将 qrcode 转换为 4 字节数据
    bool ok = false;
    quint32 qrcodeValue = masterId.toUInt(&ok);
    if (!ok) {
        logWriteQRCodeSubmitFailure(masterId, connectedLogContext,
            QString("QRCode 转换失败，rawValue=%1").arg(masterId));
        return;
    }

    // 4 字节大端序：高字节在前
    QByteArray data;
    data.append(static_cast<char>((qrcodeValue >> 24) & 0xFF));
    data.append(static_cast<char>((qrcodeValue >> 16) & 0xFF));
    data.append(static_cast<char>((qrcodeValue >> 8) & 0xFF));
    data.append(static_cast<char>(qrcodeValue & 0xFF));
    // 更新请求寄存器数据
    cmd.request.registerValue = data;
    cmd.request.byteCount     = static_cast<quint8>(data.size());

    // 同步更新 rawBytes 中的数据段（FC 0x10，数据从偏移 7 开始，共 4 字节）
    if (cmd.request.functionCode == 0x10
        && cmd.request.rawBytes.size() >= 7 + 4
        && data.size() == 4) {
        for (int i = 0; i < 4; ++i)
            cmd.request.rawBytes[7 + i] = data[i];
    }

    // 记录待处理指令
    m_writeQRCodePendingMap[cmd.uuid] = masterId;
    m_writeQRCodeContextMap[cmd.uuid] = connectedLogContext;

    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        opTask->logMessage(QString("[WriteQRCode] Device %1 -> QRCode=%2").arg(masterId).arg(qrcodeValue));
    } else {
        m_logger->summaryWarn("submitWriteQRCode", "OperationDispatchTask 为空，无法写入运行日志消息");
    }

    // 连接信号监听响应
    ModbusCommandSender *sender = master->sender();
    auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                        this, &NetworkStatusTask::onWriteQRCodeFinished,
                        Qt::QueuedConnection);
    m_connections.append(conn);

    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);
}

void NetworkStatusTask::logWriteQRCodeSubmitFailure(const QString &masterId,
                                                    const QString &connectedLogContext,
                                                    const QString &reason)
{
    const QString message = connectedLogContext.isEmpty()
        ? QString("WriteQRCode 下发失败，原因=%1").arg(reason)
        : QString("%1，WriteQRCode 下发失败，原因=%2").arg(connectedLogContext, reason);
    m_logger->deviceWarn(masterId, "onConnected", message);
}

void NetworkStatusTask::onWriteQRCodeFinished(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) return;

    // 检查是否是我们关注的 WriteQRCode 指令
    if (!m_writeQRCodePendingMap.contains(cmd.uuid)) return;

    const QString pendingMasterId = m_writeQRCodePendingMap.take(cmd.uuid);
    const QString logMasterId = pendingMasterId.isEmpty() ? masterId : pendingMasterId;
    const QString connectedLogContext = m_writeQRCodeContextMap.take(cmd.uuid);

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
            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, logMasterId,
                             execStatus, retryCount,
                             cmd.request.rawBytes, cmd.response.rawBytes, description,
                             UserPermission::Engineer);
        } else {
            m_logger->deviceWarn(logMasterId, "onWriteQRCodeFinished",
                QString("通讯日志数据库连接为空，未写入 WriteQRCode 记录，uuid=%1").arg(cmd.uuid));
        }
    }

    // 写入运行日志
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString desc = QString("[QRCode: %1]WriteQRCode command %2")
            .arg(logMasterId)
            .arg((cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy) ? "succeeded" : "failed");
        opTask->log((cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy)
                        ? OperationDispatchTask::MsgType::Message
                        : OperationDispatchTask::MsgType::Error,
                    desc, 0);
    }

    const bool ok = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    const QString resultMessage = ok
        ? QString("WriteQRCode=成功 uuid=%1").arg(cmd.uuid)
        : QString("WriteQRCode=失败 uuid=%1 received=%2 timedOut=%3 checksumError=%4 deviceBusy=%5 error=%6")
              .arg(cmd.uuid)
              .arg(cmd.received)
              .arg(cmd.timedOut)
              .arg(cmd.checksumError)
              .arg(cmd.deviceBusy)
              .arg(cmd.errorMessage);
    const QString mergedMessage = connectedLogContext.isEmpty()
        ? resultMessage
        : QString("%1，%2").arg(connectedLogContext, resultMessage);

    if (ok) {
        m_logger->deviceInfo(logMasterId, "onConnected", mergedMessage);
    } else {
        m_logger->deviceWarn(logMasterId, "onConnected", mergedMessage);
    }
}
