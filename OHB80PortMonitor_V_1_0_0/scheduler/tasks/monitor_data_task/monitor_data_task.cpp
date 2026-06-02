#include "monitor_data_task.h"
#include "communicationrecorder.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/periodiccommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "app/shareddata.h"
#include "app/alarmtype.h"
#include "classes/foupofohbinfo.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"
#include "loggerconfig.h"

#include <QDateTime>
#include <QStringList>

MonitorDataTask::MonitorDataTask(QObject *parent)
    : SchedulerTask(parent)
    , m_recorder(new CommunicationRecorder(this))
{
    // 初始化日志模块
    initMonitorDataTaskLogger();

    m_logger->summaryLogger().info("============================= MonitorDataTask 对象创建 =============================");
    // 采集器的 shouldEmit 信号连接到 onCommunicationRecorded 槽函数（写入数据库）
    connect(m_recorder, &CommunicationRecorder::shouldEmit,
            this, &MonitorDataTask::onCommunicationRecorded);
}

MonitorDataTask::~MonitorDataTask()
{
    m_logger->summaryLogger().info("============================= MonitorDataTask 对象销毁 =============================");
    delete m_logger;
}

void MonitorDataTask::start()
{
    // 初始化任务状态
    setState(Running);
    m_stopped = false;
    m_totalCount = 0;
    m_startedMs = QDateTime::currentMSecsSinceEpoch();
    m_commandTotalCount = 0;
    m_commandSuccessCount = 0;
    m_commandFailedCount = 0;
    m_commandParseFailedCount = 0;
    m_lastAlarmStates.clear();

    // 启动采集器
    if (m_recorder) {
        m_recorder->start();
    }

    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    QStringList ids = manager.masterIds();
    m_totalCount = ids.size();

    for (const QString& id : ids) {
        ModbusTcpMaster* master = manager.getMaster(id);
        if (!master) continue;

        PeriodicCommandSender* periodicSender = master->periodicSender();
        if (!periodicSender) {
            m_logger->summaryLogger().warn("[MonitorDataTask] 设备 {} 的 PeriodicCommandSender 为空，跳过该设备的数据监控", id.toStdString());
            m_logger->deviceLogger(id).warn("[Scheduler][MonitorDataTask] 设备 {} 的 PeriodicCommandSender 为空，跳过该设备的数据监控", id.toStdString());
            continue;
        }

        // 直接连接 commandCompleted 信号（信号已携带 masterId）
        connect(periodicSender, &PeriodicCommandSender::commandCompleted,
                this, &MonitorDataTask::onCommandCompleted,
                Qt::QueuedConnection);
    }

    m_logger->summaryLogger().info(QString("============================= MonitorDataTask 启动 =============================\n"
                                           "监听设备数: %1\n"
                                           "启动时间: %2\n"
                                           "日志目录: scheduler/monitor_data_task")
        .arg(m_totalCount)
        .arg(QDateTime::fromMSecsSinceEpoch(m_startedMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
        .toStdString());
    emit progress(0, QString("开始监控 %1 个设备的实时数据").arg(m_totalCount));
}

void MonitorDataTask::stop()
{
    m_stopped = true;

    // 停止采集器
    if (m_recorder) {
        m_recorder->stop();
    }

    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    QStringList ids = manager.masterIds();
    for (const QString& id : ids) {
        ModbusTcpMaster* master = manager.getMaster(id);
        if (!master) continue;
        PeriodicCommandSender* periodicSender = master->periodicSender();
        if (periodicSender) {
            disconnect(periodicSender, &PeriodicCommandSender::commandCompleted,
                       this, &MonitorDataTask::onCommandCompleted);
        }
    }

    setState(Cancelled);
    emit finished(false, "监控任务被取消");
    const qint64 stoppedMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 runningMs = m_startedMs > 0 ? stoppedMs - m_startedMs : 0;
    m_logger->summaryLogger().info(QString("============================= MonitorDataTask 停止 =============================\n"
                                           "运行时长: %1 ms\n"
                                           "监听设备数: %2\n"
                                           "通讯总数: %3\n"
                                           "成功数: %4\n"
                                           "失败数: %5\n"
                                           "解析失败数: %6\n"
                                           "停止时间: %7")
        .arg(runningMs)
        .arg(m_totalCount)
        .arg(m_commandTotalCount)
        .arg(m_commandSuccessCount)
        .arg(m_commandFailedCount)
        .arg(m_commandParseFailedCount)
        .arg(QDateTime::fromMSecsSinceEpoch(stoppedMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
        .toStdString());
}

void MonitorDataTask::onCommunicationRecorded(ModbusCommand cmd, const QString& masterId)
{
    if (cmd.id == QStringLiteral("ReadIdlePurgeAll")) {
        return;
    }

    QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");

    QString respTimeStr = cmd.responseMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QString();

    // exec_status: 0=Success / 1=Timeout / 2=Send Failed
    int execStatus = 2;
    if (cmd.received) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);

    QString description;
    if (execStatus == 0) {
        QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (!parsedData.isEmpty()) {
            QStringList parts;
            for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                parts << QString("%1=%2").arg(it.key(), it.value().toString());
            }
            description = parts.join(", ");
        }
    } else {
        description = commandFailureReason(cmd);
    }

    if (LogDB::CommunicateLogDBCon* db = LogDB::DatabaseManager::instance().communicateLogCon()) {
        db->insertRecord(
            sentTimeStr,
            respTimeStr,
            cmd.id,
            masterId,
            execStatus,
            retryCount,
            cmd.request.rawBytes,
            cmd.response.rawBytes,
            description);
    }

    emit communicationCompleted(cmd, masterId, description);
}

void MonitorDataTask::onCommandCompleted(ModbusCommand cmd, const QString& masterId)
{
    if (m_stopped) return;

    // 提交给采集器进行节流
    if (m_recorder) {
        m_recorder->submitCommand(cmd, masterId);
    }

    ++m_commandTotalCount;

    // 通过解析表统一分发（仅处理已注册的指令）
    if (!CommandResponseParser::instance().hasParser(cmd.id)) {
        m_logger->deviceLogger(masterId).debug("[Scheduler][MonitorDataTask] 指令 {} 没有对应的解析器，跳过", cmd.id.toStdString());
        return;
    }
    if (!cmd.received) {
        ++m_commandFailedCount;
        logCommandFailure(masterId, cmd);
        return;
    }

    QVariantMap data = CommandResponseParser::instance().parse(cmd);
    if (data.isEmpty()) {
        ++m_commandParseFailedCount;
        logCommandParseFailed(masterId, cmd);
        return;
    }

    ++m_commandSuccessCount;
    logCommandSuccess(masterId, cmd, data);
    updateFoupInfo(masterId, cmd.id, data);
}

void MonitorDataTask::logCommandFailure(const QString& masterId, const ModbusCommand& cmd)
{
    m_logger->deviceLogger(masterId).warn(QString("[MonitorDataTask][QRCode:%1] 指令完成\n"
                                                 "指令: %2\n"
                                                 "请求帧: %3\n"
                                                 "响应帧: %4\n"
                                                 "处理结果: 失败\n"
                                                 "失败原因: %5\n"
                                                 "发送次数: %6/%7")
        .arg(masterId)
        .arg(cmd.id)
        .arg(commandRequestFrame(cmd))
        .arg(commandResponseFrame(cmd))
        .arg(commandFailureReason(cmd))
        .arg(cmd.sendCount)
        .arg(cmd.maxRetryCount + 1)
        .toStdString());
}

void MonitorDataTask::logCommandParseFailed(const QString& masterId, const ModbusCommand& cmd)
{
    m_logger->deviceLogger(masterId).warn(QString("[MonitorDataTask][QRCode:%1] 响应解析失败\n"
                                                 "指令: %2\n"
                                                 "请求帧: %3\n"
                                                 "响应帧: %4\n"
                                                 "处理结果: 解析失败")
        .arg(masterId)
        .arg(cmd.id)
        .arg(commandRequestFrame(cmd))
        .arg(commandResponseFrame(cmd))
        .toStdString());
}

void MonitorDataTask::logCommandSuccess(const QString& masterId, const ModbusCommand& cmd, const QVariantMap& data)
{
    if (!m_logSuccessCommandFrame) {
        return;
    }

    m_logger->deviceLogger(masterId).info(QString("[MonitorDataTask][QRCode:%1] 指令完成\n"
                                                 "指令: %2\n"
                                                 "请求帧: %3\n"
                                                 "响应帧: %4\n"
                                                 "处理结果: 成功\n"
                                                 "解析结果:\n%5")
        .arg(masterId)
        .arg(cmd.id)
        .arg(commandRequestFrame(cmd))
        .arg(commandResponseFrame(cmd))
        .arg(parsedDataText(data))
        .toStdString());
}

QString MonitorDataTask::commandFailureReason(const ModbusCommand& cmd) const
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

QString MonitorDataTask::commandRequestFrame(const ModbusCommand& cmd) const
{
    QByteArray frame = cmd.request.rawBytes;
    frame.append(cmd.request.crc);
    return frame.isEmpty() ? QStringLiteral("无") : QString(frame.toHex(' ').toUpper());
}

QString MonitorDataTask::commandResponseFrame(const ModbusCommand& cmd) const
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

QString MonitorDataTask::parsedDataText(const QVariantMap& data) const
{
    if (data.isEmpty()) {
        return QStringLiteral("  无");
    }

    QStringList lines;
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        lines << QStringLiteral("  %1=%2").arg(it.key(), it.value().toString());
    }
    return lines.join(QStringLiteral("\n"));
}

QString MonitorDataTask::alarmTypeName(AlarmType alarmType) const
{
    switch (alarmType) {
    case AlarmType::VEFCAbnormal:
        return QStringLiteral("VEFCAbnormal");
    case AlarmType::SH85Abnormal:
        return QStringLiteral("SH85Abnormal");
    case AlarmType::HumidityNotReached:
        return QStringLiteral("HumidityNotReached");
    default:
        return QStringLiteral("UnknownAlarm");
    }
}

void MonitorDataTask::initMonitorDataTaskLogger()
{
    // 日志模块是否启用
    bool summary = LoggerConfig::getInstance()->isMonitorDataTaskSummaryEnabled();
    bool devices = LoggerConfig::getInstance()->isMonitorDataTaskDevicesEnabled();
    m_logger = new MonitorDataTaskLogger(summary, devices);
}

void MonitorDataTask::updateFoupInfo(const QString& masterId, const QString& commandId, const QVariantMap& data)
{
    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(masterId);
    if (!foup) {
        m_logger->deviceLogger(masterId).warn("[Scheduler][MonitorDataTask] 未找到设备 {} 对应的 FoupOfOHBInfo", masterId.toStdString());
        return;
    }

    // 根据指令 ID 分发处理
    if (commandId == QStringLiteral("ReadFoupStatus")) {
        handleReadFoupStatus(foup, masterId, data);
    } else if (commandId == QStringLiteral("ReadIdlePurgeAll")) {
        handleReadIdlePurgeAll(foup, data);
    }

    // 通用状态更新逻辑
    normalizeFoupRuntimeState(foup);
}

void MonitorDataTask::handleReadFoupStatus(FoupOfOHBInfo* foup, const QString& masterId, const QVariantMap& data)
{
    // 更新 FOUP 状态数据。
    foup->setInletPressure(data.value("inletPressure").toDouble());
    foup->setNegativePressure(data.value("negativePressure").toDouble());
    foup->setInletFlow(data.value("inletFlow").toDouble());
    foup->setRH(data.value("humidity").toDouble());
    foup->setTemperature(data.value("temperature").toDouble());
    foup->setPurgeTimeSec(data.value("purgeTimeSec").toUInt());
    foup->setOldFoupIn(foup->foupIn());
    foup->setFoupIn(data.value("foupIn").toBool());

    // 更新 FOUP 在位状态（维护开始时间和 purge 计时）
    updateFoupPresenceState(foup, masterId);
    // 报告设备状态告警（VEFC、温湿度、湿度未达标）
    reportDeviceStatusAlarms(masterId, data);
    return;
}

void MonitorDataTask::handleReadIdlePurgeAll(FoupOfOHBInfo* foup, const QVariantMap& data)
{
    foup->setIdlePurgeEnabled(data.value("idlePurgeEnabled").toBool());
    foup->setIdleState(static_cast<IdleState>(data.value("idleState").toInt()));
    foup->setIdleWorkingTimeSec(static_cast<quint16>(data.value("idleWorkingTimeSec").toUInt()));
}

void MonitorDataTask::updateFoupPresenceState(FoupOfOHBInfo* foup, const QString& masterId)
{
    if (!foup) {
        return;
    }

    // FOUP out -> in：记录 purge 开始时间。
    if (!foup->oldFoupIn() && foup->foupIn()) {
        foup->setStartTime(QTime::currentTime());
    }

    // FOUP in -> out：清空 purge 计时字段。
    else if (foup->oldFoupIn() && !foup->foupIn()) {
        foup->setStartTime(QTime(0, 0, 0));
        foup->setPurgeTimeSec(0);
    }
}

void MonitorDataTask::normalizeFoupRuntimeState(FoupOfOHBInfo* foup)
{
    if (!foup) {
        return;
    }

    // FOUP 在位时，重置空闲吹扫相关状态
    if (foup->foupIn()) {
        foup->setIdleWorkingTimeSec(0);
        foup->setIdleState(IdleState::Idle);
    } else {
        // FOUP 不在位时，重置 purge 计时相关字段
        foup->setStartTime(QTime(0, 0, 0));
        foup->setPurgeTimeSec(0);
    }

    // 空闲吹扫功能禁用时，重置空闲吹扫状态
    if (!foup->idlePurgeEnabled()) {
        foup->setIdleState(IdleState::Idle);
        foup->setIdleWorkingTimeSec(0);
    }
}

void MonitorDataTask::reportDeviceStatusAlarms(const QString& masterId, const QVariantMap& data)
{
    AlarmDispatchTask* alarmTask = SharedData::getAlarmDispatchTask();
    if (!alarmTask) {
        return;
    }

    const QString qrCodePrefix = QStringLiteral("[qrcode: %1] ").arg(masterId);
    const bool vefcStatus = data.value("vefcStatus").toBool();
    const bool tempHumStatus = data.value("tempHumStatus").toBool();
    const bool humidityNotReached = data.value("humidityReached").toBool();

    // 报告 VEFC 异常告警
    reportDeviceAlarmState(
        alarmTask,
        vefcStatus,
        AlarmType::VEFCAbnormal,
        AlarmSource::Device,
        masterId,
        qrCodePrefix + QStringLiteral("VEFC Abnormal (Flow Controller Abnormal). Alarm Code:5001"));

    // 报告温湿度传感器异常告警
    reportDeviceAlarmState(
        alarmTask,
        tempHumStatus,
        AlarmType::SH85Abnormal,
        AlarmSource::Device,
        masterId,
        qrCodePrefix + QStringLiteral("Temperature and Humidity Sensor Abnormal. Alarm Code:5003"));

    // 报告湿度未达标告警
    reportDeviceAlarmState(
        alarmTask,
        humidityNotReached,
        AlarmType::HumidityNotReached,
        AlarmSource::Device,
        masterId,
        qrCodePrefix + QStringLiteral("Humidity Not Reached (After 30min Nitrogen Purge). Alarm Code:5101"));
}

void MonitorDataTask::reportDeviceAlarmState(AlarmDispatchTask* alarmTask,
                                             bool active,
                                             AlarmType alarmType,
                                             AlarmSource alarmSource,
                                             const QString& masterId,
                                             const QString& description)
{
    if (!alarmTask) {
        return;
    }

    const int alarmTypeValue = static_cast<int>(alarmType);
    const int alarmSourceValue = static_cast<int>(alarmSource);
    QString alarmSourceText;
    if (alarmSource == AlarmSource::Device)
    {
        alarmSourceText = QString("qrcode: " + masterId);
    }

    const QString stateKey = QStringLiteral("%1|%2|%3").arg(masterId).arg(alarmTypeValue).arg(alarmSourceValue);
    const bool previousActive = m_lastAlarmStates.value(stateKey, false);

    // 根据告警状态上报告警发生或恢复
    if (active) {
        alarmTask->submitAlarm(alarmTypeValue, alarmSourceValue, masterId, description);
    } else {
        alarmTask->submitResolve(alarmTypeValue, alarmSourceValue, masterId);
    }

    if (previousActive == active) {
        return;
    }

    m_lastAlarmStates.insert(stateKey, active);
    if (active) {
        m_logger->deviceLogger(masterId).warn(QString("[MonitorDataTask][QRCode:%1] 上报告警\n"
                                                     "告警类型: %2\n"
                                                     "告警来源: %3\n"
                                                     "说明: %4")
            .arg(masterId)
            .arg(alarmTypeName(alarmType))
            .arg(alarmSourceText)
            .arg(description)
            .toStdString());
    } else {
        m_logger->deviceLogger(masterId).info(QString("[MonitorDataTask][QRCode:%1] 上报告警恢复\n"
                                                     "告警类型: %2\n"
                                                     "告警来源: %3")
            .arg(masterId)
            .arg(alarmTypeName(alarmType))
            .arg(alarmSourceText)
            .toStdString());
    }
}
