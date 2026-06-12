#include "network_status_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbusconnecter.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "app/shareddata.h"
#include "app/alarmtype.h"
#include "classes/foupofohbinfo.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "scheduler/tasks/network_status_task/network_status_task_qrcode_logger.h"
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
    m_qrcodeLogger = new QRCodeWriteLogger(true, true);
}

NetworkStatusTask::~NetworkStatusTask()
{
    if (m_logger) {
        delete m_logger;
        m_logger = nullptr;
    }
    if (m_qrcodeLogger) {
        delete m_qrcodeLogger;
        m_qrcodeLogger = nullptr;
    }
}

void NetworkStatusTask::start()
{
    setState(Running);                 // 设置任务状态为运行中
    m_stopped = false;                 // 复位停止标记
    m_totalCount = 0;                  // 重置监听设备计数
    m_lastStatusMap.clear();           // 清空设备上一次连接状态缓存
    m_offlineReportedMap.clear();      // 清空“已上报离线”的标记，避免重复上报
    m_connections.clear();             // 清空已保存的信号连接句柄（本次启动将重新建立）
    m_statusConnCount = 0;
    m_qrConnCount = 0;

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    const QStringList ids = manager.masterIds();
    m_logger->summaryInfo("start", QString("网络状态任务启动，设备总数=%1").arg(ids.size()));

    startInitCheckTask();
    startAutoReconnect(ids);

    int initialConnectedCount = 0;
    int initialConnectingCount = 0;
    int initialDisconnectedCount = 0;
    int initialErrorCount = 0;
    processInitialStatusAndConnectSignals(ids,
                                         initialConnectedCount,
                                         initialConnectingCount,
                                         initialDisconnectedCount,
                                         initialErrorCount);

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

    // 离线类状态标志（Disconnected 或 Error）
    const bool isOfflineStatus =
        (status == ModbusConnecter::ConnectionStatus::Disconnected
         || status == ModbusConnecter::ConnectionStatus::Error);
    // 该设备是否已上报离线会话（用于抑制重连过程中的重复日志/告警）
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
        // 状态变化日志：连接成功 -> info，仅输出“上一次状态->当前状态”
        m_logger->deviceInfo(masterId, "onStatusChanged",
            QString("%1->%2，IP端口=%3").arg(statusToString(lastStatus), statusToString(status), ipPortStr));

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
        // 状态变化日志：非连接成功 -> warn，仅输出“上一次状态->当前状态”
        m_logger->deviceWarn(masterId, "onStatusChanged",
            QString("%1->%2，IP端口=%3").arg(statusToString(lastStatus), statusToString(status), ipPortStr));

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
            }
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
        QString("断开所有信号连接（statusChanged=%1, commandFinished=%2），总数=%3")
            .arg(m_statusConnCount)
            .arg(m_qrConnCount)
            .arg(connectionCount));
    m_statusConnCount = 0;
    m_qrConnCount = 0;
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
    ++m_qrConnCount;

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
    if (m_qrcodeLogger) {
        m_qrcodeLogger->deviceWarn(masterId, "submitWriteQRCode", message);
    }
}

void NetworkStatusTask::onWriteQRCodeFinished(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) return;

    // 检查是否是我们关注的 WriteQRCode 指令
    if (!m_writeQRCodePendingMap.contains(cmd.uuid)) return;

    const QString logMasterId = m_writeQRCodePendingMap.take(cmd.uuid);
    const QString connectedLogContext = m_writeQRCodeContextMap.take(cmd.uuid);

    // 统一判定执行结果
    const bool ok = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    // 写入通讯日志
    auto writeCommunicateLog = [&]() {
        const QString sentTimeStr = cmd.sentMs > 0
            ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-");
        int execStatus = 3;
        if (cmd.received)           execStatus = 0; // 0=成功
        else if (cmd.timedOut)      execStatus = 1; // 1=超时
        else if (cmd.sendCount > 1) execStatus = 2; // 2=发生重试（未收到但未判定超时）
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
        if (description.isEmpty()) description = QStringLiteral("OK");

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
    };

    // 写入运行日志
    auto writeOperationLog = [&]() {
        if (auto* opTask = SharedData::getOperationDispatchTask()) {
            const QString desc = QString("[QRCode: %1]WriteQRCode command %2")
                .arg(logMasterId)
                .arg(ok ? "succeeded" : "failed");
            opTask->log(ok ? OperationDispatchTask::MsgType::Message
                           : OperationDispatchTask::MsgType::Error,
                        desc, 0);
        }
    };

    // 写入专用 QR 日志（不再写入通用设备日志，避免连接上下文以 WARN 级别刷入 devices_warn.log）
    auto writeDeviceLogs = [&]() {
        if (ok) {
            if (m_qrcodeLogger) m_qrcodeLogger->logWriteQRCodeSuccess(logMasterId, cmd);
        } else {
            if (m_qrcodeLogger) m_qrcodeLogger->logWriteQRCodeFailure(logMasterId, cmd);
        }
    };

    // 执行分段日志写入
    writeCommunicateLog();
    writeOperationLog();
    writeDeviceLogs();
}

