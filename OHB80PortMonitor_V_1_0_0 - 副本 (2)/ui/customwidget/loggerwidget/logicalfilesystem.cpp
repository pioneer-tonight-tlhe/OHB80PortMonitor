#include "logicalfilesystem.h"
#include <QCoreApplication>
#include <QDateTime>

// =====================================================================
// 构造
// =====================================================================

LogicalFileSystem::LogicalFileSystem(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<HistoryQuery>("HistoryQuery");
    qRegisterMetaType<HistoryResult>("HistoryResult");
    qRegisterMetaType<QSet<QDate>>("QSet<QDate>");
    qRegisterMetaType<QVector<QJsonObject>>("QVector<QJsonObject>");

    // LogFileSystem 不设 parent，以便 moveToThread 后由 QThread::finished 删除
    m_fs           = new LogFileSystem();
    m_workerThread = new QThread(this);
    m_fs->moveToThread(m_workerThread);

    // worker thread 结束时自动销毁 m_fs
    connect(m_workerThread, &QThread::finished, m_fs, &QObject::deleteLater);

    // 转发：主线程信号 → worker thread slot（跨线程自动建立 QueuedConnection）
    connect(this, &LogicalFileSystem::_requestInitialize,
            m_fs, &LogFileSystem::requestInitialize);
    connect(this, &LogicalFileSystem::_requestPrevPage,
            m_fs, &LogFileSystem::requestPrevPage);
    connect(this, &LogicalFileSystem::_requestNextPage,
            m_fs, &LogFileSystem::requestNextPage);
    connect(this, &LogicalFileSystem::_requestAppendLog,
            m_fs, &LogFileSystem::requestAppendLog);
    connect(this, &LogicalFileSystem::_requestAppendBatch,
            m_fs, &LogFileSystem::requestAppendBatch);
    connect(this, &LogicalFileSystem::_requestCleanOldLogs,
            m_fs, &LogFileSystem::requestCleanOldLogs);
    connect(this, &LogicalFileSystem::_requestQueryHistory,
            m_fs, &LogFileSystem::requestQueryHistory);
    connect(this, &LogicalFileSystem::_requestAvailableDates,
            m_fs, &LogFileSystem::requestAvailableDates);

    // 转发：worker thread 信号 → 本类信号（跨线程，自动 QueuedConnection）
    connect(m_fs, &LogFileSystem::pageReady,    this, &LogicalFileSystem::pageReady);
    connect(m_fs, &LogFileSystem::loadProgress, this, &LogicalFileSystem::loadProgress);
    connect(m_fs, &LogFileSystem::loadFailed,   this, &LogicalFileSystem::loadFailed);
    connect(m_fs, &LogFileSystem::logAppended,  this, &LogicalFileSystem::logAppended);
    connect(m_fs, &LogFileSystem::navigationStateChanged,
            this, &LogicalFileSystem::onNavigationStateChanged);
    connect(m_fs, &LogFileSystem::historyReady,
            this, &LogicalFileSystem::historyReady);
    connect(m_fs, &LogFileSystem::availableDatesReady,
            this, &LogicalFileSystem::availableDatesReady);

    m_workerThread->start();

    // 批量写入防抖定时器
    m_batchTimer = new QTimer(this);
    m_batchTimer->setSingleShot(true);
    m_batchTimer->setInterval(50);
    connect(m_batchTimer, &QTimer::timeout, this, &LogicalFileSystem::flushPendingLogs);

    // 午夜日志清理定时器（在主线程中）
    m_midnightTimer = new QTimer(this);
    m_midnightTimer->setSingleShot(true);
    connect(m_midnightTimer, &QTimer::timeout, this, &LogicalFileSystem::onMidnightCleanup);
    scheduleMidnight();
}

