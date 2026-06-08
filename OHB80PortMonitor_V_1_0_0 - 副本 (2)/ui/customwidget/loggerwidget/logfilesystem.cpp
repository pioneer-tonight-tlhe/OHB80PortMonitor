#include "logfilesystem.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QDateTime>

// =====================================================================
// 构造 / 配置
// =====================================================================

LogFileSystem::LogFileSystem(QObject *parent) : QObject(parent) {}

void LogFileSystem::setRootPath(const QString &path) { m_rootPath = path; }
QString LogFileSystem::rootPath() const               { return m_rootPath; }
void LogFileSystem::setPageSize(int size)              { m_pageSize = qMax(1, size); }
int  LogFileSystem::pageSize() const                   { return m_pageSize; }
void LogFileSystem::setHeaders(const QStringList &h)  { m_headers = h; }

// =====================================================================
// Slots（在 worker thread 中执行）
// =====================================================================

void LogFileSystem::requestInitialize()
{
    emit loadProgress(0);
    m_currentFile = todayFilePath();
    const PageTable *pt = getPageTable(m_currentFile);
    m_currentPage = pt ? qMax(0, pt->pageCount() - 1) : 0;
    emit loadProgress(50);
    if (pt && pt->pageCount() > 0) {
        Page page = loadPageFromDisk(m_currentFile, m_currentPage, *pt);
        PageKey ek; Page ep;
        if (m_pageCache.put(page.key, page, &ek, &ep))
            if (ep.dirty) doFlushPage(ek);
        emit loadProgress(100);
        emit pageReady(page, false);
    } else {
        emit loadProgress(100);
        emit pageReady(Page{}, false);
    }
    emitNavigationState(false);
}

void LogFileSystem::requestPrevPage()
{
    emit loadProgress(0);
    QString targetFile = m_currentFile;
    int     targetPage = m_currentPage;
    if (m_currentPage > 0) {
        targetPage = m_currentPage - 1;
    } else {
        QString prev = adjacentFilePath(m_currentFile, ScrollDirection::Up);
        if (prev.isEmpty()) { emit loadFailed(QStringLiteral("已是第一页")); return; }
        targetFile = prev;
        const PageTable *pt = getPageTable(targetFile);
        targetPage = pt ? qMax(0, pt->pageCount() - 1) : 0;
    }
    emit loadProgress(50);
    Page page = doReadPage(targetFile, targetPage);
    m_currentFile = targetFile;
    m_currentPage = targetPage;
    emit loadProgress(100);
    emit pageReady(page, true);
    emitNavigationState(true);
}

void LogFileSystem::requestNextPage()
{
    emit loadProgress(0);
    QString targetFile = m_currentFile;
    int     targetPage = m_currentPage;
    const PageTable *ptCur = getPageTable(m_currentFile);
    if (ptCur && m_currentPage < ptCur->pageCount() - 1) {
        targetPage = m_currentPage + 1;
    } else {
        QString next = adjacentFilePath(m_currentFile, ScrollDirection::Down);
        if (next.isEmpty()) { emit loadFailed(QStringLiteral("已是最后一页")); return; }
        targetFile = next;
        targetPage = 0;
    }
    emit loadProgress(50);
    Page page = doReadPage(targetFile, targetPage);
    m_currentFile = targetFile;
    m_currentPage = targetPage;
    emit loadProgress(100);
    emit pageReady(page, false);
    emitNavigationState(false);
}

void LogFileSystem::requestAppendLog(const QJsonObject &record)
{
    QString path = todayFilePath();

    // 记录追加前的页表状态
    const PageTable *oldPt = getPageTable(path);
    int oldPageCount = oldPt ? oldPt->pageCount() : 0;
    bool wasOnLastPage = (m_currentFile == path
                          && oldPageCount > 0
                          && m_currentPage == oldPageCount - 1);

    bool ok = CsvIO::appendRecord(path, m_headers, record);
    if (!ok) { emit logAppended(false, false); return; }

    ++m_todayFileRecordCount;

    // 失效旧缓存
    if (oldPageCount > 0)
        m_pageCache.remove({path, oldPageCount - 1});
    m_ptCache.remove(path);

    // 重建页表
    const PageTable *newPt = getPageTable(path);
    int newPageCount = newPt ? newPt->pageCount() : 0;
    bool pageChanged = !wasOnLastPage || (newPageCount != oldPageCount);

    // 始终导航到最后一页，确保写入后立即可见
    int lastPage = qMax(0, newPageCount - 1);
    m_currentFile = path;
    m_currentPage = lastPage;

    Page page = doReadPage(path, lastPage);
    emit pageReady(page, false);
    emitNavigationState(false);
    emit logAppended(true, pageChanged);
}

