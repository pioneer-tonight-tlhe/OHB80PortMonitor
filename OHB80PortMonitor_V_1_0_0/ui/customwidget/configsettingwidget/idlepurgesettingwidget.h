/*******************************************************************************************
 * @file idlepurgesettingwidget.h
 * @author Simon <工号：13> 2026-06-23
 *
 * @class IdlePurgeSettingWidget
 * @brief 提供 Idle Purge 配置项的界面展示与下发入口。
 *
 * 设计目标：
 *      1. 仅负责 Idle Purge 配置项的界面展示、输入收集与任务触发。
 *      2. 通过统一的设置项控件组织启用、时长和周期参数的编辑入口。
 *      3. 将配置持久化职责保留在调度任务层，避免界面层直接写配置文件。
 *******************************************************************************************/
#ifndef IDLEPURGESETTINGWIDGET_H
#define IDLEPURGESETTINGWIDGET_H

#include "settingwidget.h"
#include "tasks/set_idle_purge_task/set_idle_purge_task.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

class SettingItemWidget;

class IdlePurgeSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit IdlePurgeSettingWidget(QWidget *parent = nullptr);
    ~IdlePurgeSettingWidget() override;

    // ============================ 业务功能 ============================
    void setConfigValues(bool enabled, int purgeDurationSeconds, int purgeIntervalSeconds);
    QWidget* preparationTimeItem() const;

private:
    // ============================ 界面初始化 ============================
    void initUI();
    void initPreparationTimeItem();
    void initEnableItem();
    void initDurationItem();
    void initIntervalItem();

    // ============================ 任务提交 ============================
    void submitCommand(SettingItemWidget *item,
                       SetIdlePurgeTask::IdlePurgeProperty property,
                       quint16 value);
    void loadConfigValues();
    void setAllSetButtonsEnabled(bool enabled);

private slots:
    // ---- 按钮响应 ----
    void onEnableSetBtnClicked();
    void onDurationSetBtnClicked();
    void onIntervalSetBtnClicked();

private:
    // ---- 控件成员 ----
    QLineEdit *m_preparationTimeLineEdit;
    QComboBox *m_enableComboBox;
    QSpinBox *m_durationSpinBox;
    QSpinBox *m_intervalSpinBox;
    QPushButton *m_enableSetBtn;
    QPushButton *m_durationSetBtn;
    QPushButton *m_intervalSetBtn;

    // ---- 条目成员 ----
    SettingItemWidget *m_enableItem;
    SettingItemWidget *m_durationItem;
    SettingItemWidget *m_intervalItem;
    SettingItemWidget *m_preparationTimeItem;
};

#endif // IDLEPURGESETTINGWIDGET_H
