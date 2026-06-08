#ifndef ALARMTYPE_H
#define ALARMTYPE_H

#include <QString>
#include <QList>
#include <QPair>


// 警报级别枚举
enum class AlarmLevel : int
{
    Warn  = 1,  // 警告级别，一般不需要解决
    Error = 2,  // 错误级别，需要解决
    Fatal = 3   // 致命错误级别，必须立即解决
};

// 获取警报级别的显示名称
inline QString alarmLevelName(int level)
{
    switch (level) {
        case static_cast<int>(AlarmLevel::Warn):  return QStringLiteral("Warn");
        case static_cast<int>(AlarmLevel::Error): return QStringLiteral("Error");
        case static_cast<int>(AlarmLevel::Fatal): return QStringLiteral("Fatal");
        default: return QString::number(level);
    }
}

// 获取所有警报级别的(名称, 值)列表，便于 UI 填充下拉框
inline QList<QPair<QString, int>> alarmLevelList()
{
    return {
        { QStringLiteral("Warn"),  static_cast<int>(AlarmLevel::Warn)  },
        { QStringLiteral("Error"), static_cast<int>(AlarmLevel::Error) },
        { QStringLiteral("Fatal"), static_cast<int>(AlarmLevel::Fatal) }
    };
}

// 警告类型枚举
enum class AlarmType : int
{
    LoginFailed      = 1001,  // 登录失败，用户可见
    ChangePasswordFailed = 1002,  // 修改密码失败，用户可见
    
    DeviceOffline    = 2001,  // 设备离线，用户可见
    PermissionDenied = 2002,  // 权限拒绝，用户可见
    StorageFull      = 2003,  // 存储空间不足，用户可见
    
    TemperatureSensorAbnormal = 3001,  // 温度传感器异常（数值变为655.35度），用户可见
    HumiditySensorAbnormal = 3002,  // 湿度传感器异常（数值变为655.35度），用户可见
    VEFCUnitConfigFailed = 3003,  // VEFC单位配置失败，用户不可见
    VEFCGasMediumConfigFailed = 3004,  // VEFC气体介质类型配置失败，用户不可见
    VEFCFlowValueSetFailed = 3005,  // VEFC下发流量值设置失败，用户不可见
    
    CommandSendFailed = 4001,  // 指令发送失败，用户可见
    SH85SelfCheckActionFailed = 4100,  // SH85自检动作失败（概括性错误），用户可见
    SH85StartSelfCheckNetworkError = 4101,  // SH85开启功能，网络错误，用户可见
    SH85PreCheckNetworkError = 4102,  // SH85预检测，网络错误，用户可见
    SH85PreCheckNotEnterSelfCheck = 4103,  // SH85预检测未进入自检状态，用户可见
    SH85PreCheckStatusAbnormal = 4104,  // SH85预检测，状态值异常（固件异常），用户不可见
    SH85AcceptanceNetworkError = 4105,  // SH85验收，网络错误，用户可见
    SH85AcceptanceHumidityExceeded = 4106,  // SH85验收，湿度超限，用户可见
    SH85AcceptanceSensorCommError = 4107,  // SH85验收，传感器通信错误，用户可见
    SH85AcceptanceThresholdParamError = 4108,  // SH85验收，阈值参数错误，用户可见
    SH85AcceptanceTimeout = 4109,   // SH85验收超时，用户不可见

    VEFCAbnormal = 5001,  // VEFC异常（流量控制器异常），用户可见
    VEEPAbnormal = 5002,  // VEEP异常（压力控制器异常），用户可见
    SH85Abnormal = 5003,  // 85异常（温湿度传感器异常），用户可见
    HumidityNotReached = 5101  // 湿度未达标（充氮半小时，湿度不达标），用户可见
};

