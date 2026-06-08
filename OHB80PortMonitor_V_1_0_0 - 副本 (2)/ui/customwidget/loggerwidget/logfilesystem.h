#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QSet>
#include "lrucache.h"
#include "pagetable.h"
#include "csvio.h"
#include "historyquery.h"

// ====================================================================
// LogFileSystem —— 基本文件系统层（设计为在 Worker Thread 中运行）
// 使用约定：
//   配置 setter 在 moveToThread 之前调用；
//   此后一切交互通过信号槽（Qt::QueuedConnection）进行。
// ====================================================================
class LogFileSystem : public QObject
{
    Q_OBJECT

public:
    enum class ScrollDirection { Up, Down };

    explicit LogFileSystem(QObject *parent = nullptr);

    // -------- 配置（moveToThread 之前调用）--------
    void setRootPath(const QString &path);
    QString rootPath() const;
    void setPageSize(int size);
    int  pageSize() const;
    void setHeaders(const QStringList &headers);

public slots:
    // 初始化游标（定位到今天文件的最后一页）
    void requestInitialize();
    // 请求上一页
    void requestPrevPage();
    // 请求下一页
    void requestNextPage();
    // 追加一条 JSON 日志到今天的文件
    void requestAppendLog(const QJsonObject &record);
    // 批量追加日志（仅在全部写完后发出一次 pageReady / navigationState）
    void requestAppendBatch(const QVector<QJsonObject> &records);
    // 清理超过 6 个月的年月目录
    void requestCleanOldLogs();
    // 查询历史记录
    void requestQueryHistory(const HistoryQuery &query);
    // 获取所有有日志文件的日期集合
    void requestAvailableDates();

signals:
    // 页数据就绪（isPrev=true 表示是上一页）
    void pageReady(const Page &page, bool isPrev);
    // 加载进度（0~100）
    void loadProgress(int percent);
    // 操作失败
    void loadFailed(const QString &reason);
    // 日志追加结果（pageChanged=true 表示新记录导致了分页，UI 需要全量刷新）
    void logAppended(bool success, bool pageChanged);
    // 游标/导航状态变化（每次翻页后发出）
    void navigationStateChanged(bool hasPrev, bool hasNext,
                                 const QString &currentFile,
                                 int currentPage, int pageCount);
    // 历史查询结果
    void historyReady(const HistoryResult &result);
    // 可用日期集合
    void availableDatesReady(const QSet<QDate> &dates);

private:
    // 内部同步辅助（仅在 worker thread 中调用）
    Page        doReadPage(const QString &filePath, int pageIndex);
    bool        doFlushPage(const PageKey &key);
    const PageTable *getPageTable(const QString &filePath);
    const PageTable *loadPageTable(const QString &filePath);
    Page        loadPageFromDisk(const QString &filePath, int pageIndex,
                                 const PageTable &pt);
    QString     todayFilePath();
    QString     adjacentFilePath(const QString &filePath, ScrollDirection dir) const;
    QStringList allLogFiles() const;
    QStringList allMonthDirs() const;
    QStringList filePathsForDate(const QDate &date) const;
    bool matchesSubQuery(const QStringList &record, const HistoryQuery &query) const;
    QString extractTime(const QStringList &record, int timeCol) const;
    bool        hasPrevInternal() const;
    bool        hasNextInternal();
    void        emitNavigationState(bool isPrev);

    QString     m_rootPath;
    int         m_pageSize = 50;
    QStringList m_headers;

    // 单文件最大记录数（超过后自动轮转到新文件）
    static constexpr int kMaxRecordsPerFile = 50000;

    // 当天日志文件路径缓存
    QString m_todayFilePath;
    int     m_todayFileRecordCount = 0;  // 当前文件已有记录数（内存计数器）

    // 翻页游标（在 worker thread 中维护）
    QString m_currentFile;
    int     m_currentPage = 0;

    // 页表返回缓冲（LogFileSystem 跑在单一 worker thread，无需 thread_local）
    PageTable m_ptBuf;

    LRUCache<PageKey, Page>      m_pageCache{3};
    LRUCache<QString, PageTable> m_ptCache{5};
};
