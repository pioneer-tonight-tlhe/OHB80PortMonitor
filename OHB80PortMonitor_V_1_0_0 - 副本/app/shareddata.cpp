#include "shareddata.h"
#include "appconfig.h"
#include "ohbdeviceconfig.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/network_status_task/network_status_task.h"
#include "scheduler/tasks/monitor_data_task/monitor_data_task.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.h"
#include "scheduler/tasks/vefc_sensor_monitor_task/vefc_sensor_monitor_task.h"
#include "setofohbinfo.h"
#include <QDebug>
#include <QHash>

QVector<SetOfOHBInfo> SharedData::setOfOHBInfoList;
bool SharedData::s_modbusManagerInitialized = false;
NetworkStatusTask* SharedData::s_networkStatusTask = nullptr;
MonitorDataTask* SharedData::s_monitorDataTask = nullptr;
AlarmDispatchTask* SharedData::s_alarmDispatchTask = nullptr;
OperationDispatchTask* SharedData::s_operationDispatchTask = nullptr;
SH85PeriodicSelfCheckTask2* SharedData::s_sh85PeriodicSelfCheckTask = nullptr;
VEFCSensorMonitorTask* SharedData::s_vefcSensorMonitorTask = nullptr;

SharedData::SharedData() {

    if (setOfOHBInfoList.isEmpty()) {
        setOfOHBInfoList.reserve(20);
        QVector<int> uiIds = {2,3,4,5,6,7,8,9,10,11,12,13,36,37,38,39,40,41,42,43};
        
        // 璇诲彇 OHB 璁惧閰嶇疆锛圦RCode + 缃戠粶淇℃伅鍚堝苟锛?
        QVector<OHBDeviceInfo> devices = AppConfig::getInstance().getOHBDeviceConfig().readDevices();
        // 绱㈠紩
        int index = 0;

        // 璁剧疆绾跨▼姹犱负鏈€澶х嚎绋嬫暟锛岄伩鍏嶆墍鏈?Master 鍏辩敤涓€涓嚎绋嬪鑷翠簨浠跺惊鐜樆濉?
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
                foup.setHasAlarm(false);
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
            
            // 涓€娆℃€ц缃暣涓?Foup 闃熷垪
            setInfo.setFoups(foups);
            setOfOHBInfoList.append(setInfo);
        }
        
        qDebug() << "SharedData initialized" << setOfOHBInfoList.size() << "OHB items from config";

    }
}

QSharedPointer<SetOfOHBInfo> SharedData::getSetOfOHBInfoByUiId(int uiId)
{
    // 閬嶅巻 setOfOHBInfoList 鏌ユ壘鍖归厤鐨?uiId
    for (int i = 0; i < setOfOHBInfoList.size(); ++i) {
        if (setOfOHBInfoList[i].getUiId() == uiId) {
            // 鍒涘缓涓€涓寚鍚戣瀵硅薄鐨勬櫤鑳芥寚閽?
            // 娉ㄦ剰锛氳繖閲岃繑鍥炵殑鏄寚鍚戦潤鎬佸垪琛ㄤ腑瀵硅薄鐨勬寚閽堬紝鐢熷懡鍛ㄦ湡鐢盨haredData绠＄悊
            return QSharedPointer<SetOfOHBInfo>(&setOfOHBInfoList[i], [](SetOfOHBInfo*) {
                // 绌虹殑鍒犻櫎鍣紝鍥犱负瀵硅薄鐢盨haredData绠＄悊锛屼笉闇€瑕佸垹闄?
            });
        }
    }
    
    // 鏈壘鍒版椂杩斿洖绌烘寚閽?
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
    // 閬嶅巻鎵€鏈?SetOfOHBInfo锛屾煡鎵惧尮閰嶇殑 qrCode
    for (int i = 0; i < setOfOHBInfoList.size(); ++i) {
        SetOfOHBInfo& setInfo = setOfOHBInfoList[i];
        QVector<FoupOfOHBInfo>& foups = setInfo.getFoups();
        for (int j = 0; j < foups.size(); ++j) {
            if (foups[j].qrCode() == qrCode) {
                // 杩斿洖鎸囧悜璇?Foup 鐨勬寚閽?
                return &foups[j];
            }
        }
    }
    
    // 鏈壘鍒版椂杩斿洖绌烘寚閽?
    return nullptr;
}