// 获取警告类型的显示名称
inline QString alarmTypeName(int type)
{
    switch (type) {
        case static_cast<int>(AlarmType::LoginFailed):          return QStringLiteral("Login Failed");
        case static_cast<int>(AlarmType::ChangePasswordFailed): return QStringLiteral("Change Password Failed");
        case static_cast<int>(AlarmType::DeviceOffline):        return QStringLiteral("Device Offline");
        case static_cast<int>(AlarmType::PermissionDenied):    return QStringLiteral("Permission Denied");
        case static_cast<int>(AlarmType::StorageFull):         return QStringLiteral("Storage Full");
        case static_cast<int>(AlarmType::TemperatureSensorAbnormal): return QStringLiteral("Temperature Sensor Abnormal");
        case static_cast<int>(AlarmType::HumiditySensorAbnormal):   return QStringLiteral("Humidity Sensor Abnormal");
        case static_cast<int>(AlarmType::VEFCUnitConfigFailed): return QStringLiteral("VEFC Unit Config Failed");
        case static_cast<int>(AlarmType::VEFCGasMediumConfigFailed): return QStringLiteral("VEFC Gas Medium Config Failed");
        case static_cast<int>(AlarmType::VEFCFlowValueSetFailed): return QStringLiteral("VEFC Flow Value Set Failed");
        case static_cast<int>(AlarmType::CommandSendFailed): return QStringLiteral("Command Send Failed");
        case static_cast<int>(AlarmType::SH85SelfCheckActionFailed): return QStringLiteral("SH85 Self Check Action Failed");
        case static_cast<int>(AlarmType::SH85StartSelfCheckNetworkError): return QStringLiteral("SH85 Start Self Check Network Error");
        case static_cast<int>(AlarmType::SH85PreCheckNetworkError): return QStringLiteral("SH85 Pre Check Network Error");
        case static_cast<int>(AlarmType::SH85PreCheckNotEnterSelfCheck): return QStringLiteral("SH85 Pre Check Not Enter Self Check");
        case static_cast<int>(AlarmType::SH85PreCheckStatusAbnormal): return QStringLiteral("SH85 Pre Check Status Abnormal");
        case static_cast<int>(AlarmType::SH85AcceptanceNetworkError): return QStringLiteral("SH85 Acceptance Network Error");
        case static_cast<int>(AlarmType::SH85AcceptanceHumidityExceeded): return QStringLiteral("SH85 Acceptance Humidity Exceeded");
        case static_cast<int>(AlarmType::SH85AcceptanceSensorCommError): return QStringLiteral("SH85 Acceptance Sensor Comm Error");
        case static_cast<int>(AlarmType::SH85AcceptanceThresholdParamError): return QStringLiteral("SH85 Acceptance Threshold Param Error");
        case static_cast<int>(AlarmType::SH85AcceptanceTimeout): return QStringLiteral("SH85 Acceptance Timeout");
        case static_cast<int>(AlarmType::VEFCAbnormal): return QStringLiteral("VEFC Abnormal");
        case static_cast<int>(AlarmType::VEEPAbnormal): return QStringLiteral("VEEP Abnormal");
        case static_cast<int>(AlarmType::SH85Abnormal): return QStringLiteral("SH85 Abnormal");
        case static_cast<int>(AlarmType::HumidityNotReached): return QStringLiteral("Humidity Not Reached");
        default: return QString::number(type);
    }
}

// 获取所有警告类型的(名称, 值)列表，便于 UI 填充下拉框
inline QList<QPair<QString, int>> alarmTypeList()
{
    return {
        { QStringLiteral("Login Failed"),          static_cast<int>(AlarmType::LoginFailed)          },
        { QStringLiteral("Change Password Failed"), static_cast<int>(AlarmType::ChangePasswordFailed) },
        { QStringLiteral("Device Offline"),        static_cast<int>(AlarmType::DeviceOffline)        },
        { QStringLiteral("Permission Denied"),    static_cast<int>(AlarmType::PermissionDenied)    },
        { QStringLiteral("Storage Full"),         static_cast<int>(AlarmType::StorageFull)         },
        { QStringLiteral("Temperature Sensor Abnormal"), static_cast<int>(AlarmType::TemperatureSensorAbnormal) },
        { QStringLiteral("Humidity Sensor Abnormal"),   static_cast<int>(AlarmType::HumiditySensorAbnormal)   },
        { QStringLiteral("VEFC Unit Config Failed"), static_cast<int>(AlarmType::VEFCUnitConfigFailed) },
        { QStringLiteral("VEFC Gas Medium Config Failed"), static_cast<int>(AlarmType::VEFCGasMediumConfigFailed) },
        { QStringLiteral("VEFC Flow Value Set Failed"), static_cast<int>(AlarmType::VEFCFlowValueSetFailed) },
        { QStringLiteral("Command Send Failed"), static_cast<int>(AlarmType::CommandSendFailed) },
        { QStringLiteral("SH85 Self Check Action Failed"), static_cast<int>(AlarmType::SH85SelfCheckActionFailed) },
        { QStringLiteral("SH85 Start Self Check Network Error"), static_cast<int>(AlarmType::SH85StartSelfCheckNetworkError) },
        { QStringLiteral("SH85 Pre Check Network Error"), static_cast<int>(AlarmType::SH85PreCheckNetworkError) },
        { QStringLiteral("SH85 Pre Check Not Enter Self Check"), static_cast<int>(AlarmType::SH85PreCheckNotEnterSelfCheck) },
        { QStringLiteral("SH85 Pre Check Status Abnormal"), static_cast<int>(AlarmType::SH85PreCheckStatusAbnormal) },
        { QStringLiteral("SH85 Acceptance Network Error"), static_cast<int>(AlarmType::SH85AcceptanceNetworkError) },
        { QStringLiteral("SH85 Acceptance Humidity Exceeded"), static_cast<int>(AlarmType::SH85AcceptanceHumidityExceeded) },
        { QStringLiteral("SH85 Acceptance Sensor Comm Error"), static_cast<int>(AlarmType::SH85AcceptanceSensorCommError) },
        { QStringLiteral("SH85 Acceptance Threshold Param Error"), static_cast<int>(AlarmType::SH85AcceptanceThresholdParamError) },
        { QStringLiteral("SH85 Acceptance Timeout"), static_cast<int>(AlarmType::SH85AcceptanceTimeout) },
        { QStringLiteral("VEFC Abnormal"), static_cast<int>(AlarmType::VEFCAbnormal) },
        { QStringLiteral("VEEP Abnormal"), static_cast<int>(AlarmType::VEEPAbnormal) },
        { QStringLiteral("SH85 Abnormal"), static_cast<int>(AlarmType::SH85Abnormal) },
        { QStringLiteral("Humidity Not Reached"), static_cast<int>(AlarmType::HumidityNotReached) }
    };
}

