#include "operationlogwidget.h"
#include "ui_operationlogwidget.h"
#include "waitdialog.h"
#include "datetimesetdialog.h"

#include <QStandardItemModel>
#include <QHeaderView>
#include <QColor>
#include <QDebug>
#include <QTableView>
#include <QTimer>
#include <QScroller>
#include <QScrollerProperties>
#include <QMessageBox>

#include "scheduler/scheduler.h"
#include "scheduler/tasks/operationlogquerytask.h"
#include "scheduler/tasks/operation_dispatch_task.h"
#include "app/shareddata.h"
#include "usermanager.h"
#include "paginationwidget.h"
#include "databasemanager.h"
#include "operationlogdb/operationlogdbcon.h"

namespace {
// 日志类型文字颜色
// - Warn：警报黄色
// - Error / Fatal：参考 ScrollingTipLabel 警报模式的深红色（#c92a2a）
// - 其余：返回无效 QColor，调用方据此保持默认文字色
QColor logTypeForegroundColor(int logType)
{
    switch (logType) {
        case static_cast<int>(LogDB::OperationLogType::Warn):
            return QColor(QStringLiteral("#f5a623"));
        case static_cast<int>(LogDB::OperationLogType::Error):
        case static_cast<int>(LogDB::OperationLogType::Fatal):
            return QColor(QStringLiteral("#c92a2a"));
        default:
            return QColor(); // invalid
    }
}

// 给一行三个 item 统一设置前景色（仅当 color 有效时）
void applyLogTypeForeground(QStandardItem* itemTime,
                            QStandardItem* itemLogType,
                            QStandardItem* itemDesc,
                            int logType)
{
    const QColor fg = logTypeForegroundColor(logType);
    if (!fg.isValid()) return;
    const QBrush brush(fg);
    if (itemTime)    itemTime->setForeground(brush);
    if (itemLogType) itemLogType->setForeground(brush);
    if (itemDesc)    itemDesc->setForeground(brush);
}
} // namespace

OperationLogWidget::OperationLogWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OperationLogWidget)
    , m_pageSize(500)
    , m_currentPage(1)
    , m_totalPages(0)
    , m_pendingSelect(PSNone)
    , m_pendingSelectId(0)
    , m_totalMatchedCount(0)
    , m_firstMatchedPositionOnPage(0)
    , m_waitDialog(nullptr)
    , m_suppressPaginationSignal(false)
{
    ui->setupUi(this);

    connect(ui->checkBoxAll, &QCheckBox::stateChanged,
            this, &OperationLogWidget::onCheckBoxAllStateChanged);
    connect(ui->pushButtonSearch, &QPushButton::clicked,
            this, &OperationLogWidget::onSearchClicked);
    connect(ui->pushButtonPre, &QPushButton::clicked,
            this, &OperationLogWidget::onPreClicked);
    connect(ui->pushButtonNext, &QPushButton::clicked,
            this, &OperationLogWidget::onNextClicked);
    connect(ui->pushButtonSetRecordTime, &QPushButton::clicked,
            this, &OperationLogWidget::onSetRecordTimeClicked);
    // 表格列表：禁止编辑、按行选择；用 selectionModel::currentRowChanged 响应用户选中动作
    ui->tableViewHistoryLog->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableViewHistoryLog->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableViewHistoryLog->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableViewHistoryLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewHistoryLog->verticalHeader()->setVisible(false);
    connect(ui->tableViewHistoryLog, &QTableView::clicked,
            this, &OperationLogWidget::onHistoryLogClicked);

    connect(ui->widgetPaginate, &PaginationWidget::currentPageChanged,
            this, &OperationLogWidget::onPaginationPageChanged);

    // 初始状态下未查询，Pre / Next 不可用
    updatePrevNextButtonsEnabled();

    // 初始化日志类型下拉框
    const auto logTypes = LogDB::operationLogTypeList();
    for (const auto& item : logTypes) {
        ui->comboBoxLogType->addItem(item.first, item.second);
    }

    initLiveLog();

    // history log 表：最后一列拉伸充满剩余宽度
    ui->tableViewHistoryLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewHistoryLog->verticalHeader()->setVisible(false);

    // 启用触摸/鼠标拖动滚动手势（支持触屏滑动表格）同时设置滚动条默认 hover 色
    auto enableTouchScroll = [](QAbstractItemView* view) {
        if (!view) return;
        QScroller::grabGesture(view->viewport(), QScroller::LeftMouseButtonGesture);
        QScroller* scroller = QScroller::scroller(view->viewport());
        QScrollerProperties props = scroller->scrollerProperties();
        props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.005);
        props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.3);
        props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
        scroller->setScrollerProperties(props);
        // 滚动条 handle 默认即为 hover 色，方便用户看到滚动位置
        const QString scrollHandleStyle =
            "QScrollBar::handle:vertical{background:#D4D0C8;}"
            "QScrollBar::handle:horizontal{background:#D4D0C8;}";
        view->setStyleSheet(view->styleSheet() + scrollHandleStyle);
    };
    enableTouchScroll(ui->tableViewLiveLog);
    enableTouchScroll(ui->tableViewHistoryLog);

    // 连接 live log 点击信号，用于显示 description 完整内容
    connect(ui->tableViewLiveLog, &QTableView::clicked,
            this, &OperationLogWidget::onLiveLogClicked);
}

