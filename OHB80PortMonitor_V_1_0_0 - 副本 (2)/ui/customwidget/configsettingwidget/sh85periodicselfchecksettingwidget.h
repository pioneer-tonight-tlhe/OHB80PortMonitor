#ifndef SH85PERIODICSELFCHECKSETTINGWIDGET_H
#define SH85PERIODICSELFCHECKSETTINGWIDGET_H

#include "settingwidget.h"
#include "scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>

class SettingItemWidget;
class SH85SelfCheckReportDialog;

// SH85 周期性自检配置界面
// 提供周期性自检的启用/禁用、周期设置、状态显示和报告查看功能
class SH85PeriodicSelfCheckSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit SH85PeriodicSelfCheckSettingWidget(QWidget *parent = nullptr);
    ~SH85PeriodicSelfCheckSettingWidget() override;

    // 设置控件是否启用
    void setEnabled(bool enabled);
    // 设置周期性操作控件的启用状态（用于页面级互斥）
    void setPeriodicActionEnabled(bool enabled);

    // 返回自检是否正在运行
    bool isRunning() const { return m_isEnabled; }

signals:
    // 运行状态改变信号
    void runningStateChanged(bool running);

private slots:
    // UI 事件槽函数
    void onEnableComboChanged(int index);      // 启用下拉框改变
    void onSetBtnClicked();                     // 设置按钮点击
    void onReportBtnClicked();                  // 报告按钮点击

    // 任务回调槽函数
    void onTaskStateChanged(SH85PeriodicSelfCheckTask::State state);    // 任务状态改变
    void onTaskElapsedTick(int elapsedSeconds);                          // 自检计时器
    void onTaskIntervalCountdown(int remainingSeconds);                   // 间隔倒计时
    void onTaskBootDelayCountdown(int remainingSeconds);

private:
    void initUI();           // 初始化界面
    void initEnableItem();   // 初始化启用项
    void initPeriodItem();   // 初始化周期项
    void initStatusItem();   // 初始化状态项
    void initReportItem();   // 初始化报告项

    void bindTask();                      // 绑定任务
    void refreshStatusText();             // 刷新状态文本
    void refreshActionControlsState();    // 刷新操作控件状态

    // 从下拉框索引转换为时间单位
    static SH85PeriodicSelfCheckTask::TimeUnit unitFromIndex(int idx);

private:
    // 项 1：启用开关
    QComboBox         *m_enableCombo = nullptr;  // 启用下拉框
    SettingItemWidget *m_enableItem  = nullptr;  // 启用项容器

    // 项 2：周期参数
    QSpinBox          *m_periodSpinBox = nullptr;  // 周期数值输入框
    QComboBox         *m_unitCombo     = nullptr;  // 单位下拉框
    QPushButton       *m_setBtn        = nullptr;  // 设置按钮
    SettingItemWidget *m_periodItem    = nullptr;  // 周期项容器

    // 项 3：状态显示
    QLineEdit         *m_statusEdit = nullptr;  // 状态编辑框
    SettingItemWidget *m_statusItem = nullptr;  // 状态项容器

    // 项 4：报告
    QPushButton       *m_reportBtn  = nullptr;  // 报告按钮
    SettingItemWidget *m_reportItem = nullptr;  // 报告项容器

    // 内部状态
    bool m_isEnabled             = false;  // 是否启用周期性自检
    bool m_periodicActionEnabled = true;   // 周期性操作控件是否启用
    SH85PeriodicSelfCheckTask::State m_currentTaskState = SH85PeriodicSelfCheckTask::State::Stopped;  // 当前任务状态
    int m_elapsedSec             = 0;      // 自检已运行秒数
    int m_intervalRemainSec      = 0;      // 间隔剩余秒数
    int m_bootDelayRemainSec     = 0;

    // SharedData 持有的常驻任务
    QPointer<SH85PeriodicSelfCheckTask> m_task;

    // 懒加载复用的报告对话框
    QPointer<SH85SelfCheckReportDialog> m_reportDialog;
};

#endif // SH85PERIODICSELFCHECKSETTINGWIDGET_H
