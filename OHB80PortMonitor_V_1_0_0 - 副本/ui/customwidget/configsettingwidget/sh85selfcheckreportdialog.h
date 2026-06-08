#ifndef SH85SELFCHECKREPORTDIALOG_H
#define SH85SELFCHECKREPORTDIALOG_H

#include "scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QDialog>
#include <QHash>
#include <QString>
#include <QStringList>

class QTabWidget;
class QTableView;
class QStandardItemModel;

// ====================================================================
// SH85SelfCheckReportDialog 鈥?SH85 鍛ㄦ湡鑷鎶ュ憡妯℃€佹
//
//   涓や釜 Tab锛?//     Tab 1 Live Log锛?0 琛岋紝姣忚灞曠ず褰撳墠鑷鐨勫疄鏃惰繘搴?//       鍒楋細QRCode / 鎵ц鐘舵€?/ 鍊掕鏃?s) / 鏄惁鎴愬姛
//     Tab 2 History Log锛?0 琛岋紝姣忚灞曠ず璇ヨ澶囩殑绱/涓婁竴娆¤嚜妫€缁撴灉
//       鍒楋細涓婁竴娆¤嚜妫€寮€濮嬫椂闂?/ 鎴愬姛娆℃暟 / 澶辫触娆℃暟 / 鏄惁鍙傚姞 / Description
//
//   鎺у埗鎺ュ彛锛堢敱 SettingWidget 杞彂 task 淇″彿椹卞姩锛夛細
//     setQrcodes(QStringList)            - 鍒濆鍖?80 琛岋紙鎸?qrcode 椤哄簭锛?//     onCheckerCountdown(remain, qrcode)  - 鏇存柊 Live Log 鍊掕鏃?//     onCheckerStateChanged(state, qrcode) - 鏇存柊 Live Log 鎵ц鐘舵€?//     onOneFinished(qrcode, success, desc) - 鏇存柊 Live Log 鏄惁鎴愬姛
//     onAllFinished(summary)              - 鏇存柊 History Log 涓€琛岀殑绱 / 涓婃缁撴灉
//     onRoundStarted()                    - 閲嶇疆 Live Log 褰撳墠杞鐘舵€?// ====================================================================
class SH85SelfCheckReportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SH85SelfCheckReportDialog(QWidget *parent = nullptr);
    ~SH85SelfCheckReportDialog() override;

    /// 鍒濆鍖?/ 閲嶈鍏ㄩ儴 qrcode 琛岋紙鏃犳暟鎹垯淇濈暀绌猴級
    void setQrcodes(const QStringList &qrcodes);

public slots:
    /// 涓€杞紑濮嬶細娓呯┖ Live Log 褰撳墠杞鐨勬墽琛岀姸鎬?鍊掕鏃?鏄惁鎴愬姛
    void onRoundStarted();

    /// 鍗曡澶囧€掕鏃讹紙鏉ヨ嚜 task::countdownTick锛?    void onCheckerCountdown(int remainingSeconds, const QString &masterId);

    /// 鍗曡澶囬樁娈电姸鎬佸彉鍖栵紙鏉ヨ嚜 task::selfCheckerStateChanged锛?    void onCheckerStateChanged(SH85SelfChecker::State state, const QString &masterId);

    /// 鍗曡澶囩粨鏉燂紙鏉ヨ嚜 task::oneFinished锛?    void onOneFinished(const QString &masterId, bool success, const QString &description);

    /// 涓€杞嚜妫€缁撴潫锛堟潵鑷?task::allFinished锛?    void onAllFinished(const SH85PeriodicSelfCheckTask2::SelfCheckSummary &summary);

    /// 璁惧鍙備笌鐘舵€佸彉鍖栵紙鏉ヨ嚜 task::deviceParticipated锛?    void onDeviceParticipated(const QString &qrcode, bool participated);

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

    // qrcode -> row 绱㈠紩鏄犲皠锛圠ive / History 鍏变韩鍚屼竴浠介『搴忥級
    QStringList                 m_qrcodes;
    QHash<QString, int>         m_qrcodeToRow;

    // 姣忎釜璁惧鐨勭疮璁＄粺璁★紙History 鐢級
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

