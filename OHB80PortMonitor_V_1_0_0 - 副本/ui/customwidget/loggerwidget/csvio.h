#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include "pagetable.h"

// CSV 文件 I/O 接口
// 文件格式：第一行为逗号分隔的表头，后续每行为一条记录（字段值中逗号用双引号转义）
class CsvIO
{
public:
    // -------- 表头管理 --------

    // 读取表头（文件不存在时返回空列表）
    static QStringList readHeader(const QString &filePath);

    // 写入表头（会清空文件）
    static bool writeHeader(const QString &filePath, const QStringList &headers);

    // -------- 记录 I/O --------

    // 接口1：追加一条记录（JSON key 对应表头列，多余 key 忽略，缺失 key 填空）
    static bool appendRecord(const QString &filePath,
                             const QStringList &headers,
                             const QJsonObject &record);

    // 接口2：获取第 [from, to] 条记录（0-based，闭区间）
    //        利用 PageTableEntry 中的 offset 定位，避免全文扫描
    static QVector<QStringList> getRecords(const QString &filePath,
                                           int from, int to);

    // 接口2b：读取文件中的所有记录（跳过表头）
    static QVector<QStringList> readAllRecords(const QString &filePath);

    // 接口3：将第 [from, to] 条记录替换为 newRecords（长度需一致）
    static bool modifyRecords(const QString &filePath,
                              int from, int to,
                              const QVector<QStringList> &newRecords);

    // -------- 页表构建 --------

    // 扫描整个 CSV 文件，生成页表（记录每页首条的字节偏移和页内记录数）
    static PageTable buildPageTable(const QString &filePath, int pageSize);

    // -------- 工具方法 --------

    // 把 QJsonObject 序列化为按 headers 顺序的 QStringList
    static QStringList jsonToRow(const QStringList &headers, const QJsonObject &obj);

    // CSV 单行解析（处理带引号的字段）
    static QStringList parseLine(const QString &line);

    // QStringList 转 CSV 行（字段含逗号/引号时加双引号）
    static QString joinLine(const QStringList &fields);

    // 计数文件总记录数（不含表头）
    static int countRecords(const QString &filePath);
};
