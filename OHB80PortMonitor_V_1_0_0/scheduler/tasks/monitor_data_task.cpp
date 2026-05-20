#include "monitor_data_task.h"
#include "communicationrecorder.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmaster/modbusconnecter.h"
#include "modbustcpmastermanager/modbustcpmaster/periodiccommandsender.h"
#include "modbustcpmastermanager/modbuscommand/commandresponseparser.h"
#include "app/shareddata.h"
#include "app/applogger.h"
#include "app/alarmtype.h"
#include "loggermanager.h"
#include "classes/setofohbinfo.h"
#include "classes/foupofohbinfo.h"
#include "classes/alarminfo.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "logdatabases/alarmlogdb/alarmlogdbcon.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "scheduler/tasks/alarm_dispatch_task.h"

#include <QDebug>
#include <QDateTime>

MonitorDataTask::MonitorDataTask(QObject *parent)
    : SchedulerTask(parent)
    , m_recorder(new CommunicationRecorder(this))
{
    qDebug() << "=============================MonitorDataTask 调度任务开始=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================MonitorDataTask 调度任务开始=============================");

    // 采集器的 shouldEmit 信号连接到 onCommunicationRecorded 槽函数（写入数据库）
    connect(m_recorder, &CommunicationRecorder::shouldEmit,
            this, &MonitorDataTask::onCommunicationRecorded);
}

MonitorDataTask::~MonitorDataTask()
{
    qDebug() << "=============================MonitorDataTask 调度任务结束=============================";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        "=============================MonitorDataTask 调度任务结束=============================");
}

void MonitorDataTask::start()
{
    setState(Running);
    m_stopped = false;
    m_totalCount = 0;

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
            qWarning() << "[Scheduler][MonitorDataTask] 设备" << id << "的 PeriodicCommandSender 为空，跳过该设备的数据监控";
            LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
                QString("[Scheduler][MonitorDataTask] 设备 %1 的 PeriodicCommandSender 为空，跳过该设备的数据监控").arg(id).toStdString());
            continue;
        }

        // 直接连接 commandCompleted 信号（信号已携带 masterId）
        connect(periodicSender, &PeriodicCommandSender::commandCompleted,
                this, &MonitorDataTask::onCommandCompleted,
                Qt::QueuedConnection);
    }

    qDebug() << "[Scheduler][MonitorDataTask] 启动，监听" << m_totalCount << "个设备的 ReadFoupStatus 响应";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO,
        QString("[Scheduler][MonitorDataTask] 启动，监听 %1 个设备的实时数据").arg(m_totalCount).toStdString());
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
    qDebug() << "[Scheduler][MonitorDataTask] 已停止";
    LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::INFO, "[Scheduler][MonitorDataTask] 已停止");
}

