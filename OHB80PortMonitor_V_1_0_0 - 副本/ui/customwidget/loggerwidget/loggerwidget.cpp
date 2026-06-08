#include "loggerwidget.h"
#include "ui_loggerwidget.h"
#include "loadmoreitemwidget.h"
#include "logpagemodel.h"
#include "logitemdelegate.h"
#include <QScrollBar>
#include <QTimer>
#include <QPushButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include "historycalendardialog.h"

// =====================================================================
// LoggerWidget
// =====================================================================

LoggerWidget::LoggerWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LoggerWidget)
{
    ui->setupUi(this);

    m_lfs       = new LogicalFileSystem(this);
    m_model     = new LogPageModel(this);
    m_histModel = new LogPageModel(this);

    setupLiveTab();
    setupHistoryTab();
    setupConnections();
}

LoggerWidget::~LoggerWidget()
{
    delete ui;
}

void LoggerWidget::setupConnections()
{
    // ---- LogicalFileSystem 信号 ----
    connect(m_lfs, &LogicalFileSystem::pageReady,
            this,  &LoggerWidget::onPageReady);
    connect(m_lfs, &LogicalFileSystem::loadProgress,
            this,  &LoggerWidget::onLoadProgress);
    connect(m_lfs, &LogicalFileSystem::loadFailed,
            this,  &LoggerWidget::onLoadFailed);
    connect(m_lfs, &LogicalFileSystem::logAppended,
            this,  &LoggerWidget::onLogAppended);
    connect(m_lfs, &LogicalFileSystem::navigationUpdated,
            this,  &LoggerWidget::onNavigationUpdated);
    connect(m_lfs, &LogicalFileSystem::historyReady,
            this,  &LoggerWidget::onHistoryReady);
    connect(m_lfs, &LogicalFileSystem::availableDatesReady,
            this,  &LoggerWidget::onAvailableDatesReady);

    // ---- 历史查询按钮 ----
    connect(ui->searchBtn,   &QPushButton::clicked, this, &LoggerWidget::onSearchClicked);
    connect(ui->findPrevBtn, &QPushButton::clicked, this, &LoggerWidget::onFindPrev);
    connect(ui->findNextBtn, &QPushButton::clicked, this, &LoggerWidget::onFindNext);
    connect(ui->selectDateBtn, &QPushButton::clicked, this, &LoggerWidget::onSelectDateClicked);
}

void LoggerWidget::setupLiveTab()
{
    // 实时列表模型 / 委托
    ui->liveListView->setModel(m_model);
    m_delegate = new LogItemDelegate(ui->liveListView);
    ui->liveListView->setItemDelegate(m_delegate);

    // 顶部导航按钮（动态插入 liveLayout 最前面）
    m_topWidget = new LoadMoreItemWidget(LoadMoreItemWidget::Position::Top, ui->liveTab);
    m_topWidget->hide();
    connect(m_topWidget, &LoadMoreItemWidget::clicked,
            this, &LoggerWidget::onTopLoadMoreClicked);
    ui->liveLayout->insertWidget(0, m_topWidget);

    // 底部栏
    m_bottomWidget = new LoadMoreItemWidget(LoadMoreItemWidget::Position::Bottom, ui->liveTab);
    m_bottomWidget->hide();
    connect(m_bottomWidget, &LoadMoreItemWidget::clicked,
            this, &LoggerWidget::onBottomLoadMoreClicked);

    m_jumpToLatestBtn = new QPushButton(tr("⇣ Jump to Latest"), ui->liveTab);
    m_jumpToLatestBtn->setCursor(Qt::PointingHandCursor);
    m_jumpToLatestBtn->hide();
    connect(m_jumpToLatestBtn, &QPushButton::clicked,
            this, &LoggerWidget::onJumpToLatestClicked);

    QHBoxLayout *bottomBar = new QHBoxLayout();
    bottomBar->setContentsMargins(0, 0, 0, 0);
    bottomBar->setSpacing(4);
    bottomBar->addWidget(m_bottomWidget, 1);
    bottomBar->addWidget(m_jumpToLatestBtn, 0);
    ui->liveLayout->addLayout(bottomBar);

    connect(ui->liveListView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &LoggerWidget::onScrollValueChanged);
}

