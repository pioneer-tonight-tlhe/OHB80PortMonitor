#pragma once
#include <QString>
#include <QVector>
#include <QDate>
#include <QMetaType>

// CSV 文件中单条记录的字节区间
struct PageTableEntry {
    qint64 offset;      // 该页第一条记录在文件中的字节偏移（跳过表头行）
    int    recordCount; // 该页实际记录数
};

// 某个 CSV 文件的完整页表
struct PageTable {
    QString  filePath;              // 对应的 CSV 文件绝对路径
    int      pageSize   = 50;       // 每页记录条数（可全局设置）
    int      totalRecords = 0;      // 文件总记录数
    QVector<PageTableEntry> entries;// 每一页的偏移信息，index 即页号
    QDate fileDate;                 // 该文件对应的日期（用于跨日切换）

    int pageCount() const { return entries.size(); }
    bool isValid()  const { return !filePath.isEmpty() && !entries.isEmpty(); }
};

// 页缓存 key = (filePath, pageIndex)
struct PageKey {
    QString filePath;
    int     pageIndex;
    bool operator==(const PageKey &o) const
    { return filePath == o.filePath && pageIndex == o.pageIndex; }
};
inline uint qHash(const PageKey &k, uint seed = 0)
{ return qHash(k.filePath, seed) ^ qHash(k.pageIndex, seed + 1); }

// 一页数据：每条记录以 QStringList 存储（按表头列顺序）
struct Page {
    PageKey         key;
    QVector<QStringList> records; // records[i] = 第 i 条记录的各字段
    bool            dirty = false;
};
Q_DECLARE_METATYPE(Page)
