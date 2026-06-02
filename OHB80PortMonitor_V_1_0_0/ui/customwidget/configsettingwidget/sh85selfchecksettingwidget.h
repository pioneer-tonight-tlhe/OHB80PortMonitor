#ifndef SH85SELFCHECKSETTINGWIDGET_H
#define SH85SELFCHECKSETTINGWIDGET_H

#include "settingwidget.h"
#include "scheduler/tasks/sh85selfchecktask/sh85_self_check_task.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QSpinBox>
#include <QPushButton>
#include <QString>

class SettingItemWidget;

// ====================================================================
// SH85SelfCheckSettingWidget — SH85 自检配置控件
//
//   1. Device ID SpinBox（0 ~ 99999）
//   2. Self-check Item：一个 Check 按钮
//      - 空闲：    "Check"
//      - 0 ~ 60s： "Processing (60)" → "Processing (1)"（countdownTick - 10）
//      - 60 ~ 70s："Checking (10)"   → "Checking (1)"（来自 statusChanged）
//      - 结束：弹窗反馈，按钮恢复 "Check"
// ====================================================================
class SH85SelfCheckSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit SH85SelfCheckSettingWidget(QWidget *parent = nullptr);
    ~SH85SelfCheckSettingWidget();

    // 设置控件是否可用（false 时整个控件不可用）
    void setEnabled(bool enabled);
    void setCheckActionEnabled(bool enabled);

    // 返回是否处于自检工作中
    bool isRunning() const;

signals:
    void runningStateChanged(bool running);  // 自检状态变化信号

private slots:
    void onCheckBtnClicked();

    // 来自 SH85SelfCheckTask 的实时反馈
    void onCountdownTick(int remainingSeconds, const QString &qrcode);
    void onStatusChanged(const QString &text, const QString &qrcode);
    void onAllFinished(bool success,
                       SH85SelfChecker::Result result,
                       const QString &qrcode);

private:
    void initUI();
    void initDeviceIdItem();
    void initSelfCheckItem();

    void submitSelfCheckTask(const QString &qrcode);
    void resetButton();
    void refreshActionState();

    static QString resultToFriendlyText(SH85SelfChecker::Result r);

private:
    // 控件指针
    QSpinBox     *m_deviceIdSpinBox = nullptr;
    QPushButton  *m_checkBtn        = nullptr;

    // SettingItemWidget 指针
    SettingItemWidget *m_deviceIdItem    = nullptr;
    SettingItemWidget *m_selfCheckItem  = nullptr;

    // 当前正在执行任务的 qrcode（用于过滤无关信号）
    QString m_runningQrcode;
    bool m_checkActionEnabled = true;
};

#endif // SH85SELFCHECKSETTINGWIDGET_H