// 根据警告类型获取对应的警报级别
inline int alarmTypeToLevel(int type)
{
    switch (type) {
        case static_cast<int>(AlarmType::LoginFailed):
            return static_cast<int>(AlarmLevel::Warn);
        case static_cast<int>(AlarmType::ChangePasswordFailed):
            return static_cast<int>(AlarmLevel::Warn);
        case static_cast<int>(AlarmType::DeviceOffline):
            return static_cast<int>(AlarmLevel::Error);
        case static_cast<int>(AlarmType::PermissionDenied):
        case static_cast<int>(AlarmType::StorageFull):
            return static_cast<int>(AlarmLevel::Warn);
        case static_cast<int>(AlarmType::TemperatureSensorAbnormal):
        case static_cast<int>(AlarmType::HumiditySensorAbnormal):
            return static_cast<int>(AlarmLevel::Error);
        case static_cast<int>(AlarmType::VEFCUnitConfigFailed):
            return static_cast<int>(AlarmLevel::Error);
        case static_cast<int>(AlarmType::VEFCGasMediumConfigFailed):
        case static_cast<int>(AlarmType::VEFCFlowValueSetFailed):
            return static_cast<int>(AlarmLevel::Fatal);
        case static_cast<int>(AlarmType::CommandSendFailed):
            return static_cast<int>(AlarmLevel::Warn);
        case static_cast<int>(AlarmType::SH85SelfCheckActionFailed):
            return static_cast<int>(AlarmLevel::Error);
        case static_cast<int>(AlarmType::SH85StartSelfCheckNetworkError):
        case static_cast<int>(AlarmType::SH85PreCheckNetworkError):
        case static_cast<int>(AlarmType::SH85AcceptanceNetworkError):
            return static_cast<int>(AlarmLevel::Warn);
        case static_cast<int>(AlarmType::SH85AcceptanceHumidityExceeded):
        case static_cast<int>(AlarmType::SH85AcceptanceSensorCommError):
        case static_cast<int>(AlarmType::SH85AcceptanceThresholdParamError):
        case static_cast<int>(AlarmType::SH85AcceptanceTimeout):
            return static_cast<int>(AlarmLevel::Fatal);
        case static_cast<int>(AlarmType::SH85PreCheckNotEnterSelfCheck):
        case static_cast<int>(AlarmType::SH85PreCheckStatusAbnormal):
            return static_cast<int>(AlarmLevel::Fatal);
        case static_cast<int>(AlarmType::VEFCAbnormal):
        case static_cast<int>(AlarmType::VEEPAbnormal):
        case static_cast<int>(AlarmType::SH85Abnormal):
        case static_cast<int>(AlarmType::HumidityNotReached):
            return static_cast<int>(AlarmLevel::Error);
        default:
            return static_cast<int>(AlarmLevel::Error);
    }
}

