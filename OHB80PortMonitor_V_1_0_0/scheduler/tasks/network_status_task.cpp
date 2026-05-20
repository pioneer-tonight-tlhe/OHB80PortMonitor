#include "network_status_task.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbusconnecter.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "app/alarmtype.h"
#include "loggermanager.h"
#include "classes/foupofohbinfo.h"
#include "classes/alarminfo.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "scheduler/tasks/alarm_dispatch_task.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "usermanager/usermanager.h"

#include <QDebug>
#include <QDateTime>

NetworkStatusTask::NetworkStatusTask(QObject *parent)
    : SchedulerTask(parent)
{
    qDebug() << "=============================NetworkStatusTask 调度任务开始=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================NetworkStatusTask 调度任务开始=============================");
}

NetworkStatusTask::~NetworkStatusTask()
{
    qDebug() << "=============================NetworkStatusTask 调度任务结束=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================NetworkStatusTask 调度任务结束=============================");
}

void NetworkStatusTask::start()
{
    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_lastStatusMap.clear();
    m_connections.clear();

    // 在启动设备前，先创建并启动初始化检查任务
    m_initCheckTask = new InitCheckTask(this);
    connect(m_initCheckTask, &InitCheckTask::allFinished,
            this, &NetworkStatusTask::onInitCheckFinished,
            Qt::QueuedConnection);
    m_initCheckTask->start();

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    const QStringList ids = manager.masterIds();

    // 启动所有 ModbusTcpMaster
    for (const QString &id : ids) {
        ModbusTcpMaster *master = manager.getMaster(id);
        QString ipPortStr;
        if (master) {
            ipPortStr = QString("%1:%2").arg(master->ip()).arg(master->port());
        }
        manager.startMaster(id, ModbusConnecter::ConnectionMode::AutoReconnect);
        qDebug() << "[Scheduler][NetworkStatusTask] 设备" << id << "(" << ipPortStr << ") 已启动自动重连";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][NetworkStatusTask] 设备 %1 (%2) 已启动自动重连").arg(id, ipPortStr).toStdString());
    }

    for (const QString &id : ids) {
        ModbusTcpMaster *master = manager.getMaster(id);
        if (!master) continue;

        QString ipPortStr = QString("%1:%2").arg(master->ip()).arg(master->port());

        ModbusConnecter *connecter = master->connector();
        if (!connecter) {
            qWarning() << "[Scheduler][NetworkStatusTask] 设备" << id << "(" << ipPortStr << ") 的 ModbusConnecter 为空，跳过";
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][NetworkStatusTask] 设备 %1 (%2) 的 ModbusConnecter 为空，跳过").arg(id, ipPortStr).toStdString());
            continue;
        }

        // 记录初始状态，并立即根据当前状态同步告警
        // （部分设备可能在信号连接之前就已完成连接，需在此初始化，避免信号丢失）
        ModbusConnecter::ConnectionStatus currentStatus = connecter->getStatus();
        m_lastStatusMap[id] = currentStatus;

        FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(id);
        if (foup) {
            if (currentStatus == ModbusConnecter::ConnectionStatus::Connected) {
                foup->setHasAlarm(false);
                foup->setAlarmId("");
                qDebug() << "[Scheduler][NetworkStatusTask] 设备" << id << "(" << ipPortStr << ") 初始状态已连接，告警已清除";
            } else {
                foup->setHasAlarm(true);
                foup->setAlarmId("111");
                qDebug() << "[Scheduler][NetworkStatusTask] 设备" << id << "(" << ipPortStr << ") 初始状态未连接，设置告警";
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
            QMetaObject::invokeMethod(this, [this, id]() {
                if (m_stopped) return;
                submitWriteQRCode(id);
            }, Qt::QueuedConnection);
        }
    }

    if (m_totalCount == 0) {
        setState(Failed);
        emit finished(false, "NetworkStatusTask: 没有可监听的 ModbusConnecter");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][NetworkStatusTask] 没有可监听的 ModbusConnecter");
        return;
    }

    qDebug() << "[Scheduler][NetworkStatusTask] 启动，监听" << m_totalCount << "个设备的网络连接状态";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][NetworkStatusTask] 启动，监听 %1 个设备的网络连接状态").arg(m_totalCount).toStdString());
    emit progress(0, QString("开始监控 %1 个设备的网络连接状态").arg(m_totalCount));
}