void LoggerWidget::setupHistoryTab()
{
    // 初始化日期显示为今天（只读 QLineEdit）
    setSelectedDate(QDate::currentDate());

    // 初始化时间区间默认值 00:00:00 - 23:59:59
    ui->timeFromHour->setValue(0);
    ui->timeFromMinute->setValue(0);
    ui->timeFromSecond->setValue(0);
    ui->timeToHour->setValue(23);
    ui->timeToMinute->setValue(59);
    ui->timeToSecond->setValue(59);

    // 历史列表模型 / 委托
    ui->histListView->setModel(m_histModel);
    m_histDelegate = new LogItemDelegate(ui->histListView);
    ui->histListView->setItemDelegate(m_histDelegate);

    // 日历对话框
    m_calendarDlg = new HistoryCalendarDialog(this);
    connect(m_calendarDlg, &HistoryCalendarDialog::dateSelected,
            this, [this](const QDate &date) { setSelectedDate(date); });
}

// -------- 配置 --------

void LoggerWidget::setRootPath(const QString &path) { m_lfs->setRootPath(path); }
void LoggerWidget::setPageSize(int size)             { m_lfs->setPageSize(size); }
void LoggerWidget::setHeaders(const QStringList &h)
{
    m_headers = h;
    m_lfs->setHeaders(h);
    m_model->setCsvHeaders(h);
    m_histModel->setCsvHeaders(h);
}

void LoggerWidget::setFormat(const QString &format, const QStringList &args)
{
    m_model->setFormat(format, args);
    m_histModel->setFormat(format, args);
}

void LoggerWidget::setItemStyler(
    std::function<void(const QStringList &record, ItemStyle &style)> fn)
{
    m_stylerFn = fn;
    m_delegate->setStyler(fn);
    if (m_histDelegate) m_histDelegate->setStyler(fn);
    ui->liveListView->viewport()->update();
}

QDate LoggerWidget::selectedDate() const
{
    return QDate::fromString(ui->dateEdit->text(), "yyyy-MM-dd");
}

void LoggerWidget::setSelectedDate(const QDate &d)
{
    ui->dateEdit->setText(d.toString("yyyy-MM-dd"));
}

void LoggerWidget::writeLog(const QJsonObject &record)
{
    m_lfs->writeLog(record);
}

void LoggerWidget::initialize()
{
    m_pendingScroll = ScrollHint::Bottom; // 启动时定位到最新记录
    m_lfs->initialize();
}

// -------- 滚动监听 --------

void LoggerWidget::onScrollValueChanged(int value)
{
    QScrollBar *sb = ui->liveListView->verticalScrollBar();

    // 阈值：约 3 行高度（sizeHintForRow 无效时回退到 60px）
    int rowH = ui->liveListView->sizeHintForRow(0);
    int threshold = (rowH > 0) ? rowH * 3 : 60;

    bool nearTop    = (value <= sb->minimum() + threshold);
    bool nearBottom = (value >= sb->maximum() - threshold);

    // 顶部按钮：接近顶部且有上一页则显示，否则隐藏
    if (!m_loadingPrev) {
        if (nearTop && m_lfs->hasPrevPage()) showTopLoadMore();
        else                                 m_topWidget->hide();
    }

    // 底部按钮：接近底部且有下一页则显示，否则隐藏
    if (!m_loadingNext) {
        if (nearBottom && m_lfs->hasNextPage()) showBottomLoadMore();
        else                                    m_bottomWidget->hide();
    }
}

// -------- 顶部/底部按钮管理（show/hide 替代 insertItem/takeItem）--------

void LoggerWidget::showTopLoadMore()
{
    if (!m_topWidget->isHidden()) return;
    m_topWidget->setIdle();
    m_topWidget->show();
}

void LoggerWidget::removeTopLoadMore()
{
    if (m_loadingPrev) return;
    m_topWidget->hide();
}

void LoggerWidget::showBottomLoadMore()
{
    if (!m_bottomWidget->isHidden()) return;
    m_bottomWidget->setIdle();
    m_bottomWidget->show();
}

void LoggerWidget::removeBottomLoadMore()
{
    if (m_loadingNext) return;
    m_bottomWidget->hide();
}

// -------- 点击加载更多 --------

