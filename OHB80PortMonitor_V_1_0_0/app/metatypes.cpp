#include "metatypes.h"
#include <QDebug>
#include <QVariantMap>
#include "modbusconnecter.h"
#include "modbuscommand/modbuscommand.h"
#include "modbustcpmastermanager/modbustcpmaster/firmwareupgrader.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"
#include "loggerwidget/pagetable.h"
#include "tasks/sh85_self_check_task.h"
#include "tasks/read_vefc_flow_unit_medium_status_task.h"
#include "logdatabases/dbtypes.h"
#include "alarminfo.h"
#include "alarmrecord.h"
#include "operationrecord.h"
#include "communicaterecord.h"
#include "deviceparamrecord.h"
#include "scheduler/scheduler_task.h"
#include "usermanager/usermanager.h"

void MetaTypes::registerTypes()
{
    // 注册 SchedulerTask::State，用于 stateChanged 跨线程信号
    qRegisterMetaType<SchedulerTask::State>("SchedulerTask::State");

    // 注册连接状态类型
    qRegisterMetaType<ModbusConnecter::ConnectionStatus>("ConnectionStatus");

    // 注册 ModbusCommand 类型，用于 PeriodicCommandSender::commandCompleted 跨线程信号
    qRegisterMetaType<ModbusCommand>("ModbusCommand");

    // 注册 FirmwareUpgrader::UpgradeState，用于 deviceStateLog 等跨线程信号
    qRegisterMetaType<FirmwareUpgrader::UpgradeState>("FirmwareUpgrader::UpgradeState");

    // 注册 Page 类型，用于 LogFileSystem::pageReady 跨线程信号
    qRegisterMetaType<Page>("Page");

    // 注册 SH85SelfChecker::Result，用于 SH85SelfCheckTask::allFinished 跨线程信号
    qRegisterMetaType<SH85SelfChecker::Result>("SH85SelfChecker::Result");

    // 注册 ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus，用于 allFinished 跨线程信号
    qRegisterMetaType<ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus>("ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus");
    qRegisterMetaType<QList<ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus>>("QList<ReadVEFCFlowUnitAndMediumStatusTask::DeviceStatus>");

    // 注册 LogDB::WriteResult，用于 WriteSqlDBCon::taskCompleted 跨线程信号
    qRegisterMetaType<LogDB::WriteResult>("LogDB::WriteResult");
    qRegisterMetaType<QList<QVariantMap>>("QList<QVariantMap>");

    // 注册 4 个数据库表对应的记录类型，以及 AlarmInfo（含 AlarmRecord）
    // 用于 AlarmDispatchTask / OperationDispatchTask 等跨线程信号传递
    qRegisterMetaType<AlarmRecord>("AlarmRecord");
    qRegisterMetaType<OperationRecord>("OperationRecord");
    qRegisterMetaType<CommunicateRecord>("CommunicateRecord");
    qRegisterMetaType<DeviceParamRecord>("DeviceParamRecord");
    qRegisterMetaType<AlarmInfo>("AlarmInfo");
    qRegisterMetaType<QList<AlarmRecord>>("QList<AlarmRecord>");
    qRegisterMetaType<QList<OperationRecord>>("QList<OperationRecord>");
    qRegisterMetaType<QList<CommunicateRecord>>("QList<CommunicateRecord>");
    qRegisterMetaType<QList<DeviceParamRecord>>("QList<DeviceParamRecord>");

    // 注册 UserPermission，用于 permissionChanged / loginSuccess 跨线程信号
    qRegisterMetaType<UserPermission>("UserPermission");

    qDebug() << "All meta types registered successfully";
}
