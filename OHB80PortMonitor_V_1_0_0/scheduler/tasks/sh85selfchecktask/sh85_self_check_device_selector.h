#ifndef SH85_SELF_CHECK_DEVICE_SELECTOR_H
#define SH85_SELF_CHECK_DEVICE_SELECTOR_H

#include <QList>
#include <QString>
#include <QStringList>

class ModbusTcpMaster;

// ====================================================================
// SH85SelfCheckDeviceSelector - SH85 自检设备筛选器
//
// 设计目标：
//   1. 统一处理单设备模式/全量模式的目标设备列表。
//   2. 统一判断设备启用、FOUP 在位、网络连接等执行前置条件。
//   3. 只返回筛选结果，不修改轮次上下文，也不直接启动 checker。
// ====================================================================
class SH85SelfCheckDeviceSelector
{
public:
    // 单设备提交 checker 前的状态快照。
    struct DeviceInspection {
        QString qrcode;
        bool enabled = false;
        bool foupIn = false;
        bool connected = false;
        ModbusTcpMaster* master = nullptr;
        QString unavailableReason;

        bool canStartChecker() const
        {
            // 此处只确认 master 可用；selfChecker() 是否为空由 Task3 在启动前再判断。
            return enabled && !foupIn && connected && master;
        }
    };

    // 根据当前模式返回目标二维码列表；全量模式保持 SharedData 顺序。
    QStringList targetQrcodes(bool singleDeviceMode, const QString& singleDeviceQrcode) const;

    // 检查单个设备，并记录第一个阻止执行的原因。
    DeviceInspection inspectDevice(const QString& qrcode) const;

    // 按 Task3 后续汇总顺序检查目标设备。
    QList<DeviceInspection> inspectTargets(bool singleDeviceMode, const QString& singleDeviceQrcode) const;

    // 供 UI 查询当前可执行设备列表，不改变任何轮次状态。
    QStringList filterAvailableDevices(bool singleDeviceMode, const QString& singleDeviceQrcode) const;
};

#endif // SH85_SELF_CHECK_DEVICE_SELECTOR_H