LogicalFileSystem::~LogicalFileSystem()
{
    m_batchTimer->stop();
    // 断开所有发往 m_fs 的信号，阻止新事件入队
    disconnect(this, nullptr, m_fs, nullptr);
    // 清除工作线程事件队列中已积压的待处理事件
    QCoreApplication::removePostedEvents(m_fs);
    // 确保缓冲区中残留的日志被写入磁盘（队列已清空，此调用不会长时间阻塞）
    if (!m_pendingLogs.isEmpty()) {
        QVector<QJsonObject> batch;
        batch.swap(m_pendingLogs);
        QMetaObject::invokeMethod(m_fs, "requestAppendBatch",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(QVector<QJsonObject>, batch));
    }
    m_workerThread->quit();
    m_workerThread->wait();
    // m_fs 已由 QThread::finished 信号触发 deleteLater
}

// -------- 配置 --------

void LogicalFileSystem::setRootPath(const QString &path) { m_fs->setRootPath(path); }
void LogicalFileSystem::setPageSize(int size)             { m_fs->setPageSize(size); }
void LogicalFileSystem::setHeaders(const QStringList &h)  { m_headers = h; m_fs->setHeaders(h); }
QStringList LogicalFileSystem::headers() const            { return m_headers; }

// -------- 初始化 --------

void LogicalFileSystem::initialize()
{
    emit _requestInitialize();
}

// -------- 导航状态（缓存）--------

bool    LogicalFileSystem::hasPrevPage()     const { return m_hasPrev; }
bool    LogicalFileSystem::hasNextPage()     const { return m_hasNext; }
QString LogicalFileSystem::currentFilePath() const { return m_currentFile; }
int     LogicalFileSystem::currentPageIndex() const { return m_currentPage; }
int     LogicalFileSystem::currentPageCount() const { return m_pageCount; }

// -------- 翻页 --------

void LogicalFileSystem::requestPrevPage() { emit _requestPrevPage(); }
void LogicalFileSystem::requestNextPage() { emit _requestNextPage(); }

// -------- 写日志 --------

void LogicalFileSystem::writeLog(const QJsonObject &record)
{
    m_pendingLogs.append(record);
    // 达到批量上限时立即刷新，防止持续高频写入导致防抖定时器永远被重置而内存无限增长
    if (m_pendingLogs.size() >= kMaxPendingBatch) {
        m_batchTimer->stop();
        flushPendingLogs();
    } else {
        // 重置定时器，等待 50ms 无新写入后一次性刷新
        m_batchTimer->start();
    }
}

void LogicalFileSystem::flushPendingLogs()
{
    if (m_pendingLogs.isEmpty()) return;
    QVector<QJsonObject> batch;
    batch.swap(m_pendingLogs);
    emit _requestAppendBatch(batch);
}

// -------- 历史查询 --------

void LogicalFileSystem::queryHistory(const HistoryQuery &query)
{
    emit _requestQueryHistory(query);
}

// -------- 可用日期 --------

void LogicalFileSystem::requestAvailableDates()
{
    emit _requestAvailableDates();
}

// -------- 槽 --------

void LogicalFileSystem::onNavigationStateChanged(bool hasPrev, bool hasNext,
                                                   const QString &file,
                                                   int page, int pageCount)
{
    m_hasPrev     = hasPrev;
    m_hasNext     = hasNext;
    m_currentFile = file;
    m_currentPage = page;
    m_pageCount   = pageCount;
    emit navigationUpdated(hasPrev, hasNext);
}

void LogicalFileSystem::onMidnightCleanup()
{
    emit _requestCleanOldLogs();
    scheduleMidnight();
}

void LogicalFileSystem::scheduleMidnight()
{
    QDateTime now      = QDateTime::currentDateTime();
    QDateTime tomorrow = QDateTime(now.date().addDays(1), QTime(0, 0, 1));
    qint64 ms = now.msecsTo(tomorrow);
    m_midnightTimer->start(static_cast<int>(qMin(ms, (qint64)INT_MAX)));
}
