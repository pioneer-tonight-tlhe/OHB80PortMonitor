#include "tablewidgetmanager.h"
#include <QTableWidgetItem>

Graph::TableWidgetManager::TableWidgetManager()
{
}

Graph::TableWidgetManager::~TableWidgetManager()
{
}

QTableWidget* Graph::TableWidgetManager::addTable(const QString& name, const QStringList& head, QWidget* parent)
{
    // 如果已存在同名列表，直接返回
    int idx = findIndex(name);
    if (idx >= 0) {
        return m_tables[idx].table;
    }

    MyTableWidget item;
    item.name = name;
    item.head = head;
    item.table = new QTableWidget(parent);

    // 初始化表格：设置列数、表头、所有列按比例均分宽度
    item.table->setColumnCount(head.size());
    item.table->setHorizontalHeaderLabels(head);
    item.table->setRowCount(0);
    item.table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 禁止编辑单元格
    item.table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 隐藏垂直表头
    item.table->verticalHeader()->hide();

    // 默认隐藏
    item.table->hide();

    m_tables.append(item);
    return item.table;
}

QTableWidget* Graph::TableWidgetManager::getTable(const QString& name) const
{
    int idx = findIndex(name);
    if (idx >= 0) {
        return m_tables[idx].table;
    }
    return nullptr;
}

void Graph::TableWidgetManager::updateFirstRow(const QString& name, const QStringList& rowData)
{
    int idx = findIndex(name);
    if (idx < 0) return;

    QTableWidget* table = m_tables[idx].table;
    int colCount = m_tables[idx].head.size();

    // 如果没有行，添加一行
    if (table->rowCount() == 0) {
        table->setRowCount(1);
    }

    // 更新第一行
    Qt::Alignment alignment = m_tables[idx].defaultAlignment;
    for (int col = 0; col < colCount && col < rowData.size(); ++col) {
        QTableWidgetItem* item = table->item(0, col);
        if (item) {
            item->setText(rowData[col]);
            item->setTextAlignment(alignment);
        } else {
            QTableWidgetItem* newItem = new QTableWidgetItem(rowData[col]);
            newItem->setTextAlignment(alignment);
            table->setItem(0, col, newItem);
        }
    }

    m_tables[idx].visible = true;
    table->show();
    adjustTableHeight(table, table->rowCount());
}

void Graph::TableWidgetManager::updateRows(const QString& name, const QVector<QStringList>& rows)
{
    int idx = findIndex(name);
    if (idx < 0) return;

    QTableWidget* table = m_tables[idx].table;
    int colCount = m_tables[idx].head.size();
    int n = rows.size();

    if (n == 0) {
        table->clearContents();
        table->setRowCount(0);
        table->hide();
        return;
    }

    // 补齐行数
    if (table->rowCount() < n) {
        table->setRowCount(n);
    } else if (table->rowCount() > n) {
        table->setRowCount(n);
    }

    // 逐行更新数据
    Qt::Alignment alignment = m_tables[idx].defaultAlignment;
    for (int row = 0; row < n; ++row) {
        const QStringList& rowData = rows[row];
        for (int col = 0; col < colCount && col < rowData.size(); ++col) {
            QTableWidgetItem* item = table->item(row, col);
            if (item) {
                item->setText(rowData[col]);
                item->setTextAlignment(alignment);
            } else {
                QTableWidgetItem* newItem = new QTableWidgetItem(rowData[col]);
                newItem->setTextAlignment(alignment);
                table->setItem(row, col, newItem);
            }
        }
    }

    m_tables[idx].visible = true;
    table->show();
    adjustTableHeight(table, n);
}

void Graph::TableWidgetManager::setVisible(const QString& name, bool visible)
{
    int idx = findIndex(name);
    if (idx >= 0) {
        m_tables[idx].visible = visible;
    }
}

void Graph::TableWidgetManager::setAlignment(const QString& name, Qt::Alignment alignment)
{
    int idx = findIndex(name);
    if (idx >= 0) {
        m_tables[idx].defaultAlignment = alignment;
    }
}

void Graph::TableWidgetManager::hideAll()
{
    for (auto& item : m_tables) {
        item.visible = false;
    }
}

void Graph::TableWidgetManager::syncVisibility()
{
    for (const auto& item : m_tables) {
        if (!item.table) continue;
        if (item.visible) {
            item.table->show();
        } else {
            item.table->hide();
        }
    }
}

int Graph::TableWidgetManager::findIndex(const QString& name) const
{
    for (int i = 0; i < m_tables.size(); ++i) {
        if (m_tables[i].name == name) {
            return i;
        }
    }
    return -1;
}

void Graph::TableWidgetManager::adjustTableHeight(QTableWidget* table, int rowCount)
{
    if (!table || rowCount <= 0) return;

    int headerHeight = table->horizontalHeader()->height();
    int rowHeight = table->rowHeight(0);
    int totalHeight = headerHeight + rowHeight * rowCount + 2;
    table->setFixedHeight(totalHeight);
}
