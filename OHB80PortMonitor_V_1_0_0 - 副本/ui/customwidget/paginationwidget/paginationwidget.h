#ifndef PAGINATIONWIDGET_H
#define PAGINATIONWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class PaginationWidget;
}
QT_END_NAMESPACE

// 分页控件类
class PaginationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PaginationWidget(QWidget *parent = nullptr);
    ~PaginationWidget() override;

    // 设置总页数
    void setTotalPages(int totalPages);
    // 设置当前页
    void setCurrentPage(int currentPage);
    // 获取当前页
    int getCurrentPage() const;
    // 获取总页数
    int getTotalPages() const;

signals:
    // 当前页变化信号
    void currentPageChanged(int page);

private slots:
    // 上一页按钮
    void onButtonPreviousClicked();
    // 下一页按钮
    void onButtonNextClicked();
    // 跳转按钮
    void onButtonJumpClicked();
    // 输入框回车
    void onJumpReturnPressed();

private:
    Ui::PaginationWidget *ui;          // UI界面
    int m_totalPages;                   // 总页数
    int m_currentPage;                  // 当前页

    // 更新UI显示
    void updateUI();
};

#endif // PAGINATIONWIDGET_H