void OperationLogWidget::initLiveLog()
{
    // 表头与历史查询表保持一致（除 id 列——live log 无法提供）
    auto* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({"Occur Time", "Log Type", "Description"});

    ui->tableViewLiveLog->setModel(model);
    ui->tableViewLiveLog->setModel(model);
    ui->tableViewLiveLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewLiveLog->verticalHeader()->setVisible(false);
    ui->tableViewLiveLog->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置列宽：确保时间字段完整显示
    ui->tableViewLiveLog->setColumnWidth(0, 180);  // Occur Time

    // 订阅 OperationDispatchTask 的即时信号，实现实时显示
    if (auto* opTask = SharedData::getOperationDispatchTask()) {
        // 补播：取出本控件订阅信号之前已派发的最近日志，避免启动早期日志（如
        // NetworkStatusTask::start() 中的 [WriteQRCode] 日志）因时序丢失。
        // 必须在 connect 之前取，避免在补播与新增信号之间造成重复或漏包。
        const QList<OperationRecord> backlog = opTask->recentRecords();
        for (const OperationRecord& rec : backlog) {
            onRecordInserted(rec);
        }

        connect(opTask, &OperationDispatchTask::operationLogInserted,
                this, &OperationLogWidget::onRecordInserted, Qt::QueuedConnection);
    }
}

void OperationLogWidget::onRecordInserted(const OperationRecord& record)
{
    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewLiveLog->model());
    if (!model) return;

    // 权限过滤：仅允许查看不高于当前用户权限的记录
    const int currentPerm = static_cast<int>(UserManager::instance()->currentPermission());
    if (record.userPermission > currentPerm) {
        return;
    }

    const QString logTypeText = LogDB::operationLogTypeName(record.logType);

    auto* itemTime    = new QStandardItem(record.occurTime);
    auto* itemLogType = new QStandardItem(logTypeText);
    auto* itemDesc    = new QStandardItem(record.description);
    applyLogTypeForeground(itemTime, itemLogType, itemDesc, record.logType);

    // 设置文本对齐：除 Description 字段（第 2 列）外，其他字段居中
    itemTime->setTextAlignment(Qt::AlignCenter);
    itemLogType->setTextAlignment(Qt::AlignCenter);

    QList<QStandardItem*> items{ itemTime, itemLogType, itemDesc };

    // 按时间顺序追加到末尾（最新在底部）
    model->appendRow(items);
    // 行数超过上限时，一次性批量裁剪掉最顶部的 kLiveLogTrimBatch 条最旧记录
    if (model->rowCount() > kLiveLogMaxRows) {
        model->removeRows(0, kLiveLogTrimBatch);
    }

    // 自动滚动到底部（最新日志）
    ui->tableViewLiveLog->scrollToBottom();
}