void NetworkStatusTask::stop()
{
    m_stopped = true;
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "网络状态监控任务被取消");
    qDebug() << "[Scheduler][NetworkStatusTask] 已停止";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[Scheduler][NetworkStatusTask] 已停止");
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

    QString statusStr;
    switch (status) {
        case ModbusConnecter::ConnectionStatus::Disconnected: statusStr = "已断开"; break;
        case ModbusConnecter::ConnectionStatus::Connecting:   statusStr = "连接中"; break;
        case ModbusConnecter::ConnectionStatus::Connected:    statusStr = "已连接"; break;
        case ModbusConnecter::ConnectionStatus::Error:        statusStr = "错误";   break;
    }

    // 获取 IP 和端口信息
    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    ModbusTcpMaster *master = manager.getMaster(masterId);
    QString ipPortStr;
    if (master) {
        ipPortStr = QString("%1:%2").arg(master->ip()).arg(master->port());
    }

    qDebug() << "[Scheduler][NetworkStatusTask] 设备" << masterId << "(" << ipPortStr << ") 连接状态变更:" << statusStr;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][NetworkStatusTask] 设备 %1 (%2) 连接状态变更: %3").arg(masterId, ipPortStr, statusStr).toStdString());

    // 当连接断开或出错时，设置告警
    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(masterId);
    if (!foup) {
        qWarning() << "[Scheduler][NetworkStatusTask] 未找到设备" << masterId << "(" << ipPortStr << ") 对应的 FoupOfOHBInfo";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][NetworkStatusTask] 未找到设备 %1 (%2) 对应的 FoupOfOHBInfo").arg(masterId, ipPortStr).toStdString());
        return;
    }

    if (status == ModbusConnecter::ConnectionStatus::Connected) {
        // 连接成功：清除告警
        foup->setHasAlarm(false);
        foup->setAlarmId("");
        qDebug() << "[Scheduler][NetworkStatusTask] 设备" << masterId << "(" << ipPortStr << ") 连接成功，告警已清除";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][NetworkStatusTask] 设备 %1 (%2) 连接成功，告警已清除").arg(masterId, ipPortStr).toStdString());

        // 解决设备离线告警
        if (AlarmDispatchTask* dispatcher = SharedData::getAlarmDispatchTask()) {
            const int alarmType   = static_cast<int>(AlarmType::DeviceOffline);
            const int alarmSource = static_cast<int>(AlarmSource::Device);
            dispatcher->submitResolve(alarmType, alarmSource, masterId);
        }

        // 连接成功后下发 WriteQRCode 指令
        submitWriteQRCode(masterId);
    } else {
        // 断开或出错：设置告警
        foup->setHasAlarm(true);
        // 用新 AlarmInfo 规则生成 alarmId（设备类型 = DeviceOffline，来源 = Device + qrCode）
        AlarmInfo probe;
        probe.record.alarmType   = static_cast<int>(AlarmType::DeviceOffline);
        probe.alarmSource        = static_cast<int>(AlarmSource::Device);
        probe.record.qrCode      = foup->qrCode();
        probe.record.alarmLevel  = alarmTypeToLevel(probe.record.alarmType);
        foup->setAlarmId(probe.generateAlarmId());
        foup->setStartTime(QTime(0, 0, 0));
        qDebug() << "[Scheduler][NetworkStatusTask] 设备" << masterId << "(" << ipPortStr << ") 连接异常，已设置告警 alarmId=" << foup->alarmId();
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][NetworkStatusTask] 设备 %1 (%2) 连接异常，已设置告警 alarmId=%3").arg(masterId, ipPortStr, foup->alarmId()).toStdString());

        // 仅在最终离线（Disconnected / Error）状态上报，避免 Connecting 过渡状态误触发；
        // AlarmDispatchTask 内部已对相同 alarmId 做去重，重复提交不会产生冗余记录。
        if (status == ModbusConnecter::ConnectionStatus::Disconnected
            || status == ModbusConnecter::ConnectionStatus::Error) {
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
    qDebug() << "[Scheduler][NetworkStatusTask] InitCheckTask 已完成，释放任务";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][NetworkStatusTask] InitCheckTask 完成: 总=%1, 成功=%2, 失败=%3")
            .arg(successCount + failCount).arg(successCount).arg(failCount).toStdString());

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
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}

