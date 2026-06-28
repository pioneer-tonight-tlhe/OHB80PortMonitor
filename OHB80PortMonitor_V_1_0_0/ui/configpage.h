/*******************************************************************************************
 * @file configpage.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class ConfigPage
 * @brief 组织设备配置页面中的各类配置控件并负责页面内联动。
 *
 * 设计目标：
 *      1. 统一承载设备配置相关控件的创建、布局和导航跳转逻辑。
 *      2. 在页面初始化阶段同步各配置模块的默认显示状态与配置值。
 *      3. 管理自检、周期自检等配置控件之间的互斥联动关系。
 *******************************************************************************************/
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
class PurgeFlowSettingWidget;
class DeviceEnableSettingWidget;
class DeviceInfoSettingWidget;

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit ConfigPage(QWidget *parent = nullptr);
    ~ConfigPage() override;

    // ============================ 界面初始化 ============================
    void initUI();
    void initNav();

private:
    // ---- 权限注册 ----
    void registerModulePermissions();

private slots:
    // ---- 导航响应 ----
    void onNavBtnClicked();

    // ---- 状态联动 ----
    void onSelfCheckRunningStateChanged(bool running);
    void onPeriodicSelfCheckRunningStateChanged(bool running);

private:
    // ---- Ui 对象 ----
    Ui::ConfigPage *m_ui;

    // ---- 配置控件 ----
    IdlePurgeSettingWidget *m_idlePurgeWidget;
    PneumaticValvePressureSettingWidget *m_pneumaticValvePressureWidget;
    SH85PeriodicSelfCheckSettingWidget *m_sh85PeriodicSelfCheckWidget;
    SH85SelfCheckSettingWidget *m_sh85SelfCheckWidget;
    DeviceEnableSettingWidget *m_deviceEnableWidget;
    PurgeFlowSettingWidget *m_purgeFlowWidget;
    DeviceInfoSettingWidget *m_deviceInfoWidget;
};

#endif // CONFIGPAGE_H
