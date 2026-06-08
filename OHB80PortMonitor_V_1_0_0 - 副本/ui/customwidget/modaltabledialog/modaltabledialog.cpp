#include "modaltabledialog.h"

#include <QAbstractItemView>
#include <QBrush>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QScroller>
#include <QScrollerProperties>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ModalTableDialog::ModalTableDialog(QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    initUI();
}

ModalTableDialog::ModalTableDialog(const QString &title,
                                   const QStringList &headers,
                                   const QList<QStringList> &rows,
                                   QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    initUI();
    setDialogTitle(title);
    setTableData(headers, rows);
}

ModalTableDialog::ModalTableDialog(const QString &title,
                                   const QStringList &headers,
                                   const QVector<QStringList> &rows,
                                   QWidget *parent)
    : QWidget(parent, Qt::Window)
{
    initUI();
    setDialogTitle(title);
    setTableData(headers, rows);
}

void ModalTableDialog::initUI()
{
    setWindowTitle(tr("Table"));
    // 隐藏最小化按钮，只保留关闭按钮
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMinimizeButtonHint);
    resize(720, 480);

    m_table = new QTableWidget(this);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->horizontalHeader()->setStretchLastSection(true);

    // 启用触屏滚动
    QScroller::grabGesture(m_table->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller *scroller = QScroller::scroller(m_table->viewport());
    if (scroller) {
        QScrollerProperties props = scroller->scrollerProperties();
        props.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
        props.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootAlwaysOff);
        props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.01);
        scroller->setScrollerProperties(props);
    }

    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    // 不再依赖 QDialog::accept()，OK 按钮直接关闭窗口（配合 WA_DeleteOnClose 自动销毁）
    connect(m_buttonBox, &QDialogButtonBox::accepted, this, &QWidget::close);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(m_table);
    layout->addWidget(m_buttonBox);
}

void ModalTableDialog::setDialogTitle(const QString &title)
{
    setWindowTitle(title);
}

void ModalTableDialog::setHeaders(const QStringList &headers)
{
    m_headers = headers;
    ensureColumnCount(m_headers.size());
    applyHeaders();
}

void ModalTableDialog::setRows(const QList<QStringList> &rows)
{
    int columnCount = m_headers.size();
    for (const QStringList &row : rows) {
        columnCount = qMax(columnCount, row.size());
    }

    ensureColumnCount(columnCount);
    applyHeaders();

    m_table->setRowCount(rows.size());
    for (int row = 0; row < rows.size(); ++row) {
        setRowData(row, rows.at(row));
    }
}

void ModalTableDialog::setRows(const QVector<QStringList> &rows)
{
    QList<QStringList> listRows;
    listRows.reserve(rows.size());
    for (const QStringList &row : rows) {
        listRows.append(row);
    }
    setRows(listRows);
}

void ModalTableDialog::setTableData(const QStringList &headers, const QList<QStringList> &rows)
{
    m_headers = headers;
    setRows(rows);
}

void ModalTableDialog::setTableData(const QStringList &headers, const QVector<QStringList> &rows)
{
    m_headers = headers;
    setRows(rows);
}

void ModalTableDialog::appendRow(const QStringList &row)
{
    ensureColumnCount(qMax(m_table->columnCount(), row.size()));
    applyHeaders();

    const int rowIndex = m_table->rowCount();
    m_table->insertRow(rowIndex);
    setRowData(rowIndex, row);
}

void ModalTableDialog::setCellTextColor(int row, int column, const QColor &color)
{
    if (row < 0 || row >= m_table->rowCount()
        || column < 0 || column >= m_table->columnCount()) {
        return;
    }

    QTableWidgetItem *item = m_table->item(row, column);
    if (item) {
        item->setForeground(QBrush(color));
    }
}

void ModalTableDialog::setFieldTextColor(const QString &header,
                                         const QString &fieldValue,
                                         const QColor &color)
{
    const int column = columnIndexOf(header);
    if (column < 0) {
        return;
    }

    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, column);
        if (item && item->text() == fieldValue) {
            item->setForeground(QBrush(color));
        }
    }
}

QTableWidget *ModalTableDialog::tableWidget() const
{
    return m_table;
}

ModalTableDialog *ModalTableDialog::showAsync(QWidget *parent,
                                              const QString &title,
                                              const QStringList &headers,
                                              const QList<QStringList> &rows)
{
    auto *dialog = new ModalTableDialog(parent);
    // 关闭时自动销毁，无需调用方管理生命周期
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setDialogTitle(title);
    dialog->setTableData(headers, rows);
    // 纯非阻塞显示：QWidget::show() 不涉及任何事件循环/模态机制，
    // 调用立即返回；窗口与主窗口并存，用户可同时操作两者。
    dialog->show();
    return dialog;
}

void ModalTableDialog::ensureColumnCount(int columnCount)
{
    m_table->setColumnCount(qMax(0, columnCount));
}

void ModalTableDialog::applyHeaders()
{
    if (m_table->columnCount() == 0) {
        return;
    }

    QStringList labels = m_headers;
    while (labels.size() < m_table->columnCount()) {
        labels.append(QString("Column %1").arg(labels.size() + 1));
    }

    m_table->setHorizontalHeaderLabels(labels);
}

void ModalTableDialog::setRowData(int row, const QStringList &rowData)
{
    for (int column = 0; column < m_table->columnCount(); ++column) {
        const QString text = column < rowData.size() ? rowData.at(column) : QString();
        auto *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, column, item);
    }
}

int ModalTableDialog::columnIndexOf(const QString &header) const
{
    for (int column = 0; column < m_headers.size(); ++column) {
        if (m_headers.at(column) == header) {
            return column;
        }
    }
    return -1;
}
