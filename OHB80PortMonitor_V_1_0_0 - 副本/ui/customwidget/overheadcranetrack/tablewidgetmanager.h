#ifndef TABLEWIDGETMANAGER_H
#define TABLEWIDGETMANAGER_H

#include <QTableWidget>
#include <QHeaderView>
#include <QStringList>
#include <QVector>
#include <QString>

namespace Graph {

struct MyTableWidget
{
    QTableWidget* table = nullptr;
    QStringList head;
    QString name;
    bool visible = false;
    Qt::Alignment defaultAlignment = Qt::AlignLeft;
};

class TableWidgetManager
{
public:
    explicit TableWidgetManager();
    ~TableWidgetManager();

    // 添加一个列表（指定名称、表头、父控件）
    QTableWidget* addTable(const QString& name, const QStringList& head, QWidget* parent = nullptr);

    // 根据列表名称获取一个列表，未找到返回 nullptr
    QTableWidget* getTable(const QString& name) const;

    // 更新第一行数据（默认更新第一行，如果没有就添加一行）
    // rowData 的长度应与表头列数一致
    void updateFirstRow(const QString& name, const QStringList& rowData);

    // 更新前 n 行数据（如果不足 n 行就补齐）
    // rows: 每一行的数据，rows.size() 即为 n
    void updateRows(const QString& name, const QVector<QStringList>& rows);

    // 获取所有列表数量
    int count() const { return m_tables.size(); }

    // 设置指定表格的 visible 标记
    void setVisible(const QString& name, bool visible);

    // 设置指定表格的文本对齐方式（左对齐、右对齐、居中）
    void setAlignment(const QString& name, Qt::Alignment alignment);

    // 将所有表格的 visible 标记设为 false
    void hideAll();

    // 根据每个表格的 visible 标记，同步显示/隐藏所有表格
    void syncVisibility();

private:
    // 根据名称查找索引，未找到返回 -1
    int findIndex(const QString& name) const;

    // 自动调整表格高度
    void adjustTableHeight(QTableWidget* table, int rowCount);

    QVector<MyTableWidget> m_tables;
};

}

#endif // TABLEWIDGETMANAGER_H
