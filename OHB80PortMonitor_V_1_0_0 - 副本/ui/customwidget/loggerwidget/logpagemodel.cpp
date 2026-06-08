#include "logpagemodel.h"

LogPageModel::LogPageModel(QObject *parent) : QAbstractListModel(parent) {}

void LogPageModel::setCsvHeaders(const QStringList &headers)
{
    m_csvHeaders = headers;
    rebuildSlots();
}

void LogPageModel::setFormat(const QString &format, const QStringList &args)
{
    m_format     = format;
    m_formatArgs = args;
    rebuildSlots();
}

void LogPageModel::rebuildSlots()
{
    m_slots.clear();
    if (m_format.isEmpty() || m_formatArgs.isEmpty()) return;

    // 闭合字符集：用于区分前一字段的 suffix 与后一字段的 prefix
    static const QString kClose = QStringLiteral("])}>");

    QStringList parts = m_format.split("{}");
    int n = qMin(parts.size() - 1, m_formatArgs.size());

    QString nextPrefix = parts[0]; // 第一个字段的 prefix = 格式串开头
    for (int i = 0; i < n; ++i) {
        FieldSlot slot;
        slot.colIndex = m_csvHeaders.indexOf(m_formatArgs[i]);
        slot.prefix   = nextPrefix;

        // 段间文本 = 前字段 suffix + 后字段 prefix（按闭合字符分割）
        const QString inter = (i + 1 < parts.size()) ? parts[i + 1] : QString();
        int slen = 0;
        while (slen < inter.size() && kClose.contains(inter[slen])) ++slen;
        slot.suffix = inter.left(slen);
        nextPrefix  = inter.mid(slen);

        m_slots.append(slot);
    }
    // 尾部剩余文本追加到最后一个字段的 suffix
    if (!m_slots.isEmpty())
        m_slots.last().suffix += nextPrefix;

    beginResetModel();
    endResetModel();
}

void LogPageModel::setRecords(const QVector<QStringList> &records)
{
    beginResetModel();
    m_records = records;
    m_highlightMask.clear();
    endResetModel();
}

void LogPageModel::setHighlightMask(const QVector<bool> &mask, const QColor &color)
{
    m_highlightMask  = mask;
    m_highlightColor = color;
    if (!m_records.isEmpty())
        emit dataChanged(index(0), index(m_records.size() - 1), {Qt::BackgroundRole});
}

const QStringList &LogPageModel::recordAt(int row) const
{
    return m_records.at(row);
}

int LogPageModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_records.size();
}

QVariant LogPageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_records.size()) return QVariant();
    if (role == Qt::DisplayRole) return formatRow(m_records[index.row()]);
    if (role == Qt::BackgroundRole) {
        if (index.row() < m_highlightMask.size() && m_highlightMask[index.row()])
            return m_highlightColor;
    }
    return QVariant();
}

QString LogPageModel::formatRow(const QStringList &record) const
{
    if (!m_slots.isEmpty()) {
        // 按格式槽渲染：空字段跳过整个 prefix+value+suffix
        QString result;
        for (const FieldSlot &slot : m_slots) {
            if (slot.colIndex < 0 || slot.colIndex >= record.size()) continue;
            const QString val = record[slot.colIndex].trimmed();
            if (val.isEmpty()) continue;
            result += slot.prefix + val + slot.suffix;
        }
        return result;
    }

    // 未设置格式时的默认显示：非空字段用 []，最后一个用 ：
    QStringList fields;
    for (const QString &f : record)
        if (!f.trimmed().isEmpty()) fields.append(f);
    if (fields.isEmpty()) return QString();
    QString result;
    for (int i = 0; i < fields.size(); ++i)
        result += (i < fields.size()-1) ? '[' + fields[i] + ']'
                                        : QStringLiteral("：") + fields[i];
    return result;
}