void LogFileSystem::requestAppendBatch(const QVector<QJsonObject> &records)
{
    if (records.isEmpty()) return;

    QString path = todayFilePath();
    bool anyOk = false;

    for (const QJsonObject &record : records) {
        bool ok = CsvIO::appendRecord(path, m_headers, record);
        if (ok) anyOk = true;
    }

    m_todayFileRecordCount += records.size();

    if (!anyOk) {
        emit logAppended(false, false);
        return;
    }

    // 失效旧缓存并重建页表
    m_pageCache.remove({path, m_currentPage});
    m_ptCache.remove(path);

    const PageTable *newPt = getPageTable(path);
    int newPageCount = newPt ? newPt->pageCount() : 0;
    int lastPage = qMax(0, newPageCount - 1);
    m_currentFile = path;
    m_currentPage = lastPage;

    Page page = doReadPage(path, lastPage);
    emit pageReady(page, false);
    emitNavigationState(false);
    emit logAppended(true, true);
}

void LogFileSystem::requestCleanOldLogs()
{
    QStringList months = allMonthDirs();
    const int kKeep = 6;
    if (months.size() <= kKeep) return;
    QStringList toDelete = months.mid(0, months.size() - kKeep);
    QDir root(m_rootPath);
    for (const QString &month : toDelete) {
        QString monthPath = root.filePath(month);
        QDirIterator it(monthPath, {"*.csv"}, QDir::Files);
        while (it.hasNext()) {
            QString fp = it.next();
            PageTable pt;
            if (m_ptCache.get(fp, pt)) {
                for (int i = 0; i < pt.pageCount(); ++i)
                    m_pageCache.remove({fp, i});
                m_ptCache.remove(fp);
            }
        }
        QDir(monthPath).removeRecursively();
    }
}

// =====================================================================
// 历史查询
// =====================================================================

QStringList LogFileSystem::filePathsForDate(const QDate &date) const
{
    QStringList result;
    QString monthDir = QDir(m_rootPath).filePath(date.toString("yyyyMM"));
    QDir dir(monthDir);
    if (!dir.exists()) return result;
    QString dayPrefix = date.toString("dd");
    QStringList files = dir.entryList({dayPrefix + "_*.csv"}, QDir::Files);
    files.sort();
    for (const QString &f : files)
        result.append(dir.absoluteFilePath(f));
    return result;
}

QString LogFileSystem::extractTime(const QStringList &record, int timeCol) const
{
    if (timeCol < 0 || timeCol >= record.size()) return {};
    const QString &val = record[timeCol];
    // 支持 "yyyy-MM-dd HH:mm:ss" 或纯 "HH:mm:ss"
    int spaceIdx = val.indexOf(' ');
    return (spaceIdx >= 0) ? val.mid(spaceIdx + 1) : val;
}