void NetworkStatusTask::startInitCheckTask()
{
    m_initCheckTask = new InitCheckTask(this);
    connect(m_initCheckTask, &InitCheckTask::allFinished,
            this, &NetworkStatusTask::onInitCheckFinished,
            Qt::QueuedConnection);
    m_initCheckTask->start();
    m_logger->summaryInfo("start", "初始化检查任务已启动");
}

void NetworkStatusTask::startAutoReconnect(const QStringList &ids)
{
    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
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
}

void NetworkStatusTask::processInitialStatusAndConnectSignals(const QStringList &ids,
                                                             int &initialConnectedCount,
                                                             int &initialConnectingCount,
                                                             int &initialDisconnectedCount,
                                                             int &initialErrorCount)
{
    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    for (const QString &id : ids) {
        ModbusTcpMaster *master = manager.getMaster(id);
        if (!master) { 
            m_logger->deviceWarn(id, "start", "ModbusTcpMaster 为空，跳过监听");
            continue;
        }

        const QString ipPortStr = masterEndpoint(master); // 记录 IP:Port 文本，便于日志

        ModbusConnecter *connecter = master->connector(); // 取出连接器对象
        if (!connecter) { // 连接器缺失：无法监听状态变化
            m_logger->deviceWarn(id, "start",
                QString("ModbusConnecter 为空，跳过监听，IP端口=%1").arg(ipPortStr));
            continue;
        }

        ModbusConnecter::ConnectionStatus currentStatus = connecter->getStatus(); // 读取当前连接状态
        m_lastStatusMap[id] = currentStatus; // 初始化“上一次状态”缓存
        m_offlineReportedMap[id] = // 标记初始离线会话为“已上报”，避免后续重复告警
            (currentStatus == ModbusConnecter::ConnectionStatus::Disconnected
             || currentStatus == ModbusConnecter::ConnectionStatus::Error);

        switch (currentStatus) {
            case ModbusConnecter::ConnectionStatus::Connected: // 统计各初始状态数量
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

        QString initialConnectedLogContext; // 连接成功场景的上下文日志（用于拼接 WriteQRCode 结果）
        if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
            const int alarmType = static_cast<int>(AlarmType::DeviceOffline);
            const int alarmSource = static_cast<int>(AlarmSource::Device);
            if (currentStatus == ModbusConnecter::ConnectionStatus::Connected) { // 初始即已连接：提交离线告警恢复
                dispatcher->submitResolve(alarmType, alarmSource, id);
                initialConnectedLogContext =
                    QString("初始状态=已连接，IP端口=%1，离线告警恢复=已提交 alarmType=%2 alarmSource=%3")
                        .arg(ipPortStr)
                        .arg(alarmType)
                        .arg(alarmSource);
            } else if (currentStatus == ModbusConnecter::ConnectionStatus::Disconnected
                       || currentStatus == ModbusConnecter::ConnectionStatus::Error) { // 初始离线/错误：提交离线告警
                const QString qrCodePrefix = QStringLiteral("[qrcode: %1] ").arg(id);
                dispatcher->submitAlarm(alarmType, alarmSource, id, qrCodePrefix + QStringLiteral("Device Offline"));
                m_logger->deviceWarn(id, "start",
                    QString("初始状态异常，提交离线告警，状态=%1 alarmType=%2 alarmSource=%3")
                        .arg(statusToString(currentStatus)).arg(alarmType).arg(alarmSource));
            }
        } else {
            m_logger->summaryWarn("start", "AlarmDispatchTask 为空，无法同步初始离线告警状态"); // 告警分发器缺失，仅记录日志
            if (currentStatus == ModbusConnecter::ConnectionStatus::Connected) {
                initialConnectedLogContext =
                    QString("初始状态=已连接，IP端口=%1，离线告警恢复=未提交 AlarmDispatchTask为空")
                        .arg(ipPortStr);
            }
        }

        auto conn = connect(connecter, &ModbusConnecter::statusChanged, // 挂接连接状态变更信号（Queued）
                            this, &NetworkStatusTask::onStatusChanged,
                            Qt::QueuedConnection);
        m_connections.append(conn); // 保存连接句柄，便于 stop() 统一断开
        ++m_statusConnCount;
        m_totalCount++; // 统计已监听的设备数

        if (currentStatus == ModbusConnecter::ConnectionStatus::Connected) { // 初始即连接：异步下发 WriteQRCode，避免竞态丢失
            QMetaObject::invokeMethod(this, [this, id, initialConnectedLogContext]() {
                if (m_stopped) return;
                submitWriteQRCode(id, initialConnectedLogContext);
            }, Qt::QueuedConnection);
        }
    }
}