void MonitorDataTask::onCommunicationRecorded(ModbusCommand cmd, const QString& masterId)
{
    // 将发送时刻格式化为可读字符串
    QString sentTimeStr = cmd.sentMs > 0
        ? QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : QStringLiteral("-");

    // exec_status：0=Success / 1=Timeout / 2=Retry / 3=Send Failed
    int execStatus = 3;
    if (cmd.received) {
        execStatus = 0;
    } else if (cmd.timedOut) {
        execStatus = 1;
    } else if (cmd.sendCount > 1) {
        execStatus = 2;
    }

    const int retryCount = qMax(0, cmd.sendCount - 1);

    // description 字段：失败时写入 errorMessage，成功时解析响应字段
    QString description;
    if (execStatus != 0) {
        // 执行失败：写入错误信息
        description = cmd.errorMessage;
    } else {
        // 执行成功：解析响应字段
        QVariantMap parsedData = CommandResponseParser::instance().parse(cmd);
        if (!parsedData.isEmpty()) {
            QStringList parts;
            for (auto it = parsedData.constBegin(); it != parsedData.constEnd(); ++it) {
                parts << QString("%1=%2").arg(it.key(), it.value().toString());
            }
            description = parts.join(", ");
        }
    }

    // ReadIdlePurgeAll 指令不写入数据库，不发送信号给 UI
    if (cmd.id != QStringLiteral("ReadIdlePurgeAll")) {
        // 写入 CommunicateLogDBCon（SQLite 持久化）
        if (LogDB::CommunicateLogDBCon* db = LogDB::DatabaseManager::instance().communicateLogCon()) {
            QString respTimeStr = cmd.responseMs > 0
                ? QDateTime::fromMSecsSinceEpoch(cmd.responseMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
                : QString();

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

        // 转发 communicationCompleted 信号供 UI 更新实时日志
        emit communicationCompleted(cmd, masterId, description);
    }

    // 通讯失败时上报运行日志到 OperationDispatchTask
    if (execStatus != 0) {
        QString currentTime = QDateTime::fromMSecsSinceEpoch(cmd.sentMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        QString statusStr;
        if (execStatus == 1) statusStr = "超时";
        else if (execStatus == 2) statusStr = "重试";
        else if (execStatus == 3) statusStr = "发送失败";
        QString message = QString("设备 %1 指令 %2 %3: %4").arg(masterId, cmd.id, statusStr, description);

        if (OperationDispatchTask* loggerTask = SharedData::getOperationDispatchTask()) {
            loggerTask->logWarn(message);
        }
    }
}

void MonitorDataTask::onCommandCompleted(ModbusCommand cmd, const QString& masterId)
{
    if (m_stopped) return;

    // 提交给采集器进行节流
    if (m_recorder) {
        m_recorder->submitCommand(cmd, masterId);
    }

    // 通过解析表统一分发（仅处理已注册的指令）
    if (!CommandResponseParser::instance().hasParser(cmd.id)) return;
    if (!cmd.received) return;

    QVariantMap data = CommandResponseParser::instance().parse(cmd);
    if (data.isEmpty()) {
        qDebug() << "[Scheduler][MonitorDataTask]" << cmd.id << "响应解析失败";
        return;
    }

    updateFoupInfo(masterId, cmd.id, data);

    qDebug() << "[MonitorDataTask][ReadIdlePurgeAll] rsp: " << cmd.response.rawBytes;
}

void MonitorDataTask::updateFoupInfo(const QString& masterId, const QString& commandId, const QVariantMap& data)
{
    FoupOfOHBInfo* foup = SharedData::getFoupByQRCode(masterId);
    if (!foup) {
        qWarning() << "[Scheduler][MonitorDataTask] 未找到设备" << masterId << "对应的 FoupOfOHBInfo";
        LoggerManager::instance().log(AppLogger::SystemLoggerPath().toStdString(), Level::WARN,
            QString("[Scheduler][MonitorDataTask] 未找到设备 %1 对应的 FoupOfOHBInfo").arg(masterId).toStdString());
        return;
    }

    if (commandId == "ReadFoupStatus") {
        foup->setInletPressure(data.value("inletPressure").toDouble());
        foup->setNegativePressure(data.value("negativePressure").toDouble());
        foup->setInletFlow(data.value("inletFlow").toDouble());
        foup->setRH(data.value("humidity").toDouble());
        foup->setTemperature(data.value("temperature").toDouble());
        foup->setPurgeTimeSec(data.value("purgeTimeSec").toUInt());
        foup->setOldFoupIn(foup->foupIn());
        foup->setFoupIn(data.value("foupIn").toBool());

        // FOUP out → in：设置 startTime 为当前时间
        if (!foup->oldFoupIn() && foup->foupIn()) {
            foup->setStartTime(QTime::currentTime());
        }
        // FOUP in → out：重置相关字段
        else if (foup->oldFoupIn() && !foup->foupIn()) {
            foup->setStartTime(QTime(0, 0, 0));
            foup->setPurgeTimeSec(0);
        }

        // // 将 foup 属性写入 device_param_log 数据库
        // auto* dbCon = LogDB::DatabaseManager::instance().deviceParamLogCon();
        // if (dbCon) {
        //     dbCon->insertRecord(
        //         masterId,
        //         QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"),
        //         foup->inletPressure(),
        //         foup->negativePressure(),   // outlet_pressure 存储 negativePressure
        //         foup->inletFlow(),
        //         foup->RH(),
        //         foup->temperature(),
        //         foup->foupIn() ? 1 : 0);
        // }

        // Process device status alarms
        bool vefcStatus = data.value("vefcStatus").toBool();
        bool tempHumStatus = data.value("tempHumStatus").toBool();
        bool humidityReached = data.value("humidityReached").toBool();

        auto* alarmTask = SharedData::getAlarmDispatchTask();
        if (alarmTask) {
            QString qrCodePrefix = QStringLiteral("[qrcode: %1] ").arg(masterId);

            // VEFC 异常告警
            if (vefcStatus) {
                alarmTask->submitAlarm(
                    static_cast<int>(AlarmType::VEFCAbnormal),
                    static_cast<int>(AlarmSource::Device),
                    masterId,
                    qrCodePrefix + QStringLiteral("VEFC Abnormal (Flow Controller Abnormal). Alarm Code:5001"));
            } else {
                alarmTask->submitResolve(
                    static_cast<int>(AlarmType::VEFCAbnormal),
                    static_cast<int>(AlarmSource::Device),
                    masterId);
            }

            // 温湿度传感器异常告警
            if (tempHumStatus) {
                alarmTask->submitAlarm(
                    static_cast<int>(AlarmType::SH85Abnormal),
                    static_cast<int>(AlarmSource::Device),
                    masterId,
                    qrCodePrefix + QStringLiteral("Temperature and Humidity Sensor Abnormal. Alarm Code:5003"));
            } else {
                alarmTask->submitResolve(
                    static_cast<int>(AlarmType::SH85Abnormal),
                    static_cast<int>(AlarmSource::Device),
                    masterId);
            }

            // 湿度未达标告警
            if (humidityReached) {
                alarmTask->submitAlarm(
                    static_cast<int>(AlarmType::HumidityNotReached),
                    static_cast<int>(AlarmSource::Device),
                    masterId,
                    qrCodePrefix + QStringLiteral("Humidity Not Reached (After 30min Nitrogen Purge). Alarm Code:5101"));
            } else {
                alarmTask->submitResolve(
                    static_cast<int>(AlarmType::HumidityNotReached),
                    static_cast<int>(AlarmSource::Device),
                    masterId);
            }
        }
    } else if (commandId == "ReadIdlePurgeAll") {
        foup->setIdlePurgeEnabled(data.value("idlePurgeEnabled").toBool());
        foup->setIdleState(static_cast<IdleState>(data.value("idleState").toInt()));
        foup->setIdleWorkingTimeSec(static_cast<quint16>(data.value("idleWorkingTimeSec").toUInt()));
        qDebug() << "[MonitorDataTask][ReadIdlePurgeAll] 设备=" << masterId
                 << "idlePurgeEnabled=" << foup->idlePurgeEnabled()
                 << "idleState=" << static_cast<int>(foup->idleState())
                 << "idleWorkingTimeSec=" << foup->idleWorkingTimeSec();
    } else if (commandId == "ReadIdlePurgeEnable") {
        foup->setIdlePurgeEnabled(data.value("idlePurgeEnabled").toBool());
    } else if (commandId == "ReadIdlePurgeStatus") {
        foup->setIdleState(static_cast<IdleState>(data.value("idleState").toInt()));
    } else if (commandId == "ReadIdlePurgeWorkingTime") {
        foup->setIdleWorkingTimeSec(static_cast<quint16>(data.value("idleWorkingTimeSec").toUInt()));
    }

    if (foup->foupIn())
    {
        foup->setIdleWorkingTimeSec(0);
        foup->setIdleState(IdleState::Idle);
    } else{
        // FOUP in → out：重置 purge 相关字段
        foup->setStartTime(QTime(0, 0, 0));
        foup->setPurgeTimeSec(0);
    }

    if (!foup->idlePurgeEnabled())
    {
        foup->setIdleState(IdleState::Idle);
        foup->setIdleWorkingTimeSec(0);
    }
}