bool LogFileSystem::matchesSubQuery(const QStringList &record, const HistoryQuery &query) const
{
    // LIKE 模糊匹配（遍历所有字段）
    if (!query.likePattern.isEmpty()) {
        for (const QString &field : record) {
            if (field.contains(query.likePattern, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }
    return true;
}

void LogFileSystem::requestQueryHistory(const HistoryQuery &query)
{
    HistoryResult result;

    // 1. 找到该日期的所有文件
    QStringList files = filePathsForDate(query.date);
    if (files.isEmpty()) {
        emit historyReady(result);
        return;
    }

    // 2. 读取该日期的所有记录
    QVector<QStringList> allRecords;
    for (const QString &fp : files)
        allRecords.append(CsvIO::readAllRecords(fp));

    bool hasTimeRange = query.timeColumnIndex >= 0
                        && !query.timeFrom.isEmpty()
                        && !query.timeTo.isEmpty();

    bool hasSubQuery = !query.likePattern.isEmpty();

    // ---- 按时间区间过滤（可选）----
    QVector<QStringList> &source = allRecords;
    QVector<QStringList> filtered;
    if (hasTimeRange) {
        for (const QStringList &rec : allRecords) {
            QString t = extractTime(rec, query.timeColumnIndex);
            if (t >= query.timeFrom && t <= query.timeTo)
                filtered.append(rec);
        }
        source = filtered;
    }

    // ---- 计算全局匹配索引（基于过滤后数据集）----
    if (hasSubQuery) {
        for (int i = 0; i < source.size(); ++i) {
            if (matchesSubQuery(source[i], query))
                result.matchedGlobalIndices.append(i);
        }
    }

    // ---- 分页 ----
    result.totalRecords = source.size();
    result.totalPages   = (source.isEmpty()) ? 0
                          : (source.size() + query.pageSize - 1) / query.pageSize;
    result.currentPage  = qBound(0, query.pageIndex, qMax(0, result.totalPages - 1));

    int from = result.currentPage * query.pageSize;
    int to   = qMin(from + query.pageSize, source.size());
    for (int i = from; i < to; ++i) {
        result.records.append(source[i]);
        bool hl = hasSubQuery && matchesSubQuery(source[i], query);
        result.highlighted.append(hl);
    }

    emit historyReady(result);
}

void LogFileSystem::requestAvailableDates()
{
    QSet<QDate> dates;
    QStringList files = allLogFiles();
    for (const QString &fp : files) {
        QFileInfo fi(fp);
        QString monthDirName = fi.dir().dirName();           // "yyyyMM"
        QString dayStr       = fi.baseName().left(2);        // "dd"
        QDate d = QDate::fromString(monthDirName + dayStr, "yyyyMMdd");
        if (d.isValid()) dates.insert(d);
    }
    emit availableDatesReady(dates);
}

// =====================================================================
// 内部页操作
// =====================================================================

Page LogFileSystem::doReadPage(const QString &filePath, int pageIndex)
{
    PageKey key{filePath, pageIndex};
    Page cached;
    if (m_pageCache.get(key, cached)) return cached;
    const PageTable *pt = getPageTable(filePath);
    if (!pt || pageIndex < 0 || pageIndex >= pt->pageCount()) return Page{};
    Page page = loadPageFromDisk(filePath, pageIndex, *pt);
    PageKey ek; Page ep;
    if (m_pageCache.put(key, page, &ek, &ep))
        if (ep.dirty) doFlushPage(ek);
    return page;
}

bool LogFileSystem::doFlushPage(const PageKey &key)
{
    Page page;
    if (!m_pageCache.get(key, page) || !page.dirty) return true;
    const PageTable *pt = getPageTable(key.filePath);
    if (!pt || key.pageIndex >= pt->pageCount()) return false;
    int from = key.pageIndex * pt->pageSize;
    int to   = from + pt->entries[key.pageIndex].recordCount - 1;
    bool ok  = CsvIO::modifyRecords(key.filePath, from, to, page.records);
    if (ok) { page.dirty = false; m_pageCache.put(key, page); }
    return ok;
}

const PageTable *LogFileSystem::getPageTable(const QString &filePath)
{
    if (m_ptCache.get(filePath, m_ptBuf)) return &m_ptBuf;
    return loadPageTable(filePath);
}

const PageTable *LogFileSystem::loadPageTable(const QString &filePath)
{
    PageTable pt = CsvIO::buildPageTable(filePath, m_pageSize);
    pt.fileDate  = QDate::fromString(
        QFileInfo(filePath).dir().dirName() + QFileInfo(filePath).baseName().left(2),
        "yyyyMMdd");
    QString evictedPath; PageTable evictedPt;
    if (m_ptCache.put(filePath, pt, &evictedPath, &evictedPt)) {
        for (int i = 0; i < evictedPt.pageCount(); ++i) {
            doFlushPage({evictedPath, i});
            m_pageCache.remove({evictedPath, i});
        }
    }
    m_ptCache.get(filePath, m_ptBuf);
    return &m_ptBuf;
}

Page LogFileSystem::loadPageFromDisk(const QString &filePath, int pageIndex,
                                     const PageTable &pt)
{
    int from = pageIndex * pt.pageSize;
    int to   = from + pt.entries[pageIndex].recordCount - 1;
    Page page;
    page.key     = {filePath, pageIndex};
    page.records = CsvIO::getRecords(filePath, from, to);
    page.dirty   = false;
    return page;
}

// =====================================================================
// 文件路径工具
// =====================================================================

QString LogFileSystem::todayFilePath()
{
    QDate today = QDate::currentDate();

    // 已缓存且仍是当天
    if (!m_todayFilePath.isEmpty()) {
        QDate cached = QDate::fromString(
            QFileInfo(m_todayFilePath).dir().dirName()
            + QFileInfo(m_todayFilePath).baseName().left(2), "yyyyMMdd");
        if (cached == today) {
            if (m_todayFileRecordCount < kMaxRecordsPerFile)
                return m_todayFilePath;
            // 文件已满 → 立即创建新文件（用毫秒精度避免文件名碰撞）
            QString monthDir = QFileInfo(m_todayFilePath).absolutePath();
            m_todayFilePath = QDir(monthDir).filePath(
                QDateTime::currentDateTime().toString("dd_HHmmsszzz") + ".csv");
            if (!m_headers.isEmpty())
                CsvIO::writeHeader(m_todayFilePath, m_headers);
            m_todayFileRecordCount = 0;
            return m_todayFilePath;
        }
        m_todayFilePath.clear(); // 跨天：清除缓存，重新查找
    }

    // 在今天的年月目录中查找已存在的当天文件
    QString monthDir  = QDir(m_rootPath).filePath(today.toString("yyyyMM"));
    QString dayPrefix = today.toString("dd");
    QDir    dir(monthDir);
    QStringList existing = dir.entryList({dayPrefix + "_*.csv"}, QDir::Files);
    existing.sort(); // 按文件名字母序升序，最后一个即最晚创建的

    if (!existing.isEmpty()) {
        QString lastFile = dir.absoluteFilePath(existing.last());
        int count = CsvIO::countRecords(lastFile);
        if (count < kMaxRecordsPerFile) {
            // 复用今天最近的文件，不破坏已有数据
            m_todayFilePath = lastFile;
            m_todayFileRecordCount = count;
            return m_todayFilePath;
        }
        // 最近的文件已满 → 创建新文件
    }

    // 今天尚无文件或所有文件已满，新建
    QDir().mkpath(monthDir);
    m_todayFilePath = QDir(monthDir).filePath(
        QDateTime::currentDateTime().toString("dd_HHmmsszzz") + ".csv");
    if (!m_headers.isEmpty())
        CsvIO::writeHeader(m_todayFilePath, m_headers);
    m_todayFileRecordCount = 0;

    return m_todayFilePath;
}

QString LogFileSystem::adjacentFilePath(const QString &filePath, ScrollDirection dir) const
{
    QStringList all = allLogFiles();
    int idx = all.indexOf(filePath);
    if (idx < 0) return {};
    if (dir == ScrollDirection::Up   && idx > 0)            return all[idx - 1];
    if (dir == ScrollDirection::Down && idx < all.size()-1) return all[idx + 1];
    return {};
}

QStringList LogFileSystem::allLogFiles() const
{
    QStringList files;
    QDirIterator it(m_rootPath, {"*.csv"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) files.append(it.next());
    files.sort();
    return files;
}

QStringList LogFileSystem::allMonthDirs() const
{
    QDir root(m_rootPath);
    QStringList dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    dirs.sort();
    return dirs;
}

// =====================================================================
// 导航状态
// =====================================================================

bool LogFileSystem::hasPrevInternal() const
{
    if (m_currentPage > 0) return true;
    return !adjacentFilePath(m_currentFile, ScrollDirection::Up).isEmpty();
}

bool LogFileSystem::hasNextInternal()
{
    const PageTable *pt = getPageTable(m_currentFile);
    if (pt && m_currentPage < pt->pageCount() - 1) return true;
    return !adjacentFilePath(m_currentFile, ScrollDirection::Down).isEmpty();
}

void LogFileSystem::emitNavigationState(bool /*isPrev*/)
{
    const PageTable *pt = getPageTable(m_currentFile);
    int pageCount = pt ? pt->pageCount() : 0;
    emit navigationStateChanged(hasPrevInternal(), hasNextInternal(),
                                 m_currentFile, m_currentPage, pageCount);
}