void SharedData::initScheduler()
{
    // 鍚姩璋冨害鍣?
    Scheduler* scheduler = Scheduler::instance();
    scheduler->start();

    // 椤哄簭璇存槑锛氬繀椤诲厛鍒涘缓骞舵彁浜?OperationDispatchTask / AlarmDispatchTask
    // 鍐嶆彁浜?NetworkStatusTask / MonitorDataTask銆?
    // 鍥犱负 NetworkStatusTask::start() 浼氬湪鍚姩闃舵灏遍€氳繃
    // SharedData::getOperationDispatchTask() / getAlarmDispatchTask() 娲惧彂鏃ュ織/鍛婅锛?
    // 鑻ヤ笅娓?dispatcher 灏氭湭鍒涘缓锛屽垯浼氫涪澶辨棩蹇楋紙鎴栧 nullptr 瑙ｅ紩鐢級銆?

    // 鎻愪氦鎿嶄綔璋冨害浠诲姟锛堥暱椹伙紝鍙栦唬鑰?RunningLoggerCollector锛?
    if (!s_operationDispatchTask) {
        s_operationDispatchTask = new OperationDispatchTask();
        QString id = scheduler->submitTask(s_operationDispatchTask);
        qDebug() << "[SharedData] 宸叉彁浜ゆ搷浣滆皟搴︿换鍔? TaskID:" << id;
    }

    // 鎻愪氦璀︽姤璋冨害浠诲姟锛堥暱椹伙紝鍙栦唬鑰?AlarmLogicSystem锛?
    if (!s_alarmDispatchTask) {
        s_alarmDispatchTask = new AlarmDispatchTask();
        QString id = scheduler->submitTask(s_alarmDispatchTask);
        qDebug() << "[SharedData] 宸叉彁浜よ鎶ヨ皟搴︿换鍔? TaskID:" << id;
    }

    // 鎻愪氦缃戠粶鐘舵€佺洃鎺т换鍔★紙闀块┗浠诲姟锛?
    // NetworkStatusTask 鍐呴儴浼氬湪璁惧鍚姩鍓嶅厛鍒涘缓骞剁鐞?InitCheckTask
    if (!s_networkStatusTask) {
        s_networkStatusTask = new NetworkStatusTask();
        scheduler->submitTask(s_networkStatusTask);
        qDebug() << "[SharedData] 宸叉彁浜ょ綉缁滅姸鎬佺洃鎺т换鍔?;
    }

    // 鍒涘缓骞舵彁浜ょ洃鎺ф暟鎹换鍔★紙闀块┗浠诲姟锛?
    if (!s_monitorDataTask) {
        s_monitorDataTask = new MonitorDataTask();
        QString monitorTaskId = scheduler->submitTask(s_monitorDataTask);
        qDebug() << "[SharedData] 宸叉彁浜ょ洃鎺ф暟鎹换鍔? TaskID:" << monitorTaskId;
    }

    // 鍒涘缓骞舵彁浜?SH85 鍛ㄦ湡鑷浠诲姟锛堥暱椹讳换鍔★級
    if (!s_sh85PeriodicSelfCheckTask) {
        s_sh85PeriodicSelfCheckTask = new SH85PeriodicSelfCheckTask2();
        QString id = scheduler->submitTask(s_sh85PeriodicSelfCheckTask);
        qDebug() << "[SharedData] 宸叉彁浜?SH85 鍛ㄦ湡鑷浠诲姟, TaskID:" << id;

        // 搴旂敤閰嶇疆锛氫粠 ohb_device.ini 璇诲彇 [sh85selfchecktask] 娈?
        OHBDeviceConfig &cfg = AppConfig::getInstance().getOHBDeviceConfig();
        const bool enabled  = cfg.readSH85SelfCheckEnabled();
        const int  period_s = cfg.readSH85SelfCheckPeriodSeconds();

        // 鍏堣缃懆鏈燂紙鍗曚綅锛氱锛夛紝鍐嶈缃惎鐢ㄧ姸鎬?
        QMetaObject::invokeMethod(s_sh85PeriodicSelfCheckTask, "setPeriod",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, period_s),
                                  Q_ARG(SH85PeriodicSelfCheckTask2::TimeUnit,
                                        SH85PeriodicSelfCheckTask2::TimeUnit::Second));
        QMetaObject::invokeMethod(s_sh85PeriodicSelfCheckTask, "setEnabled",
                                  Qt::QueuedConnection,
                                  Q_ARG(bool, enabled));
    }

    // // 鍒涘缓骞舵彁浜?VEFC 浼犳劅鍣ㄧ洃鎺т换鍔★紙闀块┗浠诲姟锛?
    // if (!s_vefcSensorMonitorTask) {
    //     s_vefcSensorMonitorTask = new VEFCSensorMonitorTask();
    //     QString id = scheduler->submitTask(s_vefcSensorMonitorTask);
    //     qDebug() << "[SharedData] 宸叉彁浜?VEFC 浼犳劅鍣ㄧ洃鎺т换鍔? TaskID:" << id;
    // }

    qDebug() << "[SharedData] 璋冨害鍣ㄥ凡鍚姩锛屾墍鏈夊父椹讳换鍔″凡鎻愪氦";
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

SH85PeriodicSelfCheckTask2* SharedData::getSH85PeriodicSelfCheckTask()
{
    return s_sh85PeriodicSelfCheckTask;
}

VEFCSensorMonitorTask* SharedData::getVEFCSensorMonitorTask()
{
    return s_vefcSensorMonitorTask;
}