void LoggerWidget::onTopLoadMoreClicked()
{
    if (m_loadingPrev) return;
    m_loadingPrev   = true;
    m_pendingScroll = ScrollHint::Bottom; // 加载旧页后滚到底部（时间连接处）
    if (m_topWidget) m_topWidget->setLoading();
    m_lfs->requestPrevPage();
}

void LoggerWidget::onBottomLoadMoreClicked()
{
    if (m_loadingNext) return;
    m_loadingNext   = true;
    m_pendingScroll = ScrollHint::Top;    // 加载新页后滚到顶部（从新页开头读）
    if (m_bottomWidget) m_bottomWidget->setLoading();
    m_lfs->requestNextPage();
}

// -------- 进度 --------

void LoggerWidget::onLoadProgress(int percent)
{
    if (m_loadingPrev) m_topWidget->setProgress(percent);
    if (m_loadingNext) m_bottomWidget->setProgress(percent);
}

// -------- 页就绪 --------

void LoggerWidget::onPageReady(const Page &page, bool isPrev)
{
    Q_UNUSED(isPrev)

    // 若非用户主动翻页（即来自 init 或 append 刷新），滚动到底部显示最新记录
    if (!m_loadingPrev && !m_loadingNext)
        m_pendingScroll = ScrollHint::Bottom;

    m_loadingPrev = false;
    m_loadingNext = false;

    // 隐藏导航按钮（由 onNavigationUpdated 根据状态重新显示）
    m_topWidget->hide();
    m_bottomWidget->hide();

    // 更新模型（替换全部记录，不清空，无闪屏）
    m_model->setRecords(page.records);
}

void LoggerWidget::onNavigationUpdated(bool hasPrev, bool hasNext)
{
    Q_UNUSED(hasPrev)
    m_hasNext = hasNext;

    // 先隐藏翻页按钮，等滚动位置确定后再由 onScrollValueChanged 重新决定可见性
    m_topWidget->hide();
    m_bottomWidget->hide();

    // "回到最新"按钮：当前不在最后一页时显示
    m_jumpToLatestBtn->setVisible(hasNext);

    switch (m_pendingScroll) {
    case ScrollHint::Bottom: ui->liveListView->scrollToBottom(); break;
    case ScrollHint::Top:    ui->liveListView->scrollToTop();    break;
    case ScrollHint::None:   break;
    }
    m_pendingScroll = ScrollHint::None;

    // 若滚动值未变（已在顶/底），valueChanged 不会触发，手动补一次检查
    QTimer::singleShot(0, this, [this]() {
        onScrollValueChanged(ui->liveListView->verticalScrollBar()->value());
    });
}

void LoggerWidget::onLogAppended(bool success, bool pageChanged)
{
    Q_UNUSED(pageChanged)
    emit logWritten(success);
    // requestAppendLog 已始终导航到最后一页并通过 pageReady 刷新，
    // 无需额外操作。
}

void LoggerWidget::onJumpToLatestClicked()
{
    m_loadingPrev = false;
    m_loadingNext = false;
    m_pendingScroll = ScrollHint::Bottom;
    m_lfs->initialize();
}

void LoggerWidget::onLoadFailed(const QString &reason)
{
    m_loadingPrev = false;
    m_loadingNext = false;
    m_topWidget->setIdle();
    m_bottomWidget->setIdle();
    Q_UNUSED(reason)
}

// =====================================================================
// 历史查询 Tab
// =====================================================================

void LoggerWidget::onSearchClicked()
{
    HistoryQuery q;
    q.date      = selectedDate();
    q.pageSize  = 50;
    q.pageIndex = 0;

    // 时间区间
    if (ui->timeCheck->isChecked()) {
        int timeCol = m_headers.indexOf(QStringLiteral("时间"));
        if (timeCol < 0) {
            for (int i = 0; i < m_headers.size(); ++i) {
                if (m_headers[i].contains(QStringLiteral("时间"))) {
                    timeCol = i;
                    break;
                }
            }
        }
        q.timeColumnIndex = timeCol;
        q.timeFrom = QString("%1:%2:%3")
                      .arg(ui->timeFromHour->value(), 2, 10, QLatin1Char('0'))
                      .arg(ui->timeFromMinute->value(), 2, 10, QLatin1Char('0'))
                      .arg(ui->timeFromSecond->value(), 2, 10, QLatin1Char('0'));
        q.timeTo   = QString("%1:%2:%3")
                      .arg(ui->timeToHour->value(), 2, 10, QLatin1Char('0'))
                      .arg(ui->timeToMinute->value(), 2, 10, QLatin1Char('0'))
                      .arg(ui->timeToSecond->value(), 2, 10, QLatin1Char('0'));
    }

    // LIKE 模糊匹配
    q.likePattern = ui->likeEdit->text().trimmed();

    m_lastQuery = q;
    m_histIsNewSearch = true;
    m_histCurrentMatchPos = -1;
    m_histMatchIndices.clear();
    m_histPendingScrollRow = -1;
    ui->searchBtn->setEnabled(false);
    ui->searchBtn->setText(tr("Searching..."));
    m_lfs->queryHistory(q);
}