OperationLogWidget::~OperationLogWidget()
{
    delete ui;
}

void OperationLogWidget::onCheckBoxAllStateChanged(int state)
{
    bool checked = (state == Qt::Checked);
    ui->checkBoxLogType->setChecked(checked);
    ui->checkBoxRecordTime->setChecked(checked);
}

// 弹出对话框设置时间范围；仅更新 UI 与内部状态，不主动提交查询任务。
// 用户需自行点击 Search 按钮触发查询。
void OperationLogWidget::onSetRecordTimeClicked()
{
    DateTimeSetDialog dialog(this);
    dialog.setStartTime(ui->lineEditRecordStartTime->text());
    dialog.setEndTime(ui->lineEditRecordEndTime->text());

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString startText = dialog.isStartTimeEnabled() ? dialog.getStartTime() : QString();
    const QString endText   = dialog.isEndTimeEnabled()   ? dialog.getEndTime()   : QString();
    ui->lineEditRecordStartTime->setText(startText);
    ui->lineEditRecordEndTime->setText(endText);

    // 用户在对话框中启用了时间 → 自动勾选 checkBoxRecordTime；都未启用 → 取消勾选
    const bool anyEnabled = !startText.isEmpty() || !endText.isEmpty();
    ui->checkBoxRecordTime->setChecked(anyEnabled);

    if (anyEnabled) {
        m_range.startTime = startText;
        m_range.endTime   = endText;
    } else {
        m_range.startTime.clear();
        m_range.endTime.clear();
    }

    // 不在此提交任务；后续 Search 触发时会使用更新后的 m_range
}

// 点击 Search：把 LogType + Keyword 同步到 m_conditions；时间范围保持不变。
// 让任务自动定位首条命中所在页。
void OperationLogWidget::onSearchClicked()
{
    m_conditions = MatchConditions();
    if (ui->checkBoxLogType->isChecked()) {
        m_conditions.logType = ui->comboBoxLogType->currentData().toInt();
    }
    m_conditions.keyword = ui->lineEditKeyword->text().trimmed();

    // 同步 range（受 checkBoxRecordTime 控制）
    if (ui->checkBoxRecordTime->isChecked()) {
        const QString startText = ui->lineEditRecordStartTime->text().trimmed();
        const QString endText   = ui->lineEditRecordEndTime->text().trimmed();
        m_range.startTime = startText;
        m_range.endTime   = endText;
    } else {
        m_range.startTime.clear();
        m_range.endTime.clear();
    }

    m_selected.reset();
    m_totalMatchedCount          = 0;
    m_firstMatchedPositionOnPage = 0;
    updatePageInfoLabel();

    // targetPage=0 → 任务自动定位首条命中所在页（无匹配条件时回退到第 1 页）
    submitQueryTask(0, 0);
}

