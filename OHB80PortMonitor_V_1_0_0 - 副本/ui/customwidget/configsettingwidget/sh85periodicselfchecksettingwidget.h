#ifndef SH85PERIODICSELFCHECKSETTINGWIDGET_H
#define SH85PERIODICSELFCHECKSETTINGWIDGET_H

#include "settingwidget.h"
#include "scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>

class SettingItemWidget;
class SH85SelfCheckReportDialog;

// SH85 鍛ㄦ湡鎬ц嚜妫€閰嶇疆鐣岄潰
// 鎻愪緵鍛ㄦ湡鎬ц嚜妫€鐨勫惎鐢?绂佺敤銆佸懆鏈熻缃€佺姸鎬佹樉绀哄拰鎶ュ憡鏌ョ湅鍔熻兘
class SH85PeriodicSelfCheckSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit SH85PeriodicSelfCheckSettingWidget(QWidget *parent = nullptr);
    ~SH85PeriodicSelfCheckSettingWidget() override;

    // 璁剧疆鎺т欢鏄惁鍚敤
    void setEnabled(bool enabled);
    // 璁剧疆鍛ㄦ湡鎬ф搷浣滄帶浠剁殑鍚敤鐘舵€侊紙鐢ㄤ簬椤甸潰绾т簰鏂ワ級
    void setPeriodicActionEnabled(bool enabled);

    // 杩斿洖鑷鏄惁姝ｅ湪杩愯
    bool isRunning() const { return m_isEnabled; }

signals:
    // 杩愯鐘舵€佹敼鍙樹俊鍙?    void runningStateChanged(bool running);

private slots:
    // UI 浜嬩欢妲藉嚱鏁?    void onEnableComboChanged(int index);      // 鍚敤涓嬫媺妗嗘敼鍙?    void onSetBtnClicked();                     // 璁剧疆鎸夐挳鐐瑰嚮
    void onReportBtnClicked();                  // 鎶ュ憡鎸夐挳鐐瑰嚮

    // 浠诲姟鍥炶皟妲藉嚱鏁?    void onTaskStateChanged(SH85PeriodicSelfCheckTask2::State state);    // 浠诲姟鐘舵€佹敼鍙?    void onTaskElapsedTick(int elapsedSeconds);                          // 鑷璁℃椂鍣?    void onTaskIntervalCountdown(int remainingSeconds);                   // 闂撮殧鍊掕鏃?    void onTaskBootDelayCountdown(int remainingSeconds);

private:
    void initUI();           // 鍒濆鍖栫晫闈?    void initEnableItem();   // 鍒濆鍖栧惎鐢ㄩ」
    void initPeriodItem();   // 鍒濆鍖栧懆鏈熼」
    void initStatusItem();   // 鍒濆鍖栫姸鎬侀」
    void initReportItem();   // 鍒濆鍖栨姤鍛婇」

    void bindTask();                      // 缁戝畾浠诲姟
    void refreshStatusText();             // 鍒锋柊鐘舵€佹枃鏈?    void refreshActionControlsState();    // 鍒锋柊鎿嶄綔鎺т欢鐘舵€?
    // 浠庝笅鎷夋绱㈠紩杞崲涓烘椂闂村崟浣?    static SH85PeriodicSelfCheckTask2::TimeUnit unitFromIndex(int idx);

private:
    // 椤?1锛氬惎鐢ㄥ紑鍏?    QComboBox         *m_enableCombo = nullptr;  // 鍚敤涓嬫媺妗?    SettingItemWidget *m_enableItem  = nullptr;  // 鍚敤椤瑰鍣?
    // 椤?2锛氬懆鏈熷弬鏁?    QSpinBox          *m_periodSpinBox = nullptr;  // 鍛ㄦ湡鏁板€艰緭鍏ユ
    QComboBox         *m_unitCombo     = nullptr;  // 鍗曚綅涓嬫媺妗?    QPushButton       *m_setBtn        = nullptr;  // 璁剧疆鎸夐挳
    SettingItemWidget *m_periodItem    = nullptr;  // 鍛ㄦ湡椤瑰鍣?
    // 椤?3锛氱姸鎬佹樉绀?    QLineEdit         *m_statusEdit = nullptr;  // 鐘舵€佺紪杈戞
    SettingItemWidget *m_statusItem = nullptr;  // 鐘舵€侀」瀹瑰櫒

    // 椤?4锛氭姤鍛?    QPushButton       *m_reportBtn  = nullptr;  // 鎶ュ憡鎸夐挳
    SettingItemWidget *m_reportItem = nullptr;  // 鎶ュ憡椤瑰鍣?
    // 鍐呴儴鐘舵€?    bool m_isEnabled             = false;  // 鏄惁鍚敤鍛ㄦ湡鎬ц嚜妫€
    bool m_periodicActionEnabled = true;   // 鍛ㄦ湡鎬ф搷浣滄帶浠舵槸鍚﹀惎鐢?    SH85PeriodicSelfCheckTask2::State m_currentTaskState = SH85PeriodicSelfCheckTask2::State::Stopped;  // 褰撳墠浠诲姟鐘舵€?    int m_elapsedSec             = 0;      // 鑷宸茶繍琛岀鏁?    int m_intervalRemainSec      = 0;      // 闂撮殧鍓╀綑绉掓暟
    int m_bootDelayRemainSec     = 0;

    // SharedData 鎸佹湁鐨勫父椹讳换鍔?    QPointer<SH85PeriodicSelfCheckTask2> m_task;

    // 鎳掑姞杞藉鐢ㄧ殑鎶ュ憡瀵硅瘽妗?    QPointer<SH85SelfCheckReportDialog> m_reportDialog;
};

#endif // SH85PERIODICSELFCHECKSETTINGWIDGET_H

