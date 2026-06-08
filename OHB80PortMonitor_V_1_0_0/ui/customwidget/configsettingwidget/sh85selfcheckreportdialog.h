#ifndef SH85SELFCHECKREPORTDIALOG_H
#define SH85SELFCHECKREPORTDIALOG_H

#include "scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task3.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QDialog>
#include <QHash>
#include <QString>
#include <QStringList>

class QTabWidget;
class QTableView;
class QStandardItemModel;

// ====================================================================
// SH85SelfCheckReportDialog — SH85 周期自检报告模态框
//
//   两个 Tab：
//     Tab 1 Live Log：80 行，每行展示当前自检的实时进度
//       列：QRCode / 执行状态 / 倒计时(s) / 是否成功
//     Tab 2 History Log：80 行，每行展示该设备的累计/上一次自检结果
//       列：上一次自检开始时间 / 成功次数 / 失败次数 / 是否参加 / Description
//
//   控制接口（由 SettingWidget 转发 task 信号驱动）：
//     setQrcodes(QStringList)            - 初始化 80 行（按 qrcode 顺序）
//     onCheckerCountdown(remain, qrcode)  - 更新 Live Log 倒计时
//     onCheckerStateChanged(state, qrcode) - 更新 Live Log 执行状态
//     onOneFinished(qrcode, success, desc) - 更新 Live Log 是否成功
//     onAllFinished(summary)              - 更新 History Log 一行的累计 / 上次结果
//     onRoundStarted()                    - 重置 Live Log 当前轮次状态
// ====================================================================
class SH85SelfCheckReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SH85SelfCheckReportDialog(QWidget *parent = nullptr);
    ~SH85SelfCheckReportDialog() override;

    /// 初始化 / 重设全部 qrcode 行（无数据则保留空）
    void setQrcodes(const QStringList &qrcodes);

public slots:
    /// 一轮开始：清空 Live Log 当前轮次的执行状态/倒计时/是否成功
    void onRoundStarted();

    /// 单设备倒计时（来自 task::countdownTick）
    void onCheckerCountdown(int remainingSeconds, const QString &masterId);

    /// 单设备阶段状态变化（来自 task::selfCheckerStateChanged）
    void onCheckerStateChanged(SH85SelfChecker::State state, const QString &masterId);

    /// 单设备结束（来自 task::oneFinished）
    void onOneFinished(const QString &masterId, bool success, const QString &description);

    /// 一轮自检结束（来自 task::allFinished）
    void onAllFinished(const SH85PeriodicSelfCheckTask3::SelfCheckSummary &summary);

    /// 设备参与状态变化（来自 task::deviceParticipated）
    void onDeviceParticipated(const QString &qrcode, bool participated);

private:
    void initUI();
    void initLiveLogTab();
    void initHistoryLogTab();

    int  liveRowOf(const QString &qrcode) const;
    int  historyRowOf(const QString &qrcode) const;

private:
    QTabWidget *m_tabWidget = nullptr;

    // ---- Tab 1 Live Log ----
    QTableView         *m_liveTable  = nullptr;
    QStandardItemModel *m_liveModel  = nullptr;

    // ---- Tab 2 History Log ----
    QTableView         *m_historyTable = nullptr;
    QStandardItemModel *m_historyModel = nullptr;

    // qrcode -> row 索引映射（Live / History 共享同一份顺序）
    QStringList                 m_qrcodes;
    QHash<QString, int>         m_qrcodeToRow;

    // 每个设备的累计统计（History 用）
    struct DeviceStat {
        int     successCount = 0;
        int     failureCount = 0;
        QString lastStartTime;
        bool    lastParticipated = false;
        QString lastDescription;
    };
    QHash<QString, DeviceStat>  m_deviceStats;
};

#endif // SH85SELFCHECKREPORTDIALOG_H
