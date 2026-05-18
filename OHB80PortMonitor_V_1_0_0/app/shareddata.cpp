#include "shareddata.h"
#include "appconfig.h"
#include "ohbdeviceconfig.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/network_status_task.h"
#include "scheduler/tasks/monitor_data_task.h"
#include "scheduler/tasks/alarm_dispatch_task.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "scheduler/tasks/sh85_periodic_self_check_task.h"
#include "setofohbinfo.h"
#include <QDebug>
#include <QHash>

QVector<SetOfOHBInfo> SharedData::setOfOHBInfoList;
bool SharedData::s_modbusManagerInitialized = false;
NetworkStatusTask* SharedData::s_networkStatusTask = nullptr;
MonitorDataTask* SharedData::s_monitorDataTask = nullptr;
AlarmDispatchTask* SharedData::s_alarmDispatchTask = nullptr;
OperationDispatchTask* SharedData::s_operationDispatchTask = nullptr;
SH85PeriodicSelfCheckTask* SharedData::s_sh85PeriodicSelfCheckTask = nullptr;

SharedData::SharedData() {

    if (setOfOHBInfoList.isEmpty()) {
        setOfOHBInfoList.reserve(20);
        QVector<int> uiIds = {2,3,4,5,6,7,8,9,10,11,12,13,36,37,38,39,40,41,42,43};
        
        // 读取 OHB 设备配置（QRCode + 网络信息合并）
        QVector<OHBDeviceInfo> devices = AppConfig::getInstance().getOHBDeviceConfig().readDevices();
        // 索引
        int index = 0;

        // 设置线程池为最大线程数，避免所有 Master 共用一个线程导致事件循环阻塞
        ModbusTcpMasterManager::instance().setThreadCount(ModbusTcpMasterPool::ThreadCountMode::MaxThreads);

        for (int i = 0; i < 20; ++i) {
            SetOfOHBInfo setInfo;
            setInfo.setUiId(uiIds[i]);
            
            QVector<FoupOfOHBInfo> foups;
            for (int j = 0; j < 4; ++j) {
                FoupOfOHBInfo foup;
                foup.setQrCode(devices.at(index).qrCode);
                foup.setIp(devices.at(index).ip);
                foup.setPort(devices.at(index).port);
                foup.setEnable(devices.at(index).enable);
                index++;
                foup.setInletPressure(0);
                foup.setInletFlow(0);
                foup.setRH(0);
                foup.setFoupIn(false);
                foup.setHasAlarm(true);
                foups.append(foup);

                // static int num = 0;
                // if (!foup.ip().isEmpty() && foup.port() > 0 && num <= 0) {
                if (!foup.ip().isEmpty() && foup.port() > 0) {
                    ModbusTcpMasterManager::instance().addMaster(foup.ip(), foup.port(), foup.qrCode());
                    // num++;
                }
                else {
                    qDebug() << "Invalid IP or port for foup:" << foup.qrCode();
                }
            }
            
            // 一次性设置整个 Foup 队列
            setInfo.setFoups(foups);
            setOfOHBInfoList.append(setInfo);
        }
        
        qDebug() << "SharedData initialized" << setOfOHBInfoList.size() << "OHB items from config";

    }
}

QSharedPointer<SetOfOHBInfo> SharedData::getSetOfOHBInfoByUiId(int uiId)
{
    // 遍历 setOfOHBInfoList 查找匹配的 uiId
    for (int i = 0; i < setOfOHBInfoList.size(); ++i) {
        if (setOfOHBInfoList[i].getUiId() == uiId) {
            // 创建一个指向该对象的智能指针
            // 注意：这里返回的是指向静态列表中对象的指针，生命周期由SharedData管理
            return QSharedPointer<SetOfOHBInfo>(&setOfOHBInfoList[i], [](SetOfOHBInfo*) {
                // 空的删除器，因为对象由SharedData管理，不需要删除
            });
        }
    }
    
    // 未找到时返回空指针
    return QSharedPointer<SetOfOHBInfo>(nullptr);
}

