/*******************************************************************************************
 * @file alarmlogwidget.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class AlarmLogWidget
 * @brief 提供警报日志实时显示、历史分页查询和状态刷新界面。
 *
 * 设计目标：
 *      1. 将实时未恢复警报和历史警报查询统一放在同一业务控件中维护。
 *      2. 通过任务化查询与本地条件缓存支持分页复用和界面快速刷新。
 *      3. 保持 live log 与数据库状态同步，便于现场快速定位未解决警报。
 *******************************************************************************************/
#ifndef ALARMLOGWIDGET_H
#define ALARMLOGWIDGET_H

#include <QList>
#include <QModelIndex>
#include <QWidget>

#include "alarmrecord.h"

namespace Ui {
class AlarmLogWidget;
}

class QStandardItemModel;
class WaitDialog;

class AlarmLogWidget : public QWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit AlarmLogWidget(QWidget* parent = nullptr);
    ~AlarmLogWidget();

    // ============================ 界面初始化 ============================
    void initUi();

private:
    // ---- 查询与显示 ----
    void submitQuery(int page);
    void setHistoryLogData(const QList<AlarmRecord>& data);
    void initLiveLog();
    void loadUnresolvedToLiveLog();
    void trimLiveLogRows();
    int liveLogInsertRowForQRCode(const QStandardItemModel* model, const QString& qrCode) const;

private slots:
    // ---- UI 交互 ----
    void onCheckBoxAllStateChanged(int state);
    void onSearchClicked();
    void onSetStartTimeClicked();
    void onSetResolvedTimeClicked();
    void onPaginationPageChanged(int page);
    void onLiveLogClicked(const QModelIndex& index);
    void onHistoryLogClicked(const QModelIndex& index);
    void onCancelRequested();

    // ---- 查询与实时结果 ----
    void onPageWithConditionsResult(const QList<AlarmRecord>& records);
    void onTotalCountWithConditionsResult(int totalCount);
    void onRecordInserted(const AlarmRecord& record);
    void onRecordResolved(const QString& qrCode, const QString& alarmType, const QString& resolveTime);
    void onTaskFinished(bool success, const QString& message);

private:
    // ---- UI 成员 ----
    Ui::AlarmLogWidget* ui;
    WaitDialog* m_waitDialog;
    QString m_activeTaskId;

    // ---- 分页状态成员 ----
    int m_currentPage;
    int m_pageSize;
    int m_totalPages;

    // ---- 查询条件成员 ----
    int m_lastAlarmLevel;
    QString m_lastQRCode;
    QString m_lastAlarmType;
    int m_lastIsResolved;
    QString m_lastStartTime;
    QString m_lastEndTime;
    QString m_lastResolveStartTime;
    QString m_lastResolveEndTime;

    // ---- 容量限制 ----
    static constexpr int kLiveLogMaxRows = 100;
};

#endif // ALARMLOGWIDGET_H
