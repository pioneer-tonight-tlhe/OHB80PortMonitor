#include "set_humidity_offset_task.h"
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
#include <cmath>

namespace {
constexpr const char *kCmdThreshold = "WriteHumidityOffsetThreshold";
constexpr const char *kCmdOffset    = "WriteHumidityOffset";
constexpr int kTotalTimeoutMs       = 8000;
} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

SetHumidityOffsetTask::SetHumidityOffsetTask(const QVector<QString> &qrcodes, QObject *parent)
    : SchedulerTask(parent)
    , m_qrcodes(qrcodes)
{
    qDebug() << "[Scheduler][SetHumidityOffsetTask] 创建任务: qrcodes=" << qrcodes;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetHumidityOffsetTask] 创建任务：设备数=%1").arg(qrcodes.size()).toStdString());
}

SetHumidityOffsetTask::~SetHumidityOffsetTask()
{
    qDebug() << "[Scheduler][SetHumidityOffsetTask] 任务销毁";
}

// ============================================================
// Setter
// ============================================================

void SetHumidityOffsetTask::setThreshold(double thresholdPct)
{
    m_thresholdSet = true;
    m_thresholdPct = thresholdPct;
}

void SetHumidityOffsetTask::setOffset(double offsetPct)
{
    m_offsetSet = true;
    m_offsetPct = offsetPct;
}

// ============================================================
// start()
// ============================================================