void OperationLogWidget::submitQueryTask(int targetPage, int pendingSelectId)
{
    if (!m_waitDialog) {
        m_waitDialog = new WaitDialog(this);
        connect(m_waitDialog, &WaitDialog::cancelRequested,
                this, &OperationLogWidget::onCancelRequested);
    }
    m_waitDialog->setWaiting(tr("Querying, please wait..."));
    m_waitDialog->show();
    m_waitDialog->raise();
    m_waitDialog->activateWindow();

    m_matchedIdsSet.clear();
    m_matchedIdsOrdered.clear();
    m_pendingSelect   = (pendingSelectId > 0) ? PSId : PSNone;
    m_pendingSelectId = pendingSelectId;
    updatePrevNextButtonsEnabled();

    auto* task = new OperationLogQueryTask();
    task->setPageSize(m_pageSize);
    if (targetPage > 0) {
        task->setTargetPage(targetPage);
    }
    if (!m_range.isEmpty()) {
        task->setTimeRange(m_range.startTime, m_range.endTime);
    }
    if (m_conditions.logType != -1) {
        task->setLogType(m_conditions.logType);
    }
    if (!m_conditions.keyword.isEmpty()) {
        task->setSearchKey(m_conditions.keyword);
    }

    connect(task, &OperationLogQueryTask::targetPageResult,
            this, &OperationLogWidget::onTargetPageResult, Qt::QueuedConnection);
    connect(task, &OperationLogQueryTask::currentPageResult,
            this, &OperationLogWidget::onCurrentPageResult, Qt::QueuedConnection);
    connect(task, &OperationLogQueryTask::matchedIdsOnPageResult,
            this, &OperationLogWidget::onMatchedIdsOnPageResult, Qt::QueuedConnection);
    connect(task, &OperationLogQueryTask::totalCountInRangeResult,
            this, &OperationLogWidget::onTotalCountInRangeResult, Qt::QueuedConnection);
    connect(task, &OperationLogQueryTask::totalMatchedCountResult,
            this, &OperationLogWidget::onTotalMatchedCountResult, Qt::QueuedConnection);
    connect(task, &OperationLogQueryTask::firstMatchedPositionResult,
            this, &OperationLogWidget::onFirstMatchedPositionResult, Qt::QueuedConnection);
    connect(task, &SchedulerTask::finished,
            this, &OperationLogWidget::onTaskFinished, Qt::QueuedConnection);

    m_activeTaskId = Scheduler::instance()->submitTask(task);
}

void OperationLogWidget::applyPendingSelection()
{
    if (m_pendingSelect != PSId || m_pendingSelectId <= 0) {
        return;
    }
    if (!m_matchedIdsSet.contains(m_pendingSelectId)) {
        return; // 等其他信号都到位再尝试
    }
    if (m_firstMatchedPositionOnPage <= 0) {
        return;
    }
    const int idx = m_matchedIdsOrdered.indexOf(m_pendingSelectId);
    if (idx < 0) {
        return;
    }
    m_selected.id       = m_pendingSelectId;
    m_selected.position = m_firstMatchedPositionOnPage + idx;
    m_pendingSelect     = PSNone;
    m_pendingSelectId   = 0;

    selectAndScrollRowById(m_selected.id);
    applyRowBackgrounds();
    updatePageInfoLabel();
    updatePrevNextButtonsEnabled();
}

void OperationLogWidget::onCancelRequested()
{
    if (!m_activeTaskId.isEmpty()) {
        Scheduler::instance()->cancelTask(m_activeTaskId);
        m_activeTaskId.clear();
    }
    if (m_waitDialog) {
        m_waitDialog->hide();
    }
}

void OperationLogWidget::onTargetPageResult(int page)
{
    if (page <= 0) page = 1;
    m_currentPage = page;
    m_suppressPaginationSignal = true;
    ui->widgetPaginate->setCurrentPage(page);
    m_suppressPaginationSignal = false;
}

void OperationLogWidget::onPaginationPageChanged(int page)
{
    if (m_suppressPaginationSignal) {
        return;
    }
    m_selected.reset();
    submitQueryTask(page, 0);
}

void OperationLogWidget::onCurrentPageResult(const QList<OperationRecord>& records)
{
    setHistoryLogData(records);
}

void OperationLogWidget::onMatchedIdsOnPageResult(const QList<int>& matchedIds)
{
    m_matchedIdsSet.clear();
    m_matchedIdsOrdered.clear();
    for (int id : matchedIds) {
        if (id > 0) {
            m_matchedIdsSet.insert(id);
            m_matchedIdsOrdered.append(id);
        }
    }

    // 默认选中本页首条命中记录（仅当未选中且无显式 pending 目标时）。
    // 适用场景：Search / Set 时间范围 / 分页翻页 —— 这些路径都会先 reset 选中，
    // 然后期望加载完成后自动落在本页首条命中上。
    // Pre/Next 跨页跳转走 submitQueryTask(_, matchedId) 设置 pendingSelectId，
    // 此时 m_pendingSelect == PSId，不会被覆盖。
    //
    // 立即生效（蓝底高亮 + 滚动 + labelPageInfo）：
    //   - m_selected.id 立即赋值
    //   - position 暂用页内序号 (idx+1) 作为占位，等 firstMatchedPositionResult
    //     到达后会被 onFirstMatchedPositionResult 校正为全局序号。
    if (m_pendingSelect == PSNone
        && m_selected.id == 0
        && !m_matchedIdsOrdered.isEmpty()) {
        const int firstId = m_matchedIdsOrdered.first();
        m_selected.id       = firstId;
        m_selected.position = (m_firstMatchedPositionOnPage > 0)
                                  ? m_firstMatchedPositionOnPage
                                  : 1; // 占位值，待 firstMatchedPositionResult 校正
        selectAndScrollRowById(firstId);
    }

    applyRowBackgrounds();
    applyPendingSelection();
    updatePageInfoLabel();
    updatePrevNextButtonsEnabled();
}

