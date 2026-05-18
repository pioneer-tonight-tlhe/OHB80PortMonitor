#ifndef SH85PERIODICSELFCHECKSETTINGWIDGET_H
#define SH85PERIODICSELFCHECKSETTINGWIDGET_H

#include "settingwidget.h"
#include "scheduler/tasks/sh85_periodic_self_check_task.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>

class SettingItemWidget;
class SH85SelfCheckReportDialog;

// ====================================================================
// SH85PeriodicSelfCheckSettingWidget — SH85 周期自检配置控件
//
//   4 个 Item：
//     Item 1 启用开关：ComboBox(false / true)
//     Item 2 周期参数：SpinBox + Unit ComboBox(s/min/hour) + Set 按钮
//     Item 3 自检状态：只读 LineEdit，三种文案：
//                     - "自检功能未启用"
//                     - "自检中（执行：Xs）"
//                     - "等待下次自检中（倒计时：Xs）"
//     Item 4 查看报告：PushButton，打开自检报告模态框
//
//   常驻任务：通过 SharedData::getSH85PeriodicSelfCheckTask() 获取
//            （由 SharedData::initScheduler() 创建并提交调度器），
//            UI 层仅订阅信号 + 调用 setEnabled / setPeriod。
// ====================================================================
class SH85PeriodicSelfCheckSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit SH85PeriodicSelfCheckSettingWidget(QWidget *parent = nullptr);
    ~SH85PeriodicSelfCheckSettingWidget() override;

    /// 设置控件是否可用（false 时整个控件不可用）
    void setEnabled(bool enabled);

    /// 返回是否启用了周期自检功能
    bool isRunning() const { return m_isEnabled; }

signals:
    /// 自检启用状态变化（true=启用 / false=未启用）
    void runningStateChanged(bool running);

private slots:
    // ---- UI 事件 ----
    void onEnableComboChanged(int index);
    void onSetBtnClicked();
    void onReportBtnClicked();

    // ---- task 状态 / 计时回调 ----
    void onTaskStateChanged(SH85PeriodicSelfCheckTask::State state);
    void onTaskElapsedTick(int elapsedSeconds);
    void onTaskIntervalCountdown(int remainingSeconds);

private:
    void initUI();
    void initEnableItem();
    void initPeriodItem();
    void initStatusItem();
    void initReportItem();

    void bindTask();   // 从 SharedData 获取常驻任务并连接信号

    void refreshStatusText();   // 综合 m_currentTaskState / m_elapsedSec / m_intervalSec 更新 LineEdit

    // 把 ComboBox 的 unit 索引转为 TimeUnit
    static SH85PeriodicSelfCheckTask::TimeUnit unitFromIndex(int idx);

private:
    // ---- Item 1 启用开关 ----
    QComboBox          *m_enableCombo  = nullptr;
    SettingItemWidget  *m_enableItem   = nullptr;

    // ---- Item 2 周期参数 ----
    QSpinBox           *m_periodSpinBox = nullptr;
    QComboBox          *m_unitCombo     = nullptr;
    QPushButton        *m_setBtn        = nullptr;
    SettingItemWidget  *m_periodItem    = nullptr;

    // ---- Item 3 自检状态 ----
    QLineEdit          *m_statusEdit    = nullptr;
    SettingItemWidget  *m_statusItem    = nullptr;

    // ---- Item 4 查看报告 ----
    QPushButton        *m_reportBtn     = nullptr;
    SettingItemWidget  *m_reportItem    = nullptr;

    // ---- 内部状态 ----
    bool                m_isEnabled         = false;
    SH85PeriodicSelfCheckTask::State m_currentTaskState = SH85PeriodicSelfCheckTask::State::Stopped;
    int                 m_elapsedSec        = 0;
    int                 m_intervalRemainSec = 0;

    // ---- 调度任务（常驻任务，UI 构造时创建）----
    QPointer<SH85PeriodicSelfCheckTask> m_task;

    // ---- 报告 Dialog（懒加载）----
    QPointer<SH85SelfCheckReportDialog> m_reportDialog;
};

#endif // SH85PERIODICSELFCHECKSETTINGWIDGET_H
