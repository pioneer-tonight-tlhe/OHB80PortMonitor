#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QWidget>

namespace Ui {
class ConfigPage;
}

class IdlePurgeSettingWidget;
class PneumaticValvePressureSettingWidget;
class SH85PeriodicSelfCheckSettingWidget;
class SH85SelfCheckSettingWidget;
class HumidityOffsetSettingWidget;
class PurgeFlowSettingWidget;
class DeviceEnableSettingWidget;

// ====================================================================
// ConfigPage — 配置页面
//
//   包含所有设备配置控件的页面，支持顶部导航栏快速定位
//   包含的配置控件：
//     - IdlePurgeSettingWidget：空闲吹扫配置
//     - PneumaticValvePressureSettingWidget：气动阀门压力配置
//     - SH85PeriodicSelfCheckSettingWidget：SH85 定期自检配置
//     - SH85SelfCheckSettingWidget：SH85 手动自检配置
//     - HumidityOffsetSettingWidget：湿度偏移配置
//     - DeviceEnableSettingWidget：设备可用性配置
//     - PurgeFlowSettingWidget：吹扫流量配置
// ====================================================================
class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPage(QWidget *parent = nullptr);
    ~ConfigPage();

    void initUI();  // 初始化 UI 和所有配置控件

    // 导航栏
    void initNav();  // 初始化顶部导航栏（包含所有配置控件的导航按钮）

private slots:
    void navBtnClicked();  // 导航按钮点击事件
    void onSelfCheckRunningStateChanged(bool running);        // 手动自检状态变化
    void onPeriodicSelfCheckRunningStateChanged(bool running); // 定期自检状态变化

private:
    Ui::ConfigPage *ui;
    IdlePurgeSettingWidget *m_idlePurgeWidget;                   // 空闲吹扫配置控件
    PneumaticValvePressureSettingWidget *m_pneumaticValvePressureWidget;  // 气动阀门压力配置控件
    SH85PeriodicSelfCheckSettingWidget *m_sh85PeriodicSelfCheckWidget;   // SH85 定期自检配置控件
    SH85SelfCheckSettingWidget *m_sh85SelfCheckWidget;          // SH85 手动自检配置控件
    DeviceEnableSettingWidget *m_deviceEnableWidget;            // 设备可用性配置控件
    HumidityOffsetSettingWidget *m_humidityOffsetWidget;        // 湿度偏移配置控件
    PurgeFlowSettingWidget *m_purgeFlowWidget;                  // 吹扫流量配置控件
};

#endif // CONFIGPAGE_H