void LoggerWidget::onHistoryReady(const HistoryResult &result)
{
    m_lastResult = result;
    ui->searchBtn->setEnabled(true);
    ui->searchBtn->setText(tr("Search"));

    if (m_histIsNewSearch) {
        bool noRecords = (result.totalRecords == 0);
        bool likeEmpty = (!m_lastQuery.likePattern.isEmpty()
                          && result.matchedGlobalIndices.isEmpty());
        if (noRecords || likeEmpty) {
            ui->findPrevBtn->setEnabled(false);
            ui->findNextBtn->setEnabled(false);
            if (noRecords)
                QMessageBox::information(this, tr("Search Result"), tr("No matching records found"));
            else
                QMessageBox::information(this, tr("Search Result"), tr("No fuzzy match records found"));
            if (noRecords) return;
        }
    }

    // 更新历史模型数据
    m_histModel->setRecords(result.records);
    m_histModel->setHighlightMask(result.highlighted);

    // 首次查询时保存全局匹配索引
    if (m_histIsNewSearch) {
        m_histMatchIndices = result.matchedGlobalIndices;
        // 自动定位到第一个匹配记录
        if (!m_histMatchIndices.isEmpty()) {
            m_histCurrentMatchPos = 0;
            int globalIdx = m_histMatchIndices[0];
            int targetPage = globalIdx / m_lastQuery.pageSize;
            int targetRow  = globalIdx % m_lastQuery.pageSize;
            if (targetPage == result.currentPage) {
                QModelIndex idx = m_histModel->index(targetRow);
                ui->histListView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                ui->histListView->setCurrentIndex(idx);
            } else {
                m_histPendingScrollRow = targetRow;
                onHistoryPageClicked(targetPage);
                return;
            }
        }
    }

    // 延迟滚动（翻页后触发）
    if (m_histPendingScrollRow >= 0 && m_histPendingScrollRow < result.records.size()) {
        QModelIndex idx = m_histModel->index(m_histPendingScrollRow);
        ui->histListView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        ui->histListView->setCurrentIndex(idx);
        m_histPendingScrollRow = -1;
    }

    updateFindButtons();
    rebuildHistoryPageBar();
}

void LoggerWidget::onHistoryPageClicked(int pageIndex)
{
    m_lastQuery.pageIndex = pageIndex;
    m_histIsNewSearch = false;
    ui->searchBtn->setEnabled(false);
    ui->searchBtn->setText(tr("Searching..."));
    m_lfs->queryHistory(m_lastQuery);
}

static const int PAGE_BTN_W = 36;
static const char *PAGE_BTN_NORMAL_SS = "";
static const char *PAGE_BTN_CURRENT_SS =
    "QPushButton { background:#3080E8; color:white; font-weight:bold; border:1px solid #2060C0; border-radius:3px; }";

