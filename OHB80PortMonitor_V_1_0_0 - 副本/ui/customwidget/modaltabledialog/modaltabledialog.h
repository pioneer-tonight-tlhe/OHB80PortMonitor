#ifndef MODAL_TABLE_DIALOG_H
#define MODAL_TABLE_DIALOG_H

#include <QColor>
#include <QList>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QDialogButtonBox;
class QTableWidget;

// 独立顶层 QWidget（非 QDialog）。
// 使用 show() 显示，绝无嵌套事件循环；不强制模态，可与主窗口并存。
class ModalTableDialog : public QWidget
{
    Q_OBJECT

public:
    explicit ModalTableDialog(QWidget *parent = nullptr);
    explicit ModalTableDialog(const QString &title,
                              const QStringList &headers,
                              const QList<QStringList> &rows,
                              QWidget *parent = nullptr);
    explicit ModalTableDialog(const QString &title,
                              const QStringList &headers,
                              const QVector<QStringList> &rows,
                              QWidget *parent = nullptr);

    void setDialogTitle(const QString &title);
    void setHeaders(const QStringList &headers);
    void setRows(const QList<QStringList> &rows);
    void setRows(const QVector<QStringList> &rows);
    void setTableData(const QStringList &headers, const QList<QStringList> &rows);
    void setTableData(const QStringList &headers, const QVector<QStringList> &rows);
    void appendRow(const QStringList &row);
    void setCellTextColor(int row, int column, const QColor &color);
    void setFieldTextColor(const QString &header, const QString &fieldValue, const QColor &color);

    QTableWidget *tableWidget() const;

    // 非阻塞、非模态显示工厂方法。
    // 内部使用 QWidget::show()，不涉及任何事件循环或模态机制，
    // 因此完全不会阻塞主界面，可与主窗口并存。
    // 返回的对象由 Qt 在关闭时自动删除（设置了 Qt::WA_DeleteOnClose），
    // 调用方可在返回后继续做样式定制（如 setFieldTextColor），但不应保存指针长期使用。
    static ModalTableDialog *showAsync(QWidget *parent,
                                       const QString &title,
                                       const QStringList &headers,
                                       const QList<QStringList> &rows);

private:
    void initUI();
    void ensureColumnCount(int columnCount);
    void applyHeaders();
    void setRowData(int row, const QStringList &rowData);
    int columnIndexOf(const QString &header) const;

private:
    QTableWidget *m_table = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QStringList m_headers;
};

#endif // MODAL_TABLE_DIALOG_H
