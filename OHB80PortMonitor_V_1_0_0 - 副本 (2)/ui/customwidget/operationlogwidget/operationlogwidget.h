#ifndef OPERATIONLOGWIDGET_H
#define OPERATIONLOGWIDGET_H

#include <QWidget>
#include <QSet>
#include "operationrecord.h"
#include "dbtypes.h"

namespace Ui {
class OperationLogWidget;
}

class OperationLogQueryTask;
class WaitDialog;
class DateTimeSetDialog;

class OperationLogWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OperationLogWidget(QWidget *parent = nullptr);
    ~OperationLogWidget();

private slots:
    // UI 交互
    void onCheckBoxAllStateChanged(int state);
    void onSearchClicked();
    void onCancelRequested();
    void onHistoryLogClicked(const QModelIndex& index);
    void onLiveLogClicked(const QModelIndex& index);
    void onPreClicked();
    void onNextClicked();
    void onPaginationPageChanged(int page);
    void onSetRecordTimeClicked();

    // 任务结果（来自 OperationLogQueryTask）
    void onTargetPageResult(int page);
    void onCurrentPageResult(const QList<OperationRecord>& records);
    void onMatchedIdsOnPageResult(const QList<int>& matchedIds);
    void onTotalCountInRangeResult(int totalCount);
    void onTotalMatchedCountResult(int totalCount);
    void onFirstMatchedPositionResult(int position);
    void onTaskFinished(bool success, const QString& message);
    void onRecordInserted(const OperationRecord& record);

private:
    Ui::OperationLogWidget *ui;

    // 范围 (range)：决定数据集
    struct TimeRange {
        QString startTime;
        QString endTime;
        bool isEmpty() const { return startTime.isEmpty() && endTime.isEmpty(); }
    };
    TimeRange m_range;

    // 匹配条件 (conditions)：决定哪些记录被高亮、可被 Pre/Next 跟踪
    struct MatchConditions {
        int     logType = -1;          // -1 表示未启用
        QString keyword;               // 空表示未启用
        bool isEmpty() const { return logType == -1 && keyword.isEmpty(); }
    };
    MatchConditions m_conditions;

    // 分页
    int m_pageSize;
    int m_currentPage;
    int m_totalPages;

    // 跨页 Pre / Next 时记录新页加载完成后待选的目标
    enum PendingSelect { PSNone, PSId };
    PendingSelect m_pendingSelect;
    int           m_pendingSelectId;   // 跨页加载完成后选中并滚动到该 id

    // 当前页中满足匹配条件的命中 id 集合（高亮 + 页内顺序导航）
    QSet<int>  m_matchedIdsSet;
    QList<int> m_matchedIdsOrdered;

    // 当前选中记录信息
    struct SelectedRecord {
        int id       = 0;  // 主键 id；0 表示未选中
        int position = 0;  // 在范围+条件结果集中的全局顺序号（1-based）；0 表示未选中
        void reset() { id = 0; position = 0; }
        bool isValid() const { return id > 0; }
    };
    SelectedRecord m_selected;

    // 范围 + 条件总命中数（labelPageInfo 分母）
    int m_totalMatchedCount;
    // 该页首条命中在范围+条件结果集中的全局顺序号（页内基址）
    int m_firstMatchedPositionOnPage;

    // 等待对话框 + 活跃任务
    WaitDialog* m_waitDialog;
    QString     m_activeTaskId;

    // 防止程序性 setCurrentPage 触发循环查询
    bool m_suppressPaginationSignal;

    // 帮助函数
    void setHistoryLogData(const QList<OperationRecord>& data);
    void updatePageInfoLabel();
    void applyRowBackgrounds();
    void selectAndScrollRowById(int recordId);
    void updatePrevNextButtonsEnabled();

    // 提交查询任务：使用 m_range / m_conditions / m_pageSize；
    // targetPage<=0 表示由任务自动定位（仅对有匹配条件时生效，否则第1页）
    void submitQueryTask(int targetPage, int pendingSelectId = 0);
    // 跨页加载完成后选中 pending id
    void applyPendingSelection();
    // 用 anchor 找下一/上一条命中并跨页跳转
    void jumpToMatchingId(bool next);

    // 初始化 live log 表（建立 model + 表头 + 订阅 DBCon::recordInserted）
    void initLiveLog();

    // live log 行数上限（超过后批量裁剪最旧的 kLiveLogTrimBatch 条）
    static constexpr int kLiveLogMaxRows   = 2000;
    static constexpr int kLiveLogTrimBatch = 500;
};

#endif // OPERATIONLOGWIDGET_H
