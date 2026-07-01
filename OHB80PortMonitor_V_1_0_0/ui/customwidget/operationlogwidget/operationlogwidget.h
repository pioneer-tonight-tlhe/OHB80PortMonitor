/*******************************************************************************************
 * @file operationlogwidget.h
 * @author Simon <工号：13> 2026-07-01
 *
 * @class OperationLogWidget
 * @brief 提供运行日志实时显示、历史分页查询和命中记录导航界面。
 *
 * 设计目标：
 *      1. 将实时日志显示和历史日志查询统一放在同一业务控件中维护。
 *      2. 按当前用户权限刷新可见日志，保持界面统计与数据库结果一致。
 *      3. 支持关键词命中导航、分页跳转和相邻页锚点翻页。
 *******************************************************************************************/
#ifndef OPERATIONLOGWIDGET_H
#define OPERATIONLOGWIDGET_H

#include <QList>
#include <QModelIndex>
#include <QSet>
#include <QString>
#include <QWidget>

#include "dbtypes.h"
#include "operationrecord.h"

namespace Ui {
class OperationLogWidget;
}

class DateTimeSetDialog;
class OperationLogQueryTask;
class WaitDialog;

class OperationLogWidget : public QWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit OperationLogWidget(QWidget* parent = nullptr);
    ~OperationLogWidget();

private:
    // ---- 查询与显示 ----
    void setHistoryLogData(const QList<OperationRecord>& data);
    void appendLiveLogRecord(const OperationRecord& record, bool scrollToBottom);
    void reloadLiveLogFromDatabase();
    void refreshByCurrentPermission();
    void showWaitingDialog(bool restartElapsed = true);
    void hideWaitingDialog();

    // ---- 查询任务 ----
    enum PageQueryMode
    {
        PageByNumber,
        PageAfterAnchor,
        PageBeforeAnchor
    };

    void submitQueryTask(int targetPage,
                         int pendingSelectId = 0,
                         PageQueryMode mode = PageByNumber,
                         int anchorRecordId = 0,
                         bool navigateToAdjacentMatch = false,
                         bool navigationNext = false,
                         int navigateToMatchedPosition = 0);
    void applyPendingSelection();
    void jumpToMatchingId(bool next);
    void jumpToMatchedPosition(int position);

    // ---- 界面状态 ----
    void updatePageInfoLabel();
    void applyRowBackgrounds();
    void selectAndScrollRowById(int recordId);
    void updatePrevNextButtonsEnabled();
    void initLiveLog();

private slots:
    // ---- UI 交互 ----
    void onCheckBoxAllStateChanged(int state);
    void onSearchClicked();
    void onCancelRequested();
    void onHistoryLogClicked(const QModelIndex& index);
    void onLiveLogClicked(const QModelIndex& index);
    void onPreClicked();
    void onNextClicked();
    void onJumpRecordClicked();
    void onPaginationPageChanged(int page);
    void onSetRecordTimeClicked();

    // ---- 查询结果 ----
    void onTargetPageResult(int page);
    void onPendingSelectIdResult(int recordId);
    void onCurrentPageResult(const QList<OperationRecord>& records);
    void onMatchedIdsOnPageResult(const QList<int>& matchedIds);
    void onTotalCountInRangeResult(int totalCount);
    void onTotalMatchedCountResult(int totalCount);
    void onFirstMatchedPositionResult(int position);
    void onTaskFinished(bool success, const QString& message);
    void onRecordInserted(const OperationRecord& record);

private:
    // ---- 内部数据类型 ----
    struct TimeRange
    {
        QString startTime;
        QString endTime;

        bool isEmpty() const { return startTime.isEmpty() && endTime.isEmpty(); }
    };

    struct MatchConditions
    {
        int logType = -1;
        QString keyword;

        bool isEmpty() const { return logType == -1 && keyword.isEmpty(); }
    };

    enum PendingSelect
    {
        PSNone,
        PSId
    };

    struct SelectedRecord
    {
        int id = 0;
        int position = 0;

        void reset()
        {
            id = 0;
            position = 0;
        }

        bool isValid() const { return id > 0; }
    };

    // ---- UI 成员 ----
    Ui::OperationLogWidget* ui;
    WaitDialog* m_waitDialog;

    // ---- 查询条件成员 ----
    TimeRange m_range;
    MatchConditions m_conditions;

    // ---- 分页状态成员 ----
    int m_pageSize;
    int m_currentPage;
    int m_totalPages;

    // ---- 选中状态成员 ----
    PendingSelect m_pendingSelect;
    int m_pendingSelectId;
    QSet<int> m_matchedIdsSet;
    QList<int> m_matchedIdsOrdered;
    QList<int> m_currentPageRecordIds;
    SelectedRecord m_selected;
    int m_totalCountInRange;
    int m_totalMatchedCount;
    int m_firstMatchedPositionOnPage;
    bool m_hasCachedTotalCountInRange;
    bool m_hasCachedTotalMatchedCount;

    // ---- 查询任务状态成员 ----
    QString m_activeTaskId;
    bool m_suppressPaginationSignal;
    bool m_hasHistoryQuery;

    // ---- 容量限制 ----
    static constexpr int kLiveLogMaxRows = 2000;
    static constexpr int kLiveLogTrimBatch = 500;
};

#endif // OPERATIONLOGWIDGET_H