void SetHumidityOffsetTask::start()
{
    setState(Running);
    m_stopped       = false;
    m_totalDevices  = 0;
    m_completedDevices.storeRelease(0);
    m_pendingMap.clear();
    m_connections.clear();
    m_deviceSuccessCount.clear();
    m_deviceFailed.clear();
    m_successCount  = 0;
    m_failedQrCodes.clear();
    m_allFinishedEmitted = false;

    // 计算每台设备的子指令数
    m_subCmdPerDevice = (m_thresholdSet ? 1 : 0) + (m_offsetSet ? 1 : 0);

    if (m_subCmdPerDevice == 0) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_thresholdSet, m_thresholdPct, m_offsetSet, m_offsetPct);
        emit finished(false, "SetHumidityOffsetTask: 未指定任何待下发指令（threshold/offset 均未设置）");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetHumidityOffsetTask] 未指定任何待下发指令");
        return;
    }

    if (m_qrcodes.isEmpty()) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_thresholdSet, m_thresholdPct, m_offsetSet, m_offsetPct);
        emit finished(false, "SetHumidityOffsetTask: qrcode 列表为空");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetHumidityOffsetTask] qrcode 列表为空");
        return;
    }

    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (!pool
        || (m_thresholdSet && !pool->contains(kCmdThreshold))
        || (m_offsetSet    && !pool->contains(kCmdOffset))) {
        setState(Failed);
        emit allFinished(false, 0, {}, m_thresholdSet, m_thresholdPct, m_offsetSet, m_offsetPct);
        emit finished(false, "SetHumidityOffsetTask: 指令池缺少所需指令");
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            "[Scheduler][SetHumidityOffsetTask] 指令池缺少所需指令");
        return;
    }

    // 百分比 × 100 → 寄存器值
    auto pctToReg = [](double pct) -> quint16 {
        const double scaled = pct * kRegisterScale;
        const quint32 raw   = static_cast<quint32>(std::round(scaled));
        return static_cast<quint16>(qBound<quint32>(0, raw, 0xFFFF));
    };

    const quint16 thresholdReg = m_thresholdSet ? pctToReg(m_thresholdPct) : 0;
    const quint16 offsetReg    = m_offsetSet    ? pctToReg(m_offsetPct)    : 0;
    const QByteArray thresholdBytes = buildRegisterValue(thresholdReg);
    const QByteArray offsetBytes    = buildRegisterValue(offsetReg);

    qDebug() << "[Scheduler][SetHumidityOffsetTask] 子指令配置:"
             << "thresholdSet=" << m_thresholdSet << "value=" << m_thresholdPct << "% reg=" << thresholdReg
             << "offsetSet="    << m_offsetSet    << "value=" << m_offsetPct    << "% reg=" << offsetReg;
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetHumidityOffsetTask] 子指令配置 thresholdSet=%1 (%2%%) offsetSet=%3 (%4%%)")
            .arg(m_thresholdSet).arg(m_thresholdPct).arg(m_offsetSet).arg(m_offsetPct).toStdString());

    // 写入运行日志：任务启动
    auto* opTaskStart = SharedData::getOperationDispatchTask();
    if (opTaskStart) {
        opTaskStart->log(OperationDispatchTask::MsgType::Message,
                         QString("SetHumidityOffset task started: threshold=%1%% offset=%2%% for %3 devices")
                             .arg(m_thresholdPct).arg(m_offsetPct).arg(m_qrcodes.size()), 0);
    }

    auto fillCmd = [](ModbusCommand &cmd, const QByteArray &regBytes) {
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
    };

    for (const QString &id : m_qrcodes) {
        ModbusTcpMaster *master = mgr.getMaster(id);
        if (!master || !master->isConnected() || !master->sender()) {
            qWarning() << "[Scheduler][SetHumidityOffsetTask] 设备不可用，跳过:" << id;
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][SetHumidityOffsetTask] 设备 %1 不可用，跳过").arg(id).toStdString());
            m_failedQrCodes.append(id);
            if (auto* opTask = SharedData::getOperationDispatchTask()) {
                logFailedDevice(opTask, id);
            }
            continue;
        }

        ModbusCommandSender *sender = master->sender();

        auto conn = connect(sender, &ModbusCommandSender::commandFinished,
                            this, &SetHumidityOffsetTask::onCommandFinished,
                            Qt::QueuedConnection);
        m_connections.append(conn);

        m_deviceSuccessCount[id] = 0;
        m_deviceFailed[id] = false;
        m_totalDevices++;

        QVector<ModbusCommand> cmdsToSubmit;
        cmdsToSubmit.reserve(m_subCmdPerDevice);

        bool cloneOk = true;

        if (m_thresholdSet) {
            ModbusCommand cmdT = pool->clone(kCmdThreshold);
            if (!cmdT.isValid()) {
                qWarning() << "[Scheduler][SetHumidityOffsetTask] 克隆 Threshold 指令失败:" << id;
                cloneOk = false;
            } else {
                fillCmd(cmdT, thresholdBytes);
                m_pendingMap[cmdT.uuid] = Pending{id, CmdKind::Threshold};
                cmdsToSubmit.append(cmdT);
            }
        }

        if (cloneOk && m_offsetSet) {
            ModbusCommand cmdO = pool->clone(kCmdOffset);
            if (!cmdO.isValid()) {
                qWarning() << "[Scheduler][SetHumidityOffsetTask] 克隆 Offset 指令失败:" << id;
                cloneOk = false;
            } else {
                fillCmd(cmdO, offsetBytes);
                m_pendingMap[cmdO.uuid] = Pending{id, CmdKind::Offset};
                cmdsToSubmit.append(cmdO);
            }
        }

        if (!cloneOk) {
            // 撤销已加入 pending 的子指令
            for (const ModbusCommand &c : cmdsToSubmit)
                m_pendingMap.remove(c.uuid);
            markDeviceFailed(id);
            continue;
        }

        qDebug() << "[Scheduler][SetHumidityOffsetTask] 向设备" << id
                 << "下发" << cmdsToSubmit.size() << "条子指令";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetHumidityOffsetTask] 向设备 %1 下发 %2 条子指令").arg(id).arg(cmdsToSubmit.size()).toStdString());

        QMetaObject::invokeMethod(sender, [sender, cmdsToSubmit]() {
            for (const ModbusCommand &c : cmdsToSubmit)
                sender->submit(c);
        }, Qt::QueuedConnection);
    }

    if (m_totalDevices == 0) {
        forceFinish();
        return;
    }

    if (!m_timeoutTimer) {
        m_timeoutTimer = new QTimer(this);
        m_timeoutTimer->setSingleShot(true);
        connect(m_timeoutTimer, &QTimer::timeout,
                this, &SetHumidityOffsetTask::onTimeout);
    }
    m_timeoutTimer->start(kTotalTimeoutMs);
}

