/*******************************************************************************************
 * @file operationlogwidget.h
 * @author Simon <工号:13> 2026-06-15
 *
 * @class OperationLogWidget
 * @brief 提供运行日志实时显示、历史分页查询和命中记录导航的界面控件。
 *
 * 设计目标:
 *      1. 将实时日志显示和历史日志查询放在同一个业务控件中统一维护。
 *      2. 历史查询只展示数据库层按当前权限筛选后的记录，避免 UI 统计与显示不一致。
 *      3. 登录权限变化后刷新实时日志和已有历史查询，保证界面可见数据及时更新。
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
    // ---- 数据显示 ----
    void setHistoryLogData(const QList<OperationRecord>& data);
    void appendLiveLogRecord(const OperationRecord& record, bool scrollToBottom);
    void reloadLiveLogFromDatabase();
    void refreshByCurrentPermission();

    // ---- 查询任务 ----
    void submitQueryTask(int targetPage, int pendingSelectId = 0);
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

    // ---- 查询任务结果 ----
    void onTargetPageResult(int page);
    void onCurrentPageResult(const QList<OperationRecord>& records);
    void onMatchedIdsOnPageResult(const QList<int>& matchedIds);
    void onTotalCountInRangeResult(int totalCount);
    void onTotalMatchedCountResult(int totalCount);
    void onFirstMatchedPositionResult(int position);
    void onTaskFinished(bool success, const QString& message);
    void onRecordInserted(const OperationRecord& record);

private:
    // ---- 内部数据类型 ----
    struct TimeRange {
        QString startTime;
        QString endTime;
        bool isEmpty() const { return startTime.isEmpty() && endTime.isEmpty(); }
    };

    struct MatchConditions {
        int logType = -1;
        QString keyword;
        bool isEmpty() const { return logType == -1 && keyword.isEmpty(); }
    };

    enum PendingSelect {
        PSNone,
        PSId
    };

    struct SelectedRecord {
        int id = 0;
        int position = 0;
        void reset() { id = 0; position = 0; }
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

    // ---- 跨页选中状态成员 ----
    PendingSelect m_pendingSelect;
    int m_pendingSelectId;
    QSet<int> m_matchedIdsSet;
    QList<int> m_matchedIdsOrdered;
    QList<int> m_currentPageRecordIds;
    SelectedRecord m_selected;
    int m_totalMatchedCount;
    int m_firstMatchedPositionOnPage;

    // ---- 查询任务状态成员 ----
    QString m_activeTaskId;
    bool m_suppressPaginationSignal;
    bool m_hasHistoryQuery;

    // ---- 实时表容量限制 ----
    static constexpr int kLiveLogMaxRows = 2000;
    static constexpr int kLiveLogTrimBatch = 500;
};

#endif // OPERATIONLOGWIDGET_H
