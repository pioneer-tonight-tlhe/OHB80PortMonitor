#include "csvio.h"
#include <QFile>
#include <QTextStream>
#include <QTextCodec>
#include <QDir>
#include <QJsonValue>

// -------- 工具方法 --------

QStringList CsvIO::parseLine(const QString &line)
{
    QStringList result;
    QString field;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        QChar c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    field += '"'; ++i; // 转义引号
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                result.append(field);
                field.clear();
            } else {
                field += c;
            }
        }
    }
    result.append(field);
    return result;
}

QString CsvIO::joinLine(const QStringList &fields)
{
    QStringList escaped;
    for (const QString &f : fields) {
        if (f.contains(',') || f.contains('"') || f.contains('\n')) {
            escaped.append('"' + QString(f).replace('"', "\"\"") + '"');
        } else {
            escaped.append(f);
        }
    }
    return escaped.join(',');
}

QStringList CsvIO::jsonToRow(const QStringList &headers, const QJsonObject &obj)
{
    QStringList row;
    row.reserve(headers.size());
    for (const QString &h : headers) {
        QJsonValue v = obj.value(h);
        if (v.isUndefined() || v.isNull()) {
            row.append(QString());
        } else if (v.isString()) {
            row.append(v.toString());
        } else {
            row.append(v.toVariant().toString());
        }
    }
    return row;
}

// -------- 表头管理 --------

QStringList CsvIO::readHeader(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream in(&f);
    in.setCodec("UTF-8");
    if (in.atEnd()) return {};
    return parseLine(in.readLine());
}

bool CsvIO::writeHeader(const QString &filePath, const QStringList &headers)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream out(&f);
    out.setCodec("UTF-8");
    out << joinLine(headers) << '\n';
    return true;
}

// -------- 记录 I/O --------

bool CsvIO::appendRecord(const QString &filePath,
                         const QStringList &headers,
                         const QJsonObject &record)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    // 文件不存在时自动写入表头
    bool needHeader = !QFile::exists(filePath);
    QFile f(filePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return false;
    QTextStream out(&f);
    out.setCodec("UTF-8");
    if (needHeader) out << joinLine(headers) << '\n';
    out << joinLine(jsonToRow(headers, record)) << '\n';
    return true;
}

QVector<QStringList> CsvIO::getRecords(const QString &filePath, int from, int to)
{
    QVector<QStringList> result;
    if (from > to) return result;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    QTextStream in(&f);
    in.setCodec("UTF-8");
    if (in.atEnd()) return result; // 跳过表头
    in.readLine();
    int idx = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (idx >= from && idx <= to)
            result.append(parseLine(line));
        if (idx > to) break;
        ++idx;
    }
    return result;
}

QVector<QStringList> CsvIO::readAllRecords(const QString &filePath)
{
    QVector<QStringList> result;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return result;
    QTextStream in(&f);
    in.setCodec("UTF-8");
    if (in.atEnd()) return result;
    in.readLine(); // 跳过表头
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (!line.isEmpty())
            result.append(parseLine(line));
    }
    return result;
}

bool CsvIO::modifyRecords(const QString &filePath, int from, int to,
                          const QVector<QStringList> &newRecords)
{
    if (from > to || newRecords.size() != (to - from + 1)) return false;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream in(&f);
    in.setCodec("UTF-8");

    QStringList lines;
    while (!in.atEnd()) lines.append(in.readLine());
    f.close();
    // lines[0] 是表头，records 从 lines[1] 开始
    for (int i = from; i <= to; ++i) {
        int lineIdx = i + 1;
        if (lineIdx < lines.size())
            lines[lineIdx] = joinLine(newRecords[i - from]);
    }
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
    QTextStream out(&f);
    out.setCodec("UTF-8");
    for (const QString &l : lines) out << l << '\n';
    return true;
}

// -------- 页表构建 --------

PageTable CsvIO::buildPageTable(const QString &filePath, int pageSize)
{
    PageTable pt;
    pt.filePath  = filePath;
    pt.pageSize  = pageSize;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return pt;
    QTextStream in(&f);
    in.setCodec("UTF-8");
    if (in.atEnd()) return pt;
    in.readLine(); // 跳过表头

    PageTableEntry entry;
    entry.offset      = f.pos();
    entry.recordCount = 0;
    int recordInPage  = 0;

    while (!in.atEnd()) {
        in.readLine();
        ++recordInPage;
        ++pt.totalRecords;
        if (recordInPage == pageSize) {
            entry.recordCount = recordInPage;
            pt.entries.append(entry);
            entry.offset      = f.pos();
            entry.recordCount = 0;
            recordInPage      = 0;
        }
    }
    // 最后一页（不满 pageSize）
    if (recordInPage > 0) {
        entry.recordCount = recordInPage;
        pt.entries.append(entry);
    }
    return pt;
}

// -------- 计数 --------

int CsvIO::countRecords(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
    QTextStream in(&f);
    in.setCodec("UTF-8");
    if (in.atEnd()) return 0;
    in.readLine(); // 跳过表头
    int count = 0;
    while (!in.atEnd()) { in.readLine(); ++count; }
    return count;
}