// ============================================================
// stop()
// ============================================================

void SetHumidityOffsetTask::stop()
{
    m_stopped = true;
    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();
    setState(Cancelled);
    emit finished(false, "SetHumidityOffsetTask: 任务被取消");
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "[Scheduler][SetHumidityOffsetTask] 任务被取消");
}

// ============================================================
// onCommandFinished()
// ============================================================

void SetHumidityOffsetTask::onCommandFinished(ModbusCommand cmd, const QString &masterId)
{
    Q_UNUSED(masterId);
    if (m_stopped) return;
    if (!m_pendingMap.contains(cmd.uuid)) return;

    const Pending p = m_pendingMap.take(cmd.uuid);

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
            db->insertRecord(sentTimeStr, respTimeStr, cmd.id, p.qrcode,
                             execStatus, retryCount,
                             cmd.request.rawBytes, cmd.response.rawBytes, description,
                             UserPermission::Engineer);
        }
    }

    const bool success = cmd.received
                      && !cmd.timedOut
                      && !cmd.checksumError
                      && !cmd.deviceBusy;

    const char *kindStr = (p.kind == CmdKind::Threshold) ? "Threshold" : "Offset";

    if (success) {
        qDebug() << "[Scheduler][SetHumidityOffsetTask] 设备" << p.qrcode
                 << "子指令" << kindStr << "成功";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
            QString("[Scheduler][SetHumidityOffsetTask] 设备 %1 子指令 %2 成功")
                .arg(p.qrcode).arg(kindStr).toStdString());

        ++m_deviceSuccessCount[p.qrcode];
        tryMarkDeviceSuccess(p.qrcode);
    } else {
        qWarning() << "[Scheduler][SetHumidityOffsetTask] 设备" << p.qrcode
                   << "子指令" << kindStr << "失败"
                   << "timedOut=" << cmd.timedOut
                   << "checksumError=" << cmd.checksumError
                   << "deviceBusy=" << cmd.deviceBusy;
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][SetHumidityOffsetTask] 设备 %1 子指令 %2 失败: timedOut=%3 checksumError=%4 deviceBusy=%5")
                .arg(p.qrcode).arg(kindStr).arg(cmd.timedOut).arg(cmd.checksumError).arg(cmd.deviceBusy).toStdString());

        markDeviceFailed(p.qrcode);
    }
}

// ============================================================
// 内部辅助
// ============================================================

void SetHumidityOffsetTask::tryMarkDeviceSuccess(const QString &qrcode)
{
    if (m_deviceFailed.value(qrcode, false)) return;
    if (m_deviceSuccessCount.value(qrcode, 0) < m_subCmdPerDevice) return;

    ++m_successCount;
    qDebug() << "[Scheduler][SetHumidityOffsetTask] 设备" << qrcode << "整体成功";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][SetHumidityOffsetTask] 设备 %1 整体成功").arg(qrcode).toStdString());

    checkAllFinished();
}

void SetHumidityOffsetTask::markDeviceFailed(const QString &qrcode)
{
    if (m_deviceFailed.value(qrcode, false)) return;
    m_deviceFailed[qrcode] = true;
    m_failedQrCodes.append(qrcode);

    qWarning() << "[Scheduler][SetHumidityOffsetTask] 设备" << qrcode << "整体失败";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][SetHumidityOffsetTask] 设备 %1 整体失败").arg(qrcode).toStdString());
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        logFailedDevice(opTask, qrcode);
    }

    checkAllFinished();
}

