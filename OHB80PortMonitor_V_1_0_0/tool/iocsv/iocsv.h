/*******************************************************************************************
 * @file iocsv.h
 * @author Simon <工号：13> 2026-07-18
 *
 * @class IOCSV
 * @brief 提供 CSV 文件的打开、追加写入、记录读取和记录数查询功能。
 *
 * 设计目标：
 *      1. 打开已有 CSV 文件或自动创建不存在的文件。
 *      2. 使用 UTF-8 编码追加写入一条记录，并处理 CSV 特殊字符转义。
 *      3. 支持按零基索引读取记录和查询文件记录数。
 *******************************************************************************************/
#ifndef IOCSV_H
#define IOCSV_H

#include <QString>
#include <QStringList>
#include <QVector>

class QFile;

class IOCSV
{
public:
    // 创建未绑定文件路径的 CSV 工具对象。
    explicit IOCSV(const QString &filePath = QString());

    // 关闭文件并释放文件句柄。
    ~IOCSV();

    // 打开指定 CSV 文件，不存在时自动创建文件。
    bool open(const QString &filePath = QString());

    // 关闭当前打开的 CSV 文件。
    void close();

    // 以追加方式向 CSV 文件写入一条记录。
    bool appendRow(const QStringList &fields);

    // 读取第 rowIndex 条记录，索引从 0 开始。
    QStringList readRow(int rowIndex) const;

    // 返回 CSV 文件中的记录数量。
    int recordCount() const;

    // 返回当前是否已经成功打开文件。
    bool isOpen() const;

    // 返回最近一次操作失败的错误信息。
    QString lastError() const;

private:
    // ---- CSV 编解码 ----
    // 将一条字段列表编码为 CSV 文本行。
    static QString encodeRow(const QStringList &fields);

    // 解析完整 CSV 文本，支持引号、逗号和换行转义。
    static QVector<QStringList> decodeRows(const QString &content);

    // 设置当前错误信息。
    void setError(const QString &errorMessage) const;

private:
    QString m_filePath;
    QFile *m_file = nullptr;
    mutable QString m_lastError;
};

#endif // IOCSV_H
