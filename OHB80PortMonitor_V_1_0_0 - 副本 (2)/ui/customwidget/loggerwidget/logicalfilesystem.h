#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QJsonObject>
#include <QStringList>
#include "logfilesystem.h"

// ====================================================================
// LogicalFileSystem —— 逻辑文件系统层
// 设计思路：
//   将 LogFileSystem moveToThread 到 worker thread，
//   主线程通过转发信号让 LogFileSystem 的 slots 异步执行，
//   LogFileSystem 的结果信号回传到主线程。
//   本类缓存导航状态（hasPrev/hasNext/currentFile/page）供主线程同步读取。
// ====================================================================
class LogicalFileSystem : public QObject
{
    Q_OBJECT
public:
    explicit LogicalFileSystem(QObject *parent = nullptr);
    ~LogicalFileSystem();

    // -------- 配置（initialize 之前调用）--------
    void setRootPath(const QString &path);
    void setPageSize(int size);
    void setHeaders(const QStringList &headers);
    QStringList headers() const;

    // 初始化：定位到今天文件的最后一页（异步，结果通过 pageReady 返回）
    void initialize();

    // -------- 缓存的导航状态（主线程中同步读取）--------
    bool    hasPrevPage()     const;
    bool    hasNextPage()     const;
    QString currentFilePath() const;
    int     currentPageIndex() const;
    int     currentPageCount() const;

    // -------- 异步翻页 --------
    void requestPrevPage();
    void requestNextPage();

    // -------- 写入日志（JSON，异步，结果通过 logAppended 信号返回）--------
    // 日志会被缓冲，短时间内的多条写入合并为一次批量操作后统一刷新 UI
    void writeLog(const QJsonObject &record);

    // -------- 历史查询（异步，结果通过 historyReady 信号返回）--------
    void queryHistory(const HistoryQuery &query);

    // -------- 获取可用日期集合（异步）--------
    void requestAvailableDates();

signals:
    void pageReady(const Page &page, bool isPrev);
    void loadProgress(int percent);
    void loadFailed(const QString &reason);
    void logAppended(bool success, bool pageChanged);
    // 每次翻页后、导航状态（hasPrev/hasNext）更新完毕时发出
    void navigationUpdated(bool hasPrev, bool hasNext);
    // 历史查询结果
    void historyReady(const HistoryResult &result);
    // 可用日期集合
    void availableDatesReady(const QSet<QDate> &dates);

    // 转发信号：主线程 → worker thread（自动建立 QueuedConnection）
    void _requestInitialize();
    void _requestPrevPage();
    void _requestNextPage();
    void _requestAppendLog(const QJsonObject &record);
    void _requestAppendBatch(const QVector<QJsonObject> &records);
    void _requestCleanOldLogs();
    void _requestQueryHistory(const HistoryQuery &query);
    void _requestAvailableDates();

private slots:
    void onNavigationStateChanged(bool hasPrev, bool hasNext,
                                   const QString &file, int page, int pageCount);
    void onMidnightCleanup();
    void flushPendingLogs();

private:
    void scheduleMidnight();

    LogFileSystem *m_fs           = nullptr;
    QThread       *m_workerThread = nullptr;
    QTimer        *m_midnightTimer = nullptr;
    QTimer        *m_batchTimer    = nullptr;
    QVector<QJsonObject> m_pendingLogs;
    static constexpr int kMaxPendingBatch = 200;

    // 缓存的导航状态（由 worker thread 异步更新，主线程只读）
    bool    m_hasPrev     = false;
    bool    m_hasNext     = false;
    QString m_currentFile;
    int     m_currentPage = 0;
    int     m_pageCount   = 0;

    QStringList m_headers;
};
