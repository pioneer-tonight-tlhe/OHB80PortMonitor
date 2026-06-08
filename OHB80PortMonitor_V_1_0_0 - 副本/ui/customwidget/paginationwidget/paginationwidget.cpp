#include "paginationwidget.h"
#include "ui_paginationwidget.h"
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QIntValidator>
#include <QSet>
#include <QList>
#include <algorithm>

PaginationWidget::PaginationWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PaginationWidget)
    , m_totalPages(1)
    , m_currentPage(1)
{
    ui->setupUi(this);

    // 整体样式（仅作用于本控件作用域内的按钮 / 标签）
    this->setStyleSheet(R"(
        QPushButton {
            font-size: 12px;
            padding: 2px 6px;
            min-height: 28px;
            min-width: 28px;
        }
        QLabel#labelPageInfo {
            font-size: 12px;
            padding: 0 6px;
        }
    )");

    connect(ui->buttonPrevious, &QPushButton::clicked, this, &PaginationWidget::onButtonPreviousClicked);
    connect(ui->buttonNext,     &QPushButton::clicked, this, &PaginationWidget::onButtonNextClicked);
    connect(ui->buttonJump,     &QPushButton::clicked, this, &PaginationWidget::onButtonJumpClicked);
    connect(ui->lineEditJump, &QLineEdit::returnPressed, this, &PaginationWidget::onJumpReturnPressed);

    ui->lineEditJump->setValidator(new QIntValidator(1, 1, this));

    updateUI();
}

PaginationWidget::~PaginationWidget()
{
    delete ui;
}

void PaginationWidget::setTotalPages(int totalPages)
{
    if (totalPages < 1) {
        totalPages = 1;
    }
    m_totalPages = totalPages;

    if (m_currentPage > m_totalPages) {
        m_currentPage = m_totalPages;
    }
    updateUI();
}

void PaginationWidget::setCurrentPage(int currentPage)
{
    if (currentPage < 1) {
        currentPage = 1;
    }
    if (currentPage > m_totalPages) {
        currentPage = m_totalPages;
    }
    if (m_currentPage != currentPage) {
        m_currentPage = currentPage;
        updateUI();
        emit currentPageChanged(m_currentPage);
    }
}

int PaginationWidget::getCurrentPage() const
{
    return m_currentPage;
}

int PaginationWidget::getTotalPages() const
{
    return m_totalPages;
}

void PaginationWidget::onButtonPreviousClicked()
{
    setCurrentPage(m_currentPage - 1);
}

void PaginationWidget::onButtonNextClicked()
{
    setCurrentPage(m_currentPage + 1);
}

void PaginationWidget::onButtonJumpClicked()
{
    const int page = ui->lineEditJump->text().toInt();
    if (page >= 1 && page <= m_totalPages) {
        setCurrentPage(page);
    }
}

void PaginationWidget::onJumpReturnPressed()
{
    onButtonJumpClicked();
}

// 生成要显示的页码集合（按规则收集，排序去重）。
//   - 总页数 ≤ 8：全部显示
//   - 否则：始终显示 {1, 2, 3} ∪ {current..current+4} ∪ {last}
// 结果按升序返回。
static QList<int> collectPagesToShow(int currentPage, int totalPages)
{
    QSet<int> set;
    if (totalPages <= 8) {
        for (int p = 1; p <= totalPages; ++p) {
            set.insert(p);
        }
    } else {
        // 首部：1, 2, 3
        for (int p = 1; p <= 3; ++p) {
            set.insert(p);
        }
        // 当前页及其后 4 页
        for (int p = currentPage; p <= currentPage + 4 && p <= totalPages; ++p) {
            if (p >= 1) set.insert(p);
        }
        // 末页
        set.insert(totalPages);
    }
    QList<int> pages = set.values();
    std::sort(pages.begin(), pages.end());
    return pages;
}

void PaginationWidget::updateUI()
{
    // labelPageInfo: "PageX/Y"
    ui->labelPageInfo->setText(QString("Page%1/%2").arg(m_currentPage).arg(m_totalPages));

    // < / > 按钮可用性
    ui->buttonPrevious->setEnabled(m_currentPage > 1);
    ui->buttonNext->setEnabled(m_currentPage < m_totalPages);

    // 同步输入框范围并显示当前页
    if (QIntValidator *v = qobject_cast<QIntValidator *>(const_cast<QValidator *>(ui->lineEditJump->validator())))
        v->setRange(1, m_totalPages);
    ui->lineEditJump->setText(QString::number(m_currentPage));

    // 清空动态页码按钮
    QLayout* btnLayout = ui->pageButtonsContainerLayout;
    while (QLayoutItem* item = btnLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const QList<int> pages = collectPagesToShow(m_currentPage, m_totalPages);

    // 当前页按钮样式：(P) 圆括号 + 蓝底白字 + 不可点击
    const QString currentStyle =
        "QPushButton { font-size: 12px; font-weight: bold; padding: 2px 4px;"
        " min-height: 28px; min-width: 32px;"
        " background-color: #4a90d9; color: white;"
        " border: 1px solid #2a70b9; border-radius: 3px; }";
    const QString normalStyle =
        "QPushButton { font-size: 12px; padding: 2px 4px;"
        " min-height: 28px; min-width: 28px; }";
    const QString ellipsisStyle =
        "QLabel { font-size: 12px; padding: 0 2px; min-width: 16px; }";

    int prev = 0;
    for (int i = 0; i < pages.size(); ++i) {
        const int p = pages.at(i);
        // 不连续 → 先插入 "..." 标签
        if (i > 0 && p > prev + 1) {
            QLabel* dots = new QLabel("...", ui->pageButtonsContainer);
            dots->setAlignment(Qt::AlignCenter);
            dots->setStyleSheet(ellipsisStyle);
            btnLayout->addWidget(dots);
        }

        if (p == m_currentPage) {
            QPushButton* btn = new QPushButton(QString("(%1)").arg(p), ui->pageButtonsContainer);
            btn->setEnabled(false);
            btn->setStyleSheet(currentStyle);
            btnLayout->addWidget(btn);
        } else {
            QPushButton* btn = new QPushButton(QString::number(p), ui->pageButtonsContainer);
            btn->setStyleSheet(normalStyle);
            connect(btn, &QPushButton::clicked, this, [this, p]() {
                setCurrentPage(p);
            });
            btnLayout->addWidget(btn);
        }

        prev = p;
    }
}
