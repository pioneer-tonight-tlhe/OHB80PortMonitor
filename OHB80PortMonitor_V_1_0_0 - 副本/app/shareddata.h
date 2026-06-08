#ifndef SHAREDDATA_H
#define SHAREDDATA_H

#include <QVector>
#include <QStringList>
#include <QSharedPointer>
#include <QString>
#include <QVariantMap>
#include "setofohbinfo.h"

class NetworkStatusTask;
class MonitorDataTask;
class AlarmDispatchTask;
class OperationDispatchTask;
class SH85PeriodicSelfCheckTask2;
class VEFCSensorMonitorTask;
class VEFCMonitorInfo;

class SharedData
{
public:
    SharedData();

    static QVector<SetOfOHBInfo> setOfOHBInfoList;

    // 鏍规嵁 uiId 鑾峰彇 SetOfOHBInfo 瀵硅薄
    static QSharedPointer<SetOfOHBInfo> getSetOfOHBInfoByUiId(int uiId);

    // 鏍规嵁 qrCode 鑾峰彇 FoupOfOHBInfo 鎸囬拡锛堢敤浜庣洿鎺ヤ慨鏀癸級
    static FoupOfOHBInfo* getFoupByQRCode(const QString& qrCode);

    // 鏀堕泦鎵€鏈夎澶囩殑 qrCode 鍒楄〃
    static QStringList getAllQrcodes();

    // 鍒濆鍖栬皟搴﹀櫒骞跺惎鍔ㄥ父椹讳换鍔?    static void initScheduler();

    // 鑾峰彇缃戠粶鐘舵€佺洃鎺т换鍔?    static NetworkStatusTask* getNetworkStatusTask();

    // 鑾峰彇鐩戞帶鏁版嵁浠诲姟
    static MonitorDataTask* getMonitorDataTask();

    // 鑾峰彇璀︽姤璋冨害浠诲姟锛堟墍鏈?submitAlarm / submitResolve 鍏ュ彛锛?    static AlarmDispatchTask* getAlarmDispatchTask();

    // 鑾峰彇鎿嶄綔璋冨害浠诲姟锛坙ogMessage / logWarn / logError 鍏ュ彛锛?    static OperationDispatchTask* getOperationDispatchTask();

    // 鑾峰彇 SH85 鍛ㄦ湡鑷浠诲姟锛堝父椹讳换鍔★級
    static SH85PeriodicSelfCheckTask2* getSH85PeriodicSelfCheckTask();

    // 鑾峰彇 VEFC 浼犳劅鍣ㄧ洃鎺т换鍔★紙甯搁┗浠诲姟锛?    static VEFCSensorMonitorTask* getVEFCSensorMonitorTask();

    // 鑾峰彇 VEFC 鐩戞帶淇℃伅锛堟寜 QRCode锛?    static VEFCMonitorInfo* getVEFCMonitorInfo(const QString& qrCode);

private:
    static bool s_modbusManagerInitialized;
    static NetworkStatusTask* s_networkStatusTask;
    static MonitorDataTask* s_monitorDataTask;
    static AlarmDispatchTask* s_alarmDispatchTask;
    static OperationDispatchTask* s_operationDispatchTask;
    static SH85PeriodicSelfCheckTask2* s_sh85PeriodicSelfCheckTask;
    static VEFCSensorMonitorTask* s_vefcSensorMonitorTask;
};

#endif // SHAREDDATA_H