void OperationLogWidget::onTotalCountInRangeResult(int totalCount)
{
    const int pageSize = (m_pageSize > 0) ? m_pageSize : 1;
    m_totalPages = (totalCount + pageSize - 1) / pageSize;
    ui->widgetPaginate->setTotalPages(m_totalPages);
}

void OperationLogWidget::onTotalMatchedCountResult(int totalCount)
{
    m_totalMatchedCount = totalCount;
    updatePageInfoLabel();
    updatePrevNextButtonsEnabled();
}

void OperationLogWidget::onFirstMatchedPositionResult(int position)
{
    m_firstMatchedPositionOnPage = position;
    applyPendingSelection();

    // 默认选中场景：在 onMatchedIdsOnPageResult 已立即用占位序号赋值 m_selected.position；
    // firstMatchedPosition 到达后用全局序号校正。
    if (position > 0 && m_selected.id > 0) {
        const int idx = m_matchedIdsOrdered.indexOf(m_selected.id);
        if (idx >= 0) {
            const int correct = position + idx;
            if (m_selected.position != correct) {
                m_selected.position = correct;
                updatePageInfoLabel();
                updatePrevNextButtonsEnabled();
            }
        }
    }
}

void OperationLogWidget::onHistoryLogClicked(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableViewHistoryLog->model());
    if (!model) {
        return;
    }

    // 检查是否点击了 Description 列（第 3 列）
    constexpr int kColDescription = 3;
    if (index.column() == kColDescription) {
        auto* item = model->item(index.row(), kColDescription);
        if (item) {
            const QString description = item->text();
            if (!description.isEmpty()) {
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(tr("Description"));
                msgBox.setText(description);
                msgBox.setTextInteractionFlags(Qt::TextSelectableByMouse);
                msgBox.exec();
            }
        }
        return;
    }

    QStandardItem* firstItem = model->item(index.row(), 0);
    const int recordId = firstItem ? firstItem->data(Qt::UserRole).toInt() : 0;

    if (recordId > 0 && m_matchedIdsSet.contains(recordId)) {
        const int idxInMatched = m_matchedIdsOrdered.indexOf(recordId);
        m_selected.id = recordId;
        if (idxInMatched < 0) {
            m_selected.position = 0;
        } else if (m_firstMatchedPositionOnPage > 0) {
            m_selected.position = m_firstMatchedPositionOnPage + idxInMatched;
        } else {
            m_selected.position = idxInMatched + 1;
        }
        applyRowBackgrounds();
        updatePageInfoLabel();
        updatePrevNextButtonsEnabled();
    }
}

void OperationLogWidget::onLiveLogClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // Live log 的 Description 是第 2 列（Occur Time, Log Type, Description）
    constexpr int kColDescription = 2;
    if (index.column() != kColDescription) return;

    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewLiveLog->model());
    if (!model) return;

    auto* item = model->item(index.row(), kColDescription);
    if (!item) return;

    const QString description = item->text();
    if (description.isEmpty()) return;

    // 弹出模态框显示完整内容
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Description"));
    msgBox.setText(description);
    msgBox.setTextInteractionFlags(Qt::TextSelectableByMouse);
    msgBox.exec();
}

