#pragma once
#include <QAbstractListModel>
#include <QStringList>
#include <QVector>
#include <QColor>

// ====================================================================
// LogPageModel —— 单页日志数据模型
// 显示格式：[字段1][字段2]...：最后一个字段
// ====================================================================
class LogPageModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit LogPageModel(QObject *parent = nullptr);

    // CSV 列头（与 record 列顺序对应）
    void setCsvHeaders(const QStringList &headers);
    // 显示格式：格式字符串 + 各 {} 对应的表头名
    // 例： setFormat("[{}][{}][{}]:{}", {"level","time","name","msg"})
    void setFormat(const QString &format, const QStringList &args);
    void setRecords(const QVector<QStringList> &records);
    void setHighlightMask(const QVector<bool> &mask, const QColor &color = QColor(173, 216, 230));
    const QStringList &recordAt(int row) const;
    int     rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    // 将一条 CSV 记录按格式字符串渲染为显示文本（公开供历史查询 Tab 复用）
    QString formatRow(const QStringList &record) const;

private:
    struct FieldSlot {
        int     colIndex; // 在 CSV record 中的列索引（-1 表示未找到）
        QString prefix;
        QString suffix;
    };
    void    rebuildSlots();

    QStringList          m_csvHeaders;
    QString              m_format;
    QStringList          m_formatArgs;
    QList<FieldSlot>     m_slots;
    QVector<QStringList> m_records;
    QVector<bool>        m_highlightMask;
    QColor               m_highlightColor;
};
