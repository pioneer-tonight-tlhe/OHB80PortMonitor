/*******************************************************************************************
 * @file debugpage.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class DebugPage
 * @brief 组织调试页面中的固件、VEFC、UI 和设备状态配置控件。
 *
 * 设计目标：
 *      1. 统一管理调试配置模块的创建、布局和导航跳转逻辑。
 *      2. 在页面初始化阶段同步设备配置中的调试参数默认值。
 *      3. 将调试页面模块权限统一交由 App 按配置文件注册。
 *******************************************************************************************/
#ifndef DEBUGPAGE_H
#define DEBUGPAGE_H

#include <QWidget>

namespace Ui {
class DebugPage;
}

class FirmwareUpdateConfigSettingWidget;
class FirmwareUpdateSettingWidget;
class VEFCGasTypeSettingWidget;
class UIRefreshTimeSettingWidget;
class HumidityOffsetSettingWidget;
class VEFCFlowUnitMediumStatusWidget;
class BoardEnableStatusWidget;
class FoupInVacuumExtractionEnableWidget;

class DebugPage : public QWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit DebugPage(QWidget *parent = nullptr);
    ~DebugPage() override;

    // ============================ 界面初始化 ============================
    void initUI();
    void initNav();

private:
    // ---- 权限注册 ----
    void registerModulePermissions();

private slots:
    // ---- 导航响应 ----
    void navBtnClicked();

private:
    // ---- Ui 对象 ----
    Ui::DebugPage *ui;

    // ---- 调试配置控件 ----
    FirmwareUpdateConfigSettingWidget *m_firmwareConfigWidget;
    FirmwareUpdateSettingWidget *m_firmwareUpdateWidget;
    VEFCGasTypeSettingWidget *m_vefcGasTypeWidget;
    UIRefreshTimeSettingWidget *m_uiRefreshTimeWidget;
    HumidityOffsetSettingWidget *m_humidityOffsetWidget;
    VEFCFlowUnitMediumStatusWidget *m_vefcFlowUnitMediumStatusWidget;
    BoardEnableStatusWidget *m_boardEnableStatusWidget;
    FoupInVacuumExtractionEnableWidget *m_foupInVacuumExtractionEnableWidget;
};

#endif // DEBUGPAGE_H