// Pre/Next：跨页跟踪 —— 在范围 + 条件下查找上/下一条命中，
// 跨越页面边界时自动切换到目标记录所在页。
void OperationLogWidget::jumpToMatchingId(bool next)
{
    if (m_conditions.isEmpty()) {
        return;
    }

    // 1) 先尝试在本页内导航
    if (m_selected.id > 0) {
        const int idx = m_matchedIdsOrdered.indexOf(m_selected.id);
        if (idx >= 0) {
            const int targetIdx = next ? (idx + 1) : (idx - 1);
            if (targetIdx >= 0 && targetIdx < m_matchedIdsOrdered.size()) {
                m_selected.id       = m_matchedIdsOrdered.at(targetIdx);
                m_selected.position = (m_firstMatchedPositionOnPage > 0)
                                          ? (m_firstMatchedPositionOnPage + targetIdx)
                                          : (targetIdx + 1);
                selectAndScrollRowById(m_selected.id);
                applyRowBackgrounds();
                updatePageInfoLabel();
                updatePrevNextButtonsEnabled();
                return;
            }
        }
    }

    // 2) 跨页：基于 anchorId 查询下/上一条命中（在范围内）
    auto* db = LogDB::DatabaseManager::instance().operationLogCon();
    if (!db) return;

    int anchorId = m_selected.id;
    if (anchorId <= 0) {
        // 当前未选中：以本页首/末条命中作为 anchor 起点
        if (m_matchedIdsOrdered.isEmpty()) return;
        anchorId = next ? m_matchedIdsOrdered.last() : m_matchedIdsOrdered.first();
    }

    int matchedId = next
        ? db->queryNextMatchingId(anchorId, m_range.startTime, m_range.endTime,
                                   m_conditions.logType, m_conditions.keyword)
        : db->queryPrevMatchingId(anchorId, m_range.startTime, m_range.endTime,
                                   m_conditions.logType, m_conditions.keyword);
    if (matchedId <= 0) {
        return; // 没有更多命中
    }

    // 3) 算出该 id 在范围内的页号
    int targetPage = db->queryRecordPageInRange(matchedId,
                                                 m_range.startTime, m_range.endTime,
                                                 m_pageSize);
    if (targetPage <= 0) targetPage = 1;

    if (targetPage == m_currentPage) {
        // 同页但本页没有这个 id（不应发生）—— 退化为重查
        submitQueryTask(targetPage, matchedId);
    } else {
        submitQueryTask(targetPage, matchedId);
    }
}

void OperationLogWidget::onPreClicked()
{
    jumpToMatchingId(false);
}

void OperationLogWidget::onNextClicked()
{
    jumpToMatchingId(true);
}

void OperationLogWidget::onTaskFinished(bool success, const QString& message)
{
    if (m_activeTaskId.isEmpty()) {
        return;
    }
    m_activeTaskId.clear();
    if (!m_waitDialog || !m_waitDialog->isVisible()) {
        return;
    }
    if (success) {
        m_waitDialog->setSuccess(tr("Query successful"));
    } else {
        m_waitDialog->setFailure(tr("Query failed: %1").arg(message));
    }
    QTimer::singleShot(1000, this, [this]() {
        if (m_waitDialog) {
            m_waitDialog->hide();
        }
    });
}

void OperationLogWidget::updatePageInfoLabel()
{
    ui->labelPageInfo->setText(QString("%1/%2")
                               .arg(m_selected.position)
                               .arg(m_totalMatchedCount));
}

// Pre/Next 按钮可用性：
//   - 无匹配条件                              ：均不可用（"满足条件"概念不存在）
//   - 总命中数 = 0                            ：均不可用
//   - 已选中 + position > 1                   ：Pre 可用
//   - 已选中 + position < totalMatchedCount   ：Next 可用
//   - 未选中                                   ：只要有命中就都可用（首次跳转选首条）
void OperationLogWidget::updatePrevNextButtonsEnabled()
{
    const bool hasConditions = !m_conditions.isEmpty();
    const bool hasMatches    = (m_totalMatchedCount > 0);
    const bool hasSelection  = (m_selected.position > 0);

    const bool canPre  = hasConditions && hasMatches
                         && (!hasSelection || m_selected.position > 1);
    const bool canNext = hasConditions && hasMatches
                         && (!hasSelection || m_selected.position < m_totalMatchedCount);
    ui->pushButtonPre->setEnabled(canPre);
    ui->pushButtonNext->setEnabled(canNext);
}