void NetworkStatusTask::submitWriteQRCode(const QString &masterId)
{
    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    ModbusTcpMaster *master = mgr.getMaster(masterId);
    if (!master || !master->sender()) {
        qWarning() << "[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：master 不可用 masterId=" << masterId;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：master 不可用 masterId=%1").arg(masterId).toStdString());
        return;
    }

    CommandPool *pool = mgr.commandPool();
    if (!pool || !pool->contains("WriteQRCode")) {
        qWarning() << "[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：指令池缺少 WriteQRCode";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：指令池缺少 WriteQRCode");
        return;
    }

    ModbusCommand cmd = pool->clone("WriteQRCode");
    if (!cmd.isValid()) {
        qWarning() << "[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：指令克隆失败";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：指令克隆失败");
        return;
    }

    cmd.module = CommandModule::BusinessCommandIssuer;

    // 将 qrcode 转换为 4 字节数据
    bool ok = false;
    quint32 qrcodeValue = masterId.toUInt(&ok);
    if (!ok) {
        qWarning() << "[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：qrcode 转换失败 masterId=" << masterId;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][NetworkStatusTask] 下发 WriteQRCode 失败：qrcode 转换失败 masterId=%1").arg(masterId).toStdString());
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

    qDebug() << "[Scheduler][NetworkStatusTask] 下发 WriteQRCode masterId=" << masterId << "value=" << qrcodeValue << "uuid=" << cmd.uuid;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][NetworkStatusTask] 下发 WriteQRCode masterId=%1 value=%2 uuid=%3").arg(masterId).arg(qrcodeValue).arg(cmd.uuid).toStdString());
    SharedData::getOperationDispatchTask()->logMessage(
        QString("[WriteQRCode] Device %1 → QRCode=%2").arg(masterId).arg(qrcodeValue));

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

void NetworkStatusTask::onWriteQRCodeFinished(ModbusCommand cmd, const QString &masterId)
{
    if (m_stopped) return;

    // 检查是否是我们关注的 WriteQRCode 指令
    if (!m_writeQRCodePendingMap.contains(cmd.uuid)) return;

    m_writeQRCodePendingMap.remove(cmd.uuid);

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

    // 写入运行日志
    auto* opTask = SharedData::getOperationDispatchTask();
    if (opTask) {
        const QString desc = QString("[QRCode: %1]WriteQRCode command %2")
            .arg(masterId)
            .arg((cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy) ? "succeeded" : "failed");
        opTask->log((cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy)
                        ? OperationDispatchTask::MsgType::Message
                        : OperationDispatchTask::MsgType::Error,
                    desc, 0);
    }

    const bool ok = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;

    if (ok) {
        qDebug() << "[Scheduler][NetworkStatusTask] WriteQRCode 指令成功 masterId=" << masterId << "uuid=" << cmd.uuid;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][NetworkStatusTask] WriteQRCode 指令成功 masterId=%1 uuid=%2").arg(masterId).arg(cmd.uuid).toStdString());
    } else {
        qWarning() << "[Scheduler][NetworkStatusTask] WriteQRCode 指令失败 masterId=" << masterId
                   << "uuid=" << cmd.uuid << "received=" << cmd.received
                   << "timedOut=" << cmd.timedOut << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][NetworkStatusTask] WriteQRCode 指令失败 masterId=%1 uuid=%2 received=%3 timedOut=%4 checksumError=%5 deviceBusy=%6")
                .arg(masterId).arg(cmd.uuid).arg(cmd.received).arg(cmd.timedOut).arg(cmd.checksumError).arg(cmd.deviceBusy).toStdString());
    }
}