QStringList SharedData::getAllQrcodes()
{
    QStringList qrcodes;
    for (const SetOfOHBInfo& setInfo : setOfOHBInfoList) {
        for (const FoupOfOHBInfo& foup : setInfo.getFoups()) {
            if (!foup.qrCode().isEmpty()) {
                qrcodes << foup.qrCode();
            }
        }
    }
    return qrcodes;
}

FoupOfOHBInfo* SharedData::getFoupByQRCode(const QString& qrCode)
{
    // 遍历所有 SetOfOHBInfo，查找匹配的 qrCode
    for (int i = 0; i < setOfOHBInfoList.size(); ++i) {
        SetOfOHBInfo& setInfo = setOfOHBInfoList[i];
        QVector<FoupOfOHBInfo>& foups = setInfo.getFoups();
        for (int j = 0; j < foups.size(); ++j) {
            if (foups[j].qrCode() == qrCode) {
                // 返回指向该 Foup 的指针
                return &foups[j];
            }
        }
    }
    
    // 未找到时返回空指针
    return nullptr;
}

void SharedData::initScheduler()
{
    // 启动调度器
    Scheduler* scheduler = Scheduler::instance();
    scheduler->start();

    // 顺序说明：必须先创建并提交 OperationDispatchTask / AlarmDispatchTask
    // 再提交 NetworkStatusTask / MonitorDataTask。
    // 因为 NetworkStatusTask::start() 会在启动阶段就通过
    // SharedData::getOperationDispatchTask() / getAlarmDispatchTask() 派发日志/告警，
    // 若下游 dispatcher 尚未创建，则会丢失日志（或对 nullptr 解引用）。

    // 提交操作调度任务（长驻，取代老 RunningLoggerCollector）
    if (!s_operationDispatchTask) {
        s_operationDispatchTask = new OperationDispatchTask();
        QString id = scheduler->submitTask(s_operationDispatchTask);
        qDebug() << "[SharedData] 已提交操作调度任务, TaskID:" << id;
    }

    // 提交警报调度任务（长驻，取代老 AlarmLogicSystem）
    if (!s_alarmDispatchTask) {
        s_alarmDispatchTask = new AlarmDispatchTask();
        QString id = scheduler->submitTask(s_alarmDispatchTask);
        qDebug() << "[SharedData] 已提交警报调度任务, TaskID:" << id;
    }

    // 提交网络状态监控任务（长驻任务）
    // NetworkStatusTask 内部会在设备启动前先创建并管理 InitCheckTask
    if (!s_networkStatusTask) {
        s_networkStatusTask = new NetworkStatusTask();
        scheduler->submitTask(s_networkStatusTask);
        qDebug() << "[SharedData] 已提交网络状态监控任务";
    }

    // 创建并提交监控数据任务（长驻任务）
    if (!s_monitorDataTask) {
        s_monitorDataTask = new MonitorDataTask();
        QString monitorTaskId = scheduler->submitTask(s_monitorDataTask);
        qDebug() << "[SharedData] 已提交监控数据任务, TaskID:" << monitorTaskId;
    }

    // 创建并提交 SH85 周期自检任务（长驻任务）
    if (!s_sh85PeriodicSelfCheckTask) {
        s_sh85PeriodicSelfCheckTask = new SH85PeriodicSelfCheckTask();
        QString id = scheduler->submitTask(s_sh85PeriodicSelfCheckTask);
        qDebug() << "[SharedData] 已提交 SH85 周期自检任务, TaskID:" << id;
    }

    qDebug() << "[SharedData] 调度器已启动，所有常驻任务已提交";
}

NetworkStatusTask* SharedData::getNetworkStatusTask()
{
    return s_networkStatusTask;
}

MonitorDataTask* SharedData::getMonitorDataTask()
{
    return s_monitorDataTask;
}

AlarmDispatchTask* SharedData::getAlarmDispatchTask()
{
    return s_alarmDispatchTask;
}

OperationDispatchTask* SharedData::getOperationDispatchTask()
{
    return s_operationDispatchTask;
}

SH85PeriodicSelfCheckTask* SharedData::getSH85PeriodicSelfCheckTask()
{
    return s_sh85PeriodicSelfCheckTask;
}
