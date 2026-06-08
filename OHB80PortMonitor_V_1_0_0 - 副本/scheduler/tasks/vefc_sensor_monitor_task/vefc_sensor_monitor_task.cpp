#include "vefc_sensor_monitor_task.h"

#include "app/shareddata.h"
#include "classes/foupofohbinfo.h"
#include "config/loggerconfig.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/vefcsensormonitordb/vefcsensormonitordbcon.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "modbustcpmastermanager/modbustcpmaster/modbuscommandsender.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"

#include <QDateTime>
#include <QStringList>

VEFCSensorMonitorTask::VEFCSensorMonitorTask(QObject* parent)
    : SchedulerTask(parent)
    , m_timer(new QTimer(this))
{
    initLogger();
    m_timer->setInterval(kIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &VEFCSensorMonitorTask::onIntervalTick);

    m_logger->summaryLogger().info("[VEFCSensorMonitorTask] 对象创建");
}

VEFCSensorMonitorTask::~VEFCSensorMonitorTask()
{
    if (m_logger) {
        m_logger->summaryLogger().info("[VEFCSensorMonitorTask] 对象销毁");
        delete m_logger;
        m_logger = nullptr;
    }
}

void VEFCSensorMonitorTask::start()
{
    setState(Running);
    m_stopped = false;
    m_roundActive = false;
    m_startedMs = QDateTime::currentMSecsSinceEpoch();
    m_roundIndex = 0;

    connectAllSenders();
    m_timer->start();

    m_logger->summaryLogger().info(QString("============================= VEFCSensorMonitorTask 启动 =============================\n"
                                           "周期: %1 ms\n"
                                           "启动时间: %2\n"
                                           "日志目录: scheduler/vefc_sensor_monitor_task")
        .arg(kIntervalMs)
        .arg(QDateTime::fromMSecsSinceEpoch(m_startedMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
        .toStdString());

    startRound();
}

void VEFCSensorMonitorTask::stop()
{
    m_stopped = true;
    if (m_timer) {
        m_timer->stop();
    }

    disconnectAllSenders();
    m_pendingCommands.clear();
    m_deviceStates.clear();
    m_roundActive = false;

    setState(Cancelled);
    emit finished(false, QStringLiteral("VEFC 传感器监控任务已停止"));

    const qint64 stoppedMs = QDateTime::currentMSecsSinceEpoch();
    m_logger->summaryLogger().info(QString("============================= VEFCSensorMonitorTask 停止 =============================\n"
                                           "运行时长: %1 ms\n"
                                           "停止时间: %2")
        .arg(m_startedMs > 0 ? stoppedMs - m_startedMs : 0)
        .arg(QDateTime::fromMSecsSinceEpoch(stoppedMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
        .toStdString());
}

void VEFCSensorMonitorTask::onIntervalTick()
{
    if (m_stopped) {
        return;
    }

    if (m_roundActive) {
        m_logger->summaryLogger().warn(QString("[VEFCSensorMonitorTask] 上一轮尚未完成，跳过本次触发\n"
                                               "轮次ID: %1\n"
                                               "待完成指令数: %2")
            .arg(m_roundId)
            .arg(m_pendingCommands.size())
            .toStdString());
        return;
    }

    startRound();
}

void VEFCSensorMonitorTask::onCommandFinished(ModbusCommand cmd, const QString& masterId)
{
    Q_UNUSED(masterId)

    if (m_stopped || !m_roundActive) {
        return;
    }
    if (!m_pendingCommands.contains(cmd.uuid)) {
        return;
    }

    const PendingCommand pending = m_pendingCommands.take(cmd.uuid);
    const QString qrCode = pending.qrCode;
    const bool commandOk = cmd.received && !cmd.timedOut && !cmd.checksumError && !cmd.deviceBusy;
    if (!commandOk) {
        failCommand(qrCode, pending.type, cmd, commandFailureReason(cmd));
        finishRoundIfReady();
        return;
    }

    completeCommand(qrCode, pending.type, cmd);
    finishRoundIfReady();
}

void VEFCSensorMonitorTask::onCommandTimeoutRetry(ModbusCommand cmd, const QString& masterId)
{
    Q_UNUSED(masterId)

    if (!m_pendingCommands.contains(cmd.uuid)) {
        return;
    }

    const PendingCommand pending = m_pendingCommands.value(cmd.uuid);
    m_logger->deviceLogger(pending.qrCode).info(QString("[VEFCSensorMonitorTask][QRCode:%1] 指令超时重试\n"
                                                       "轮次ID: %2\n"
                                                       "指令: %3\n"
                                                       "发送次数: %4/%5")
        .arg(pending.qrCode)
        .arg(m_roundId)
        .arg(cmd.id)
        .arg(cmd.sendCount)
        .arg(cmd.maxRetryCount + 1)
        .toStdString());
}

void VEFCSensorMonitorTask::initLogger()
{
    const bool summary = LoggerConfig::getInstance()->isVEFCSensorMonitorTaskSummaryEnabled();
    const bool devices = LoggerConfig::getInstance()->isVEFCSensorMonitorTaskDevicesEnabled();
    m_logger = new VEFCSensorMonitorTaskLogger(summary, devices);
}

void VEFCSensorMonitorTask::connectAllSenders()
{
    disconnectAllSenders();

    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    const QStringList ids = manager.masterIds();
    for (const QString& id : ids) {
        ModbusTcpMaster* master = manager.getMaster(id);
        if (!master || !master->sender()) {
            continue;
        }

        ModbusCommandSender* sender = master->sender();
        m_connections.append(connect(sender, &ModbusCommandSender::commandFinished,
                                     this, &VEFCSensorMonitorTask::onCommandFinished,
                                     Qt::QueuedConnection));
        m_connections.append(connect(sender, &ModbusCommandSender::commandTimeoutRetry,
                                     this, &VEFCSensorMonitorTask::onCommandTimeoutRetry,
                                     Qt::QueuedConnection));
    }
}

void VEFCSensorMonitorTask::disconnectAllSenders()
{
    for (const QMetaObject::Connection& connection : qAsConst(m_connections)) {
        disconnect(connection);
    }
    m_connections.clear();
}

void VEFCSensorMonitorTask::startRound()
{
    if (m_stopped) {
        return;
    }

    ++m_roundIndex;
    m_roundActive = true;
    m_roundTimestamp = QDateTime::currentMSecsSinceEpoch();
    m_roundId = roundIdFromTimestamp(m_roundTimestamp);
    m_pendingCommands.clear();
    m_deviceStates.clear();

    const QStringList qrCodes = SharedData::getAllQrcodes();
    m_logger->summaryLogger().info(QString("============================= VEFC 传感器监控轮次开始 =============================\n"
                                           "轮次ID: %1\n"
                                           "轮次序号: %2\n"
                                           "记录时间戳: %3\n"
                                           "记录时间: %4\n"
                                           "目标设备数: %5")
        .arg(m_roundId)
        .arg(m_roundIndex)
        .arg(m_roundTimestamp)
        .arg(QDateTime::fromMSecsSinceEpoch(m_roundTimestamp).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
        .arg(qrCodes.size())
        .toStdString());

    CommandPool* pool = ModbusTcpMasterManager::instance().commandPool();
    if (!pool || !pool->contains(kReadPressureCmdId) || !pool->contains(kReadTemperatureCmdId)) {
        m_logger->summaryLogger().error(QString("[VEFCSensorMonitorTask] VEFC 传感器监控指令不存在，轮次取消\n"
                                                "轮次ID: %1\n"
                                                "ReadVEFCPressure: %2\n"
                                                "ReadVEFCTemperature: %3")
            .arg(m_roundId)
            .arg(pool && pool->contains(kReadPressureCmdId) ? QStringLiteral("存在") : QStringLiteral("不存在"))
            .arg(pool && pool->contains(kReadTemperatureCmdId) ? QStringLiteral("存在") : QStringLiteral("不存在"))
            .toStdString());
        m_roundActive = false;
        return;
    }

    for (const QString& qrCode : qrCodes) {
        submitDeviceCommands(qrCode);
    }

    finishRoundIfReady();
}

void VEFCSensorMonitorTask::submitDeviceCommands(const QString& qrCode)
{
    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(qrCode);
    if (!foup) {
        skipDevice(qrCode, QStringLiteral("未找到 FoupOfOHBInfo"));
        return;
    }

    DeviceRoundState state;
    state.qrCode = qrCode;
    state.record.qrCode = qrCode;
    state.record.recordTimestamp = m_roundTimestamp;
    state.record.gasPressure = foup->inletPressure();
    state.record.actualFlow = foup->inletFlow();
    m_deviceStates.insert(qrCode, state);

    ModbusTcpMaster* master = ModbusTcpMasterManager::instance().getMaster(qrCode);
    if (!master) {
        skipDevice(qrCode, QStringLiteral("Master 不存在"));
        return;
    }
    if (!master->isConnected()) {
        skipDevice(qrCode, QStringLiteral("设备未连接"));
        return;
    }
    if (!master->sender()) {
        skipDevice(qrCode, QStringLiteral("Sender 为空"));
        return;
    }

    DeviceRoundState& current = m_deviceStates[qrCode];
    if (!submitCommand(qrCode, SensorCommandType::Pressure, kReadPressureCmdId)) {
        current.pressureFinished = true;
        current.pressureOk = false;
        current.failReason = QStringLiteral("ReadVEFCPressure 提交失败");
    }
    if (!submitCommand(qrCode, SensorCommandType::Temperature, kReadTemperatureCmdId)) {
        current.temperatureFinished = true;
        current.temperatureOk = false;
        if (!current.failReason.isEmpty()) {
            current.failReason += QStringLiteral("; ");
        }
        current.failReason += QStringLiteral("ReadVEFCTemperature 提交失败");
    }
}

bool VEFCSensorMonitorTask::submitCommand(const QString& qrCode, SensorCommandType type, const char* commandId)
{
    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    ModbusTcpMaster* master = manager.getMaster(qrCode);
    CommandPool* pool = manager.commandPool();
    if (!master || !master->sender() || !pool || !pool->contains(commandId)) {
        return false;
    }

    ModbusCommand cmd = pool->clone(commandId);
    if (!cmd.isValid()) {
        return false;
    }

    cmd.module = CommandModule::BusinessCommandIssuer;
    m_pendingCommands.insert(cmd.uuid, PendingCommand{qrCode, type});

    ModbusCommandSender* sender = master->sender();
    QMetaObject::invokeMethod(sender, [sender, cmd]() {
        sender->submit(cmd);
    }, Qt::QueuedConnection);

    return true;
}

void VEFCSensorMonitorTask::skipDevice(const QString& qrCode, const QString& reason)
{
    DeviceRoundState& state = m_deviceStates[qrCode];
    state.qrCode = qrCode;
    state.skipped = true;
    state.failReason = reason;
    state.pressureFinished = true;
    state.temperatureFinished = true;

    m_logger->deviceLogger(qrCode).info(QString("[VEFCSensorMonitorTask][QRCode:%1] 跳过本轮记录\n"
                                                "轮次ID: %2\n"
                                                "记录时间: %3\n"
                                                "原因: %4")
        .arg(qrCode)
        .arg(m_roundId)
        .arg(QDateTime::fromMSecsSinceEpoch(m_roundTimestamp).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")))
        .arg(reason)
        .toStdString());
}

void VEFCSensorMonitorTask::completeCommand(const QString& qrCode, SensorCommandType type, const ModbusCommand& cmd)
{
    DeviceRoundState& state = m_deviceStates[qrCode];
    const QVariantMap data = CommandResponseParser::instance().parse(cmd);

    if (type == SensorCommandType::Pressure) {
        state.pressureFinished = true;
        if (data.contains(QStringLiteral("sensorPressure"))) {
            state.pressureOk = true;
            state.record.sensorPressure = data.value(QStringLiteral("sensorPressure")).toDouble();
        } else {
            state.pressureOk = false;
            state.failReason = QStringLiteral("ReadVEFCPressure 解析失败");
        }
    } else {
        state.temperatureFinished = true;
        if (data.contains(QStringLiteral("sensorTemperature"))) {
            state.temperatureOk = true;
            state.record.sensorTemperature = data.value(QStringLiteral("sensorTemperature")).toDouble();
        } else {
            state.temperatureOk = false;
            if (!state.failReason.isEmpty()) {
                state.failReason += QStringLiteral("; ");
            }
            state.failReason += QStringLiteral("ReadVEFCTemperature 解析失败");
        }
    }

    m_logger->deviceLogger(qrCode).info(QString("[VEFCSensorMonitorTask][QRCode:%1] 指令完成\n"
                                               "轮次ID: %2\n"
                                               "指令: %3\n"
                                               "请求帧: %4\n"
                                               "响应帧: %5\n"
                                               "处理结果: 成功")
        .arg(qrCode)
        .arg(m_roundId)
        .arg(cmd.id)
        .arg(commandRequestFrame(cmd))
        .arg(commandResponseFrame(cmd))
        .toStdString());
}

void VEFCSensorMonitorTask::failCommand(const QString& qrCode,
                                        SensorCommandType type,
                                        const ModbusCommand& cmd,
                                        const QString& reason)
{
    DeviceRoundState& state = m_deviceStates[qrCode];
    if (type == SensorCommandType::Pressure) {
        state.pressureFinished = true;
        state.pressureOk = false;
    } else {
        state.temperatureFinished = true;
        state.temperatureOk = false;
    }

    if (!state.failReason.isEmpty()) {
        state.failReason += QStringLiteral("; ");
    }
    state.failReason += QStringLiteral("%1: %2").arg(cmd.id, reason);

    m_logger->deviceLogger(qrCode).warn(QString("[VEFCSensorMonitorTask][QRCode:%1] 指令完成\n"
                                               "轮次ID: %2\n"
                                               "指令: %3\n"
                                               "请求帧: %4\n"
                                               "响应帧: %5\n"
                                               "处理结果: 失败\n"
                                               "原因: %6")
        .arg(qrCode)
        .arg(m_roundId)
        .arg(cmd.id)
        .arg(commandRequestFrame(cmd))
        .arg(commandResponseFrame(cmd))
        .arg(reason)
        .toStdString());
}

void VEFCSensorMonitorTask::finishRoundIfReady()
{
    if (!m_roundActive || !m_pendingCommands.isEmpty()) {
        return;
    }

    QVector<VEFCSensorMonitorRecord> records;
    QStringList failedDevices;
    QStringList skippedDevices;
    records.reserve(m_deviceStates.size());

    for (auto it = m_deviceStates.begin(); it != m_deviceStates.end(); ++it) {
        const DeviceRoundState& state = it.value();
        if (state.skipped) {
            skippedDevices << state.qrCode;
            continue;
        }
        // 数据库记录必须同时包含全局采集值和两条实时传感器指令结果。
        // 任一指令失败、超时或解析失败时，本轮该设备只进入失败摘要，不写入数据库。
        if (state.pressureOk && state.temperatureOk) {
            records.append(state.record);
            m_logger->deviceLogger(state.qrCode).info(QString("[VEFCSensorMonitorTask][QRCode:%1] 生成监控记录\n"
                                                              "轮次ID: %2\n"
                                                              "记录时间: %3\n"
                                                              "气体压力: %4\n"
                                                              "实际流量: %5\n"
                                                              "传感器压力: %6\n"
                                                              "传感器温度: %7")
                .arg(state.qrCode)
                .arg(m_roundId)
                .arg(state.record.recordTimeString())
                .arg(state.record.gasPressure)
                .arg(state.record.actualFlow)
                .arg(state.record.sensorPressure)
                .arg(state.record.sensorTemperature)
                .toStdString());
        } else {
            failedDevices << QStringLiteral("%1(%2)").arg(state.qrCode, state.failReason);
        }
    }

    if (!records.isEmpty()) {
        if (LogDB::VEFCSensorMonitorDBCon* db = LogDB::DatabaseManager::instance().vefcSensorMonitorCon()) {
            db->insertRecords(records);
        } else {
            m_logger->summaryLogger().error(QString("[VEFCSensorMonitorTask] 数据库连接不可用，丢弃本轮记录\n"
                                                    "轮次ID: %1\n"
                                                    "记录数: %2")
                .arg(m_roundId)
                .arg(records.size())
                .toStdString());
        }
    }

    m_logger->summaryLogger().info(QString("============================= VEFC 传感器监控轮次结束 =============================\n"
                                           "轮次ID: %1\n"
                                           "目标设备数: %2\n"
                                           "写入记录数: %3\n"
                                           "失败设备数: %4\n"
                                           "跳过设备数: %5\n"
                                           "失败设备: %6\n"
                                           "跳过设备: %7")
        .arg(m_roundId)
        .arg(m_deviceStates.size())
        .arg(records.size())
        .arg(failedDevices.size())
        .arg(skippedDevices.size())
        .arg(failedDevices.isEmpty() ? QStringLiteral("无") : failedDevices.join(QStringLiteral(", ")))
        .arg(skippedDevices.isEmpty() ? QStringLiteral("无") : skippedDevices.join(QStringLiteral(", ")))
        .toStdString());

    m_roundActive = false;
    m_pendingCommands.clear();
    m_deviceStates.clear();
}

QString VEFCSensorMonitorTask::commandTypeName(SensorCommandType type) const
{
    return type == SensorCommandType::Pressure
        ? QStringLiteral("传感器压力")
        : QStringLiteral("传感器温度");
}

QString VEFCSensorMonitorTask::commandFailureReason(const ModbusCommand& cmd) const
{
    QStringList reasons;
    if (cmd.timedOut) {
        reasons << QStringLiteral("超时");
    }
    if (cmd.checksumError) {
        reasons << QStringLiteral("校验错误");
    }
    if (cmd.deviceBusy) {
        reasons << QStringLiteral("设备忙");
    }
    if (!cmd.errorMessage.trimmed().isEmpty()) {
        reasons << cmd.errorMessage.trimmed();
    }
    return reasons.isEmpty() ? QStringLiteral("未收到响应") : reasons.join(QStringLiteral(", "));
}

QString VEFCSensorMonitorTask::commandRequestFrame(const ModbusCommand& cmd) const
{
    QByteArray frame = cmd.request.rawBytes;
    frame.append(cmd.request.crc);
    return frame.isEmpty() ? QStringLiteral("无") : QString(frame.toHex(' ').toUpper());
}

QString VEFCSensorMonitorTask::commandResponseFrame(const ModbusCommand& cmd) const
{
    QByteArray frame = cmd.response.rawBytes;
    frame.append(cmd.response.crc);
    const QString frameText = frame.isEmpty() ? QStringLiteral("无") : QString(frame.toHex(' ').toUpper());
    if (cmd.received) {
        return frameText;
    }

    const QString reason = commandFailureReason(cmd);
    return frame.isEmpty() ? reason : QStringLiteral("%1, %2").arg(reason, frameText);
}

QString VEFCSensorMonitorTask::roundIdFromTimestamp(qint64 timestamp) const
{
    return QDateTime::fromMSecsSinceEpoch(timestamp).toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"));
}
