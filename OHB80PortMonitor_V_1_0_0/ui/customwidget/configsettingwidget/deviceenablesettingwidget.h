#ifndef DEVICEENABLESETTINGWIDGET_H
#define DEVICEENABLESETTINGWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QString>

class SettingItemWidget;

// ====================================================================
// DeviceEnableSettingWidget — OHB 设备可用性配置控件
//
//   1. Target Device QRCode SpinBox（int，初始值来自 SharedData::getAllQrcodes() 第一个）
//   2. 设备状态 ComboBox（Enable/Disable）+ Set 按钮
//
//   按钮说明：
//     - Set → 仅对 SpinBox 中的设备 ID 生效
//
//   功能：
//     - 持久化到配置文件（OHBDeviceConfig）
//     - 更新内存中 FoupOfOHBInfo::enable 状态
//     - 下发 Modbus 指令 SetBoardEnable（FC06, 寄存器 0x00FF, 1=禁用, 0=正常）
// ====================================================================
class DeviceEnableSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit DeviceEnableSettingWidget(QWidget *parent = nullptr);
    ~DeviceEnableSettingWidget();

    void setInitialConfigValue(bool enabled);

private slots:
    void onSetClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initStatusItem();

    // 下发 SetBoardEnable Modbus 指令
    void submitBoardEnableCommand(const QString &qrcode, bool enable);

    // 设置任务运行期间所有 Set 按钮的启用状态
    void setAllSetButtonsEnabled(bool enabled);

private:
    QSpinBox        *m_qrcodeSpinBox = nullptr;
    QComboBox       *m_statusComboBox = nullptr;

    // Set 按钮指针（任务期间禁用，避免并发任务提交）
    QPushButton *m_setBtn;

    SettingItemWidget *m_qrcodeItem = nullptr;
    SettingItemWidget *m_statusItem = nullptr;
};

#endif // DEVICEENABLESETTINGWIDGET_H
