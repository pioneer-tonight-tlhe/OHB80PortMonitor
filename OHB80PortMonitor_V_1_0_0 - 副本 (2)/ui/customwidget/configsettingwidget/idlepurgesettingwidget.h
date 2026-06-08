#ifndef IDLEPURGESETTINGWIDGET_H
#define IDLEPURGESETTINGWIDGET_H

#include "settingwidget.h"
#include "tasks/set_idle_purge_task.h"

#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>

class SettingItemWidget;

// ====================================================================
// IdlePurgeSettingWidget — Idle Purge 参数配置控件
//   继承 SettingWidget，提供以下设置项：
//   1. Preparation Time    — 只读，固定显示 10 s
//   2. Idle Purge Enable   — ComboBox(Enable/Disable) + Set 按钮
//   3. Purge Duration      — SpinBox(秒) + Set 按钮
//   4. Purge Interval      — SpinBox(秒) + Set 按钮
// ====================================================================
class IdlePurgeSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit IdlePurgeSettingWidget(QWidget *parent = nullptr);
    ~IdlePurgeSettingWidget();

private slots:
    void onEnableSetBtnClicked();       // 设置 Idle Purge 使能
    void onDurationSetBtnClicked();     // 设置充气持续时间
    void onIntervalSetBtnClicked();     // 设置充气间隔时间

private:
    void initUI();

    void initPrepTimeItem();            // 准备阶段时间（只读）
    void initEnableItem();              // 使能设置项
    void initDurationItem();            // 充气持续时间设置项
    void initIntervalItem();            // 充气间隔时间设置项

    // 通用指令提交方法
    void submitCommand(SettingItemWidget *item,
                       SetIdlePurgeTask::IdlePurgeProperty property,
                       quint16 value);

private:
    // 设置任务运行期间所有 Set 按钮的启用状态
    void setAllSetButtonsEnabled(bool enabled);

private:
    // 控件指针
    QLineEdit  *m_prepTimeLineEdit;     // 准备阶段时间（只读）
    QComboBox  *m_enableComboBox;       // 使能选择
    QSpinBox   *m_durationSpinBox;      // 充气持续时间
    QSpinBox   *m_intervalSpinBox;      // 充气间隔时间

    // Set 按钮指针（任务期间禁用，避免并发任务提交）
    QPushButton *m_enableSetBtn;
    QPushButton *m_durationSetBtn;
    QPushButton *m_intervalSetBtn;

    // SettingItemWidget 指针（用于显示状态）
    SettingItemWidget *m_enableItem;
    SettingItemWidget *m_durationItem;
    SettingItemWidget *m_intervalItem;
};

#endif // IDLEPURGESETTINGWIDGET_H