void OperationLogWidget::setHistoryLogData(const QList<OperationRecord>& data)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableViewHistoryLog->model());
    if (!model) {
        model = new QStandardItemModel(this);
        ui->tableViewHistoryLog->setModel(model);
    }
    ui->tableViewHistoryLog->verticalHeader()->hide();
    model->clear();

    if (data.isEmpty()) {
        return;
    }

    // 权限过滤：仅保留 record.userPermission <= currentPerm 的记录
    const int currentPerm = static_cast<int>(UserManager::instance()->currentPermission());
    QList<OperationRecord> filtered;
    filtered.reserve(data.size());
    for (const OperationRecord& rec : data) {
        if (rec.userPermission <= currentPerm) {
            filtered.append(rec);
        }
    }

    QStringList headers;
    headers << "Occur Time" << "Log Type" << "Description";
    model->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < filtered.size(); ++row) {
        const OperationRecord& record = filtered[row];
        auto* itemTime    = new QStandardItem(record.occurTime);
        auto* itemLogType = new QStandardItem(LogDB::operationLogTypeName(record.logType));
        auto* itemDesc    = new QStandardItem(record.description);
        itemTime->setData(record.id, Qt::UserRole);
        applyLogTypeForeground(itemTime, itemLogType, itemDesc, record.logType);
        model->setItem(row, 0, itemTime);
        model->setItem(row, 1, itemLogType);
        model->setItem(row, 2, itemDesc);
    }
    applyRowBackgrounds();
    ui->tableViewHistoryLog->resizeColumnsToContents();
    // resizeColumnsToContents 会对所有列设置固定宽度，覆盖 stretch；重新启用
    ui->tableViewHistoryLog->horizontalHeader()->setStretchLastSection(true);
    // model->clear() 使 sizeHint 缩小后布局不会自动恢复，主动通知重新计算
    ui->tableViewHistoryLog->updateGeometry();
}

void OperationLogWidget::selectAndScrollRowById(int recordId)
{
    if (recordId <= 0) {
        return;
    }
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableViewHistoryLog->model());
    if (!model) {
        return;
    }
    for (int row = 0; row < model->rowCount(); ++row) {
        QStandardItem* it = model->item(row, 0);
        if (it && it->data(Qt::UserRole).toInt() == recordId) {
            QModelIndex idx = model->index(row, 0);
            ui->tableViewHistoryLog->scrollTo(idx);
            ui->tableViewHistoryLog->selectRow(row);
            return;
        }
    }
}

// 行背景色：被选中行 = 蓝色；命中行 = 浅绿；其它 = 默认
void OperationLogWidget::applyRowBackgrounds()
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableViewHistoryLog->model());
    if (!model) {
        return;
    }
    const QColor blueSelected(100, 149, 237);
    const QColor lightGreen(144, 238, 144);
    const QBrush defaultBrush;

    const int rows = model->rowCount();
    const int cols = model->columnCount();
    for (int row = 0; row < rows; ++row) {
        QStandardItem* firstItem = model->item(row, 0);
        const int recordId = firstItem ? firstItem->data(Qt::UserRole).toInt() : 0;

        QBrush brush = defaultBrush;
        if (recordId > 0 && recordId == m_selected.id) {
            brush = QBrush(blueSelected);
        } else if (m_matchedIdsSet.contains(recordId)) {
            brush = QBrush(lightGreen);
        }
        for (int col = 0; col < cols; ++col) {
            if (QStandardItem* it = model->item(row, col)) {
                it->setBackground(brush);
            }
        }
    }
}