void LoggerWidget::rebuildHistoryPageBar()
{
    QHBoxLayout *bar = ui->pageBarLayout;
    // 清空旧控件
    while (bar->count() > 0) {
        QLayoutItem *item = bar->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    if (m_lastResult.totalPages <= 1) return;

    int cur   = m_lastResult.currentPage;
    int total = m_lastResult.totalPages;

    auto makeBtn = [&](const QString &text, bool enabled, int page) -> QPushButton* {
        QPushButton *btn = new QPushButton(text);
        btn->setFixedWidth(PAGE_BTN_W);
        btn->setEnabled(enabled);
        if (page == cur)
            btn->setStyleSheet(PAGE_BTN_CURRENT_SS);
        else
            btn->setStyleSheet(PAGE_BTN_NORMAL_SS);
        if (enabled) {
            connect(btn, &QPushButton::clicked, this, [this, page]() {
                onHistoryPageClicked(page);
            });
        }
        return btn;
    };

    auto makeDots = [&](int leftPage, int rightPage) -> QPushButton* {
        QPushButton *dots = new QPushButton("...");
        dots->setFixedWidth(PAGE_BTN_W);
        dots->setCursor(Qt::PointingHandCursor);
        connect(dots, &QPushButton::clicked, this, [this, leftPage, rightPage]() {
            bool ok = false;
            int page = QInputDialog::getInt(this, tr("Go to Page"),
                           tr("Enter page number (%1-%2):").arg(leftPage + 1).arg(rightPage + 1),
                           m_lastResult.currentPage + 1,
                           leftPage + 1, rightPage + 1, 1, &ok);
            if (ok) onHistoryPageClicked(page - 1);
        });
        return dots;
    };

    // 上一页
    bar->addWidget(makeBtn(tr("<"), cur > 0, cur - 1));

    // 页码按钮（当前页附近最多 7 个）
    int start = qMax(0, cur - 3);
    int end   = qMin(total - 1, cur + 3);

    if (start > 0) {
        bar->addWidget(makeBtn("1", true, 0));
        if (start > 1)
            bar->addWidget(makeDots(1, start - 1));
    }
    for (int i = start; i <= end; ++i)
        bar->addWidget(makeBtn(QString::number(i + 1), (i != cur), i));
    if (end < total - 1) {
        if (end < total - 2)
            bar->addWidget(makeDots(end + 1, total - 2));
        bar->addWidget(makeBtn(QString::number(total), true, total - 1));
    }

    // 下一页
    bar->addWidget(makeBtn(tr(">"), cur < total - 1, cur + 1));

    // 页信息标签
    QLabel *info = new QLabel(tr("Page %1/%2, Total %3 records")
                              .arg(cur + 1).arg(total).arg(m_lastResult.totalRecords));
    bar->addWidget(info);
    bar->addStretch();
}

void LoggerWidget::onSelectDateClicked()
{
    ui->selectDateBtn->setEnabled(false);
    ui->selectDateBtn->setText(tr("Loading..."));
    m_lfs->requestAvailableDates();
}

void LoggerWidget::onAvailableDatesReady(const QSet<QDate> &dates)
{
    ui->selectDateBtn->setEnabled(true);
    ui->selectDateBtn->setText(tr("Select Date"));

    m_calendarDlg->setAvailableDates(dates);
    m_calendarDlg->setSelectedDate(selectedDate());
    m_calendarDlg->exec();
}

void LoggerWidget::onFindPrev()
{
    if (m_histMatchIndices.isEmpty()) return;
    if (m_histCurrentMatchPos > 0)
        --m_histCurrentMatchPos;
    else
        m_histCurrentMatchPos = m_histMatchIndices.size() - 1; // 循环
    navigateToGlobalIndex(m_histMatchIndices[m_histCurrentMatchPos]);
    updateFindButtons();
}

void LoggerWidget::onFindNext()
{
    if (m_histMatchIndices.isEmpty()) return;
    if (m_histCurrentMatchPos < m_histMatchIndices.size() - 1)
        ++m_histCurrentMatchPos;
    else
        m_histCurrentMatchPos = 0; // 循环
    navigateToGlobalIndex(m_histMatchIndices[m_histCurrentMatchPos]);
    updateFindButtons();
}

void LoggerWidget::navigateToGlobalIndex(int globalIdx)
{
    int targetPage = globalIdx / m_lastQuery.pageSize;
    int targetRow  = globalIdx % m_lastQuery.pageSize;

    if (targetPage == m_lastResult.currentPage) {
        QModelIndex idx = m_histModel->index(targetRow);
        ui->histListView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
        ui->histListView->setCurrentIndex(idx);
    } else {
        m_histPendingScrollRow = targetRow;
        onHistoryPageClicked(targetPage);
    }
}

void LoggerWidget::updateFindButtons()
{
    bool hasMatches = !m_histMatchIndices.isEmpty();
    ui->findPrevBtn->setEnabled(hasMatches);
    ui->findNextBtn->setEnabled(hasMatches);
}