// 警报来源枚举
enum class AlarmSource : int
{
    Device = 1,  // 设备警报（使用qrcode确定设备）
    User   = 2,  // 用户警报（使用用户名）
    System = 3   // 系统警报
};

// 警报解决状态枚举
enum class AlarmResolvedStatus : int
{
    Unresolved = 0,  // 未解决
    Resolved   = 1,  // 已解决
    NoNeed     = 2   // 无需解决
};

// 获取警报解决状态的显示名称
inline QString alarmResolvedStatusName(int status)
{
    switch (status) {
        case static_cast<int>(AlarmResolvedStatus::Unresolved): return QStringLiteral("Unresolved");
        case static_cast<int>(AlarmResolvedStatus::Resolved):   return QStringLiteral("Resolved");
        case static_cast<int>(AlarmResolvedStatus::NoNeed):     return QStringLiteral("No Need");
        default: return QString::number(status);
    }
}

// 获取所有警报解决状态的(名称, 值)列表，便于 UI 填充下拉框
inline QList<QPair<QString, int>> alarmResolvedStatusList()
{
    return {
        { QStringLiteral("Unresolved"), static_cast<int>(AlarmResolvedStatus::Unresolved) },
        { QStringLiteral("Resolved"),   static_cast<int>(AlarmResolvedStatus::Resolved)   },
        { QStringLiteral("No Need"),     static_cast<int>(AlarmResolvedStatus::NoNeed)     }
    };
}

// 根据警告类型获取对应的默认解决状态
inline int alarmTypeToResolvedStatus(int type)
{
    switch (type) {
        case static_cast<int>(AlarmType::LoginFailed):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);  // 登录失败无需解决（仅记录）
        case static_cast<int>(AlarmType::ChangePasswordFailed):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);  // 修改密码失败无需解决
        case static_cast<int>(AlarmType::DeviceOffline):
            return static_cast<int>(AlarmResolvedStatus::Unresolved);  // 设备离线需解决
        case static_cast<int>(AlarmType::PermissionDenied):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);  // 权限拒绝无需解决（需配置）
        case static_cast<int>(AlarmType::StorageFull):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::TemperatureSensorAbnormal):
        case static_cast<int>(AlarmType::HumiditySensorAbnormal):
            return static_cast<int>(AlarmResolvedStatus::Unresolved);
        case static_cast<int>(AlarmType::VEFCUnitConfigFailed):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::VEFCGasMediumConfigFailed):
        case static_cast<int>(AlarmType::VEFCFlowValueSetFailed):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::CommandSendFailed):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::SH85SelfCheckActionFailed):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::SH85StartSelfCheckNetworkError):
        case static_cast<int>(AlarmType::SH85PreCheckNetworkError):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::SH85PreCheckNotEnterSelfCheck):
        case static_cast<int>(AlarmType::SH85PreCheckStatusAbnormal):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::SH85AcceptanceNetworkError):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::SH85AcceptanceHumidityExceeded):
        case static_cast<int>(AlarmType::SH85AcceptanceSensorCommError):
        case static_cast<int>(AlarmType::SH85AcceptanceThresholdParamError):
        case static_cast<int>(AlarmType::SH85AcceptanceTimeout):
            return static_cast<int>(AlarmResolvedStatus::NoNeed);
        case static_cast<int>(AlarmType::VEFCAbnormal):
        case static_cast<int>(AlarmType::VEEPAbnormal):
        case static_cast<int>(AlarmType::SH85Abnormal):
        case static_cast<int>(AlarmType::HumidityNotReached):
            return static_cast<int>(AlarmResolvedStatus::Unresolved);
        default:
            return static_cast<int>(AlarmResolvedStatus::Unresolved);
    }
}

// 获取警报来源的显示名称
inline QString alarmSourceName(int source)
{
    switch (source) {
        case static_cast<int>(AlarmSource::Device): return QStringLiteral("Device");
        case static_cast<int>(AlarmSource::User):   return QStringLiteral("User");
        case static_cast<int>(AlarmSource::System): return QStringLiteral("System");
        default: return QString::number(source);
    }
}

// 获取所有警报来源的(名称, 值)列表，便于 UI 填充下拉框
inline QList<QPair<QString, int>> alarmSourceList()
{
    return {
        { QStringLiteral("Device"), static_cast<int>(AlarmSource::Device) },
        { QStringLiteral("User"),   static_cast<int>(AlarmSource::User)   },
        { QStringLiteral("System"), static_cast<int>(AlarmSource::System) }
    };
}


#endif // ALARMTYPE_H
