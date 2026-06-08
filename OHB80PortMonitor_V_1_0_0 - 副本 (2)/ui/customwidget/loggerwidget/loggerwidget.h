#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QStringList>
#include <functional>
#include "itemstyle.h"
#include "logicalfilesystem.h"
#include "historyquery.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LoggerWidget; }
QT_END_NAMESPACE

class QPushButton;
class LogPageModel;
class LogItemDelegate;
class LoadMoreItemWidget;
class HistoryCalendarDialog;

// ====================================================================
// LoggerWidget —— 主界面组件
// ====================================================================
class LoggerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LoggerWidget(QWidget *parent = nullptr);
    ~LoggerWidget();

    // -------- 配置接口 --------
    void setRootPath(const QString &path);
    void setPageSize(int size);
    void setHeaders(const QStringList &headers);
    void setFormat(const QString &format, const QStringList &args);
    void setItemStyler(std::function<void(const QStringList &record, ItemStyle &style)> fn);

    // -------- 写入一条日志 --------
    void writeLog(const QJsonObject &record);

    // -------- 初始化（配置完成后调用） --------
    void initialize();

signals:
    void logWritten(bool success);

private slots:
    // ---- 实时 Tab ----
    void onPageReady(const Page &page, bool isPrev);
    void onLoadProgress(int percent);
    void onLoadFailed(const QString &reason);
    void onScrollValueChanged(int value);
    void onTopLoadMoreClicked();
    void onBottomLoadMoreClicked();
    void onLogAppended(bool success, bool pageChanged);
    void onNavigationUpdated(bool hasPrev, bool hasNext);
    void onJumpToLatestClicked();

    // ---- 历史查询 Tab ----
    void onSearchClicked();
    void onHistoryReady(const HistoryResult &result);
    void onHistoryPageClicked(int pageIndex);
    void onSelectDateClicked();
    void onAvailableDatesReady(const QSet<QDate> &dates);
    void onFindPrev();
    void onFindNext();

private:
    void setupConnections();
    void setupLiveTab();
    void setupHistoryTab();
    void rebuildHistoryPageBar();
    void navigateToGlobalIndex(int globalIdx);
    void updateFindButtons();
    QDate selectedDate() const;
    void  setSelectedDate(const QDate &d);

    // 顶部/底部加载按钮管理
    void showTopLoadMore();
    void removeTopLoadMore();
    void showBottomLoadMore();
    void removeBottomLoadMore();

    Ui::LoggerWidget *ui = nullptr;

    LogicalFileSystem  *m_lfs         = nullptr;

    // ---- 实时 Tab 成员 ----
    LogPageModel       *m_model       = nullptr;
    LogItemDelegate    *m_delegate    = nullptr;
    std::function<void(const QStringList &record, ItemStyle &style)> m_stylerFn;

    LoadMoreItemWidget *m_topWidget    = nullptr;
    LoadMoreItemWidget *m_bottomWidget = nullptr;
    QPushButton        *m_jumpToLatestBtn = nullptr;

    QStringList m_headers;
    bool        m_loadingPrev = false;
    bool        m_loadingNext = false;
    bool        m_hasNext     = false;

    enum class ScrollHint { None, Top, Bottom };
    ScrollHint  m_pendingScroll = ScrollHint::None;

    // ---- 历史查询 Tab 成员 ----
    HistoryCalendarDialog *m_calendarDlg = nullptr;
    LogPageModel       *m_histModel    = nullptr;
    LogItemDelegate    *m_histDelegate = nullptr;

    HistoryQuery  m_lastQuery;
    HistoryResult m_lastResult;
    bool          m_histIsNewSearch = false;
    QVector<int>  m_histMatchIndices;
    int           m_histCurrentMatchPos = -1;
    int           m_histPendingScrollRow = -1;
};