void SetHumidityOffsetTask::checkAllFinished()
{
    const int done = m_completedDevices.fetchAndAddOrdered(1) + 1;
    if (done < m_totalDevices) return;
    forceFinish();
}

void SetHumidityOffsetTask::onTimeout()
{
    const int remaining = m_pendingMap.size();
    qWarning() << "[Scheduler][SetHumidityOffsetTask] 超时，剩余" << remaining << "条子指令未响应";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
        QString("[Scheduler][SetHumidityOffsetTask] 超时，剩余 %1 条子指令未响应").arg(remaining).toStdString());
    forceFinish();
}

void SetHumidityOffsetTask::forceFinish()
{
    if (m_allFinishedEmitted) return;
    m_allFinishedEmitted = true;

    if (m_timeoutTimer) m_timeoutTimer->stop();
    disconnectAll();

    // 超时仍未响应的设备：补登失败日志
    auto* opTaskPending = SharedData::getOperationDispatchTask();
    for (auto it = m_pendingMap.constBegin(); it != m_pendingMap.constEnd(); ++it) {
        const QString &qr = it.value().qrcode;
        if (!m_deviceFailed.value(qr, false)) {
            m_deviceFailed[qr] = true;
            m_failedQrCodes.append(qr);
            if (opTaskPending) {
                logFailedDevice(opTaskPending, qr);
            }
        }
    }
    m_pendingMap.clear();

    const bool allSuccess = m_failedQrCodes.isEmpty();
    setState(allSuccess ? Finished : Failed);

    // 写入运行日志：任务汇总
    if (auto* opTaskEnd = SharedData::getOperationDispatchTask()) {
        if (allSuccess) {
            const QString desc = QString("SetHumidityOffset task completed: %1 devices succeeded")
                  .arg(m_successCount);
            opTaskEnd->log(OperationDispatchTask::MsgType::Message, desc, 0);
        } else {
            const QString desc = QString("SetHumidityOffset task finished: %1 succeeded, %2 failed")
                  .arg(m_successCount).arg(m_failedQrCodes.count());
            opTaskEnd->log(OperationDispatchTask::MsgType::Error, desc, 0);
        }
    }

    emit allFinished(allSuccess, m_successCount, m_failedQrCodes,
                     m_thresholdSet, m_thresholdPct,
                     m_offsetSet,    m_offsetPct);
    emit finished(allSuccess,
                  allSuccess
                      ? QString("SetHumidityOffsetTask: 设置完成（%1 台）").arg(m_successCount)
                      : QString("SetHumidityOffsetTask: 设置完成，%1 台成功，%2 台失败")
                            .arg(m_successCount).arg(m_failedQrCodes.count()));

    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(),
        allSuccess ? Level::INFO : Level::WARN,
        QString("[Scheduler][SetHumidityOffsetTask] 任务结束: %1 台成功，%2 台失败 thresholdSet=%3 (%4%%) offsetSet=%5 (%6%%)")
            .arg(m_successCount).arg(m_failedQrCodes.count())
            .arg(m_thresholdSet).arg(m_thresholdPct)
            .arg(m_offsetSet).arg(m_offsetPct).toStdString());
}

void SetHumidityOffsetTask::logFailedDevice(OperationDispatchTask* opTask, const QString& qrcode)
{
    const QString desc = QString("SetHumidityOffset task failed: device %1").arg(qrcode);
    opTask->log(OperationDispatchTask::MsgType::Error, desc, 0);
}

QByteArray SetHumidityOffsetTask::buildRegisterValue(quint16 value) const
{
    QByteArray bytes(2, 0);
    bytes[0] = static_cast<char>((value >> 8) & 0xFF);
    bytes[1] = static_cast<char>(value & 0xFF);
    return bytes;
}

void SetHumidityOffsetTask::disconnectAll()
{
    for (const QMetaObject::Connection &conn : qAsConst(m_connections))
        QObject::disconnect(conn);
    m_connections.clear();
}
