#include "alarmlogwidget.h"
#include "ui_alarmlogwidget.h"
#include "datetimesetdialog.h"
#include "alarmtype.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/alarmlogquerytask.h"
#include "paginationwidget.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/alarmlogdb/alarmlogdbcon.h"
#include "usermanager.h"
#include <QStandardItemModel>
#include <QScroller>
#include <QScrollerProperties>
#include <QDebug>
#include <QMessageBox>

AlarmLogWidget::AlarmLogWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AlarmLogWidget)
    , m_currentPage(1)
    , m_pageSize(500)
    , m_totalPages(0)
    , m_lastAlarmLevel(-1)
    , m_lastIsResolved(-1)
{
    ui->setupUi(this);

    connect(ui->checkBoxAll, &QCheckBox::stateChanged,
            this, &AlarmLogWidget::onCheckBoxAllStateChanged);

    connect(ui->pushButtonSetStartTime, &QPushButton::clicked,
            this, &AlarmLogWidget::onSetStartTimeClicked);
    connect(ui->pushButtonSetResolvedTime, &QPushButton::clicked,
            this, &AlarmLogWidget::onSetResolvedTimeClicked);

    connect(ui->pushButtonSearch, &QPushButton::clicked,
            this, &AlarmLogWidget::onSearchClicked);

    connect(ui->widgetPaginate, &PaginationWidget::currentPageChanged,
            this, &AlarmLogWidget::onPaginationPageChanged);

    // ---- checkBox 联动：勾选才启用对应输入控件 ----
    auto bindEnable = [](QCheckBox* cb, const QList<QWidget*>& targets) {
        auto apply = [cb, targets]() {
            const bool on = cb->isChecked();
            for (QWidget* w : targets) if (w) w->setEnabled(on);
        };
        QObject::connect(cb, &QCheckBox::toggled, cb, apply);
        apply(); // 初始化为当前 checkBox 状态
    };
    bindEnable(ui->checkBoxQRCode,       { ui->spinBoxQRCode });
    bindEnable(ui->checkBoxAlarmLevel,   { ui->comboBoxAlarmLevel });
    bindEnable(ui->checkBoxAlarmType,    { ui->comboBoxAlarmType });
    bindEnable(ui->checkBoxIsResolved,   { ui->comboBoxIsResolved });
    bindEnable(ui->checkBoxStartTime,    { ui->lineEditStartTime, ui->lineEditEndTime, ui->pushButtonSetStartTime });
    bindEnable(ui->checkBoxResolvedTime, { ui->lineEditResolvedTime, ui->lineEditResolvedEndTime, ui->pushButtonSetResolvedTime });

    // ---- 时间 lineEdit 设为只读（仅通过 DateTimeSetDialog 设值）----
    ui->lineEditStartTime->setReadOnly(true);
    ui->lineEditEndTime->setReadOnly(true);
    ui->lineEditResolvedTime->setReadOnly(true);
    ui->lineEditResolvedEndTime->setReadOnly(true);

    initLiveLog();

    // history log 表：最后一列拉伸充满剩余宽度
    ui->tableViewHistoryLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewHistoryLog->verticalHeader()->setVisible(false);
    ui->tableViewHistoryLog->setEditTriggers(QAbstractItemView::NoEditTriggers);

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

    // 连接表格点击信号，用于显示 description 完整内容
    connect(ui->tableViewLiveLog, &QTableView::clicked,
            this, &AlarmLogWidget::onLiveLogClicked);
    connect(ui->tableViewHistoryLog, &QTableView::clicked,
            this, &AlarmLogWidget::onHistoryLogClicked);
}

void AlarmLogWidget::initLiveLog()
{
    // 表头与历史查询表保持一致（除 id 列——live log 无法提供）
    auto* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({
        "Alarm Level", "Occur Time", "QRCode", "Alarm Type",
        "Is Resolved", "Resolve Time", "Description"
    });
    ui->tableViewLiveLog->setModel(model);
    ui->tableViewLiveLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewLiveLog->verticalHeader()->setVisible(false);
    ui->tableViewLiveLog->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置列宽：确保时间字段和其他字段完整显示
    ui->tableViewLiveLog->setColumnWidth(1, 180);  // Occur Time
    ui->tableViewLiveLog->setColumnWidth(3, 200);  // Alarm Type
    ui->tableViewLiveLog->setColumnWidth(4, 120);  // Is Resolved
    ui->tableViewLiveLog->setColumnWidth(5, 180);  // Resolve Time

    if (auto* db = LogDB::DatabaseManager::instance().alarmLogCon()) {
        connect(db, &LogDB::AlarmLogDBCon::recordInserted,
                this, &AlarmLogWidget::onRecordInserted);
        connect(db, &LogDB::AlarmLogDBCon::recordResolved,
                this, &AlarmLogWidget::onRecordResolved);
    }

    // 启动时预点状态：将上一次未解决的警报载入 live log
    loadUnresolvedToLiveLog();
}

void AlarmLogWidget::loadUnresolvedToLiveLog()
{
    auto* db = LogDB::DatabaseManager::instance().alarmLogCon();
    if (!db) return;

    // 查询 is_resolved=0 的未解决记录（SQL 结果按 occur_time DESC）
    const QList<AlarmRecord> rows = db->queryPageWithConditions(
        /*alarmLevel*/ -1,
        /*qrCode*/ QString(),
        /*alarmType*/ QString(),
        /*isResolved*/ 0,
        /*startTime*/ QString(),
        /*endTime*/ QString(),
        /*pageSize*/ kLiveLogMaxRows,
        /*pageNumber*/ 1);

    // onRecordInserted 采用 insertRow(0)（最新在顶），
    // 需逆序递交才能让最新一条最后插入、位于 row 0
    for (auto it = rows.crbegin(); it != rows.crend(); ++it) {
        onRecordInserted(*it);
    }
}

void AlarmLogWidget::onRecordResolved(const QString& qrCode,
                                      const QString& alarmType,
                                      const QString& resolveTime)
{
    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewLiveLog->model());
    if (!model) return;

    // alarm_type 列存的是友好名称（alarmTypeName），使同样映射后再比较
    bool typeOk = false;
    const int typeVal = alarmType.toInt(&typeOk);
    const QString typeText = typeOk ? alarmTypeName(typeVal) : alarmType;
    const QString resolvedText = alarmResolvedStatusName(1);

    // 列顺序：0=Level 1=OccurTime 2=QRCode 3=AlarmType 4=IsResolved 5=ResolveTime ...
    constexpr int kColQrCode      = 2;
    constexpr int kColAlarmType   = 3;
    constexpr int kColIsResolved  = 4;
    constexpr int kColResolveTime = 5;

    // 从顶部（最新）向下查找首个未解决且 (qrCode, alarmType) 匹配的行
    const int rows = model->rowCount();
    for (int r = 0; r < rows; ++r) {
        const QString rowQr   = model->item(r, kColQrCode)   ? model->item(r, kColQrCode)->text()   : QString();
        const QString rowType = model->item(r, kColAlarmType)? model->item(r, kColAlarmType)->text(): QString();
        const QString rowRes  = model->item(r, kColIsResolved)? model->item(r, kColIsResolved)->text(): QString();
        if (rowQr == qrCode && rowType == typeText && rowRes != resolvedText) {
            if (auto* it = model->item(r, kColIsResolved))  it->setText(resolvedText);
            if (auto* it = model->item(r, kColResolveTime)) it->setText(resolveTime);

            // 仅更新 Is Resolved 字段（第 4 列）背景色为绿色（已解决）
            const QColor resolvedColor(200, 255, 200);
            if (auto* it = model->item(r, kColIsResolved)) {
                it->setBackground(resolvedColor);
            }
            return;
        }
    }
}

void AlarmLogWidget::onRecordInserted(const AlarmRecord& record)
{
    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewLiveLog->model());
    if (!model) return;

    // 权限过滤：仅允许查看不高于当前用户权限的记录
    const int currentPerm = static_cast<int>(UserManager::instance()->currentPermission());
    if (record.userPermission > currentPerm) {
        return;
    }

    // alarm_level / alarm_type / is_resolved 做友好化映射
    const QString levelText = alarmLevelName(record.alarmLevel);

    const QString typeText = alarmTypeName(record.alarmType);

    const QString resolvedText = alarmResolvedStatusName(record.isResolved);

    QList<QStandardItem*> items;
    items << new QStandardItem(levelText)
          << new QStandardItem(record.occurTime)
          << new QStandardItem(record.qrCode)
          << new QStandardItem(typeText)
          << new QStandardItem(resolvedText)
          << new QStandardItem(record.resolveTime)
          << new QStandardItem(record.description);

    // 设置文本对齐：除 Description 字段（第 6 列）外，其他字段居中
    for (int i = 0; i < items.size(); ++i) {
        if (i != 6) {  // Description 列不居中
            items[i]->setTextAlignment(Qt::AlignCenter);
        }
    }

    // 仅对 Is Resolved 字段（第 4 列）设置背景色和字体颜色
    constexpr int kColIsResolved = 4;
    QColor resolvedColor;
    switch (record.isResolved) {
        case static_cast<int>(AlarmResolvedStatus::Unresolved):
            resolvedColor = QColor(255, 100, 100);  // 鲜艳红色
            break;
        case static_cast<int>(AlarmResolvedStatus::Resolved):
            resolvedColor = QColor(200, 255, 200);  // 绿色
            break;
        case static_cast<int>(AlarmResolvedStatus::NoNeed):
            resolvedColor = QColor(255, 255, 200);  // 黄色
            break;
        default:
            resolvedColor = QColor(255, 255, 255);  // 白色
            break;
    }
    if (kColIsResolved < items.size()) {
        items[kColIsResolved]->setBackground(resolvedColor);
    }

    model->insertRow(0, items);

    // 行数超过上限时，清除所有已解决（Resolved）和无需解决（NoNeed）的记录
    // 仅保留未解决（Unresolved）告警
    // 列 4 = Is Resolved，文本由 alarmResolvedStatusName 映射
    if (model->rowCount() > kLiveLogMaxRows) {
        constexpr int kColIsResolved = 4;
        const QString resolvedText = alarmResolvedStatusName(static_cast<int>(AlarmResolvedStatus::Resolved));
        const QString noNeedText   = alarmResolvedStatusName(static_cast<int>(AlarmResolvedStatus::NoNeed));
        // 从底部向上删除，避免行号偏移
        for (int r = model->rowCount() - 1; r >= 0; --r) {
            QStandardItem* it = model->item(r, kColIsResolved);
            if (!it) continue;
            const QString s = it->text();
            if (s == resolvedText || s == noNeedText) {
                model->removeRow(r);
            }
        }
    }
}

void AlarmLogWidget::initUi()
{
    // QRCode 设备编号范围 1~10（与写入测试中的 DEVICE-0001..DEVICE-0010 对齐）
    ui->spinBoxQRCode->setRange(1, 10);
    ui->spinBoxQRCode->setValue(1);

    // 警报级别：按 alarmtype.h 中定义的 AlarmLevel 枚举填充
    ui->comboBoxAlarmLevel->clear();
    for (const auto& it : alarmLevelList()) {
        ui->comboBoxAlarmLevel->addItem(it.first, it.second);
    }

    // 警报类型：按 alarmtype.h 中定义的 AlarmType 枚举填充（显示名称，data=枚举值）
    ui->comboBoxAlarmType->clear();
    for (const auto& it : alarmTypeList()) {
        ui->comboBoxAlarmType->addItem(it.first, it.second);
    }

    // 是否解决：按 alarmtype.h 中定义的 AlarmResolvedStatus 枚举填充
    ui->comboBoxIsResolved->clear();
    for (const auto& it : alarmResolvedStatusList()) {
        ui->comboBoxIsResolved->addItem(it.first, it.second);
    }
}

void AlarmLogWidget::onSetStartTimeClicked()
{
    DateTimeSetDialog dialog(this);
    dialog.setStartTime(ui->lineEditStartTime->text().trimmed());
    dialog.setEndTime(ui->lineEditEndTime->text().trimmed());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 仅当对话框中对应的开关启用时才回填文本，否则清空
    const QString startText = dialog.isStartTimeEnabled() ? dialog.getStartTime() : QString();
    const QString endText   = dialog.isEndTimeEnabled()   ? dialog.getEndTime()   : QString();
    ui->lineEditStartTime->setText(startText);
    ui->lineEditEndTime->setText(endText);

    // 用户在对话框中启用了任一时间 → 自动勾选 checkBoxStartTime；都未启用 → 取消勾选
    const bool anyEnabled = !startText.isEmpty() || !endText.isEmpty();
    ui->checkBoxStartTime->setChecked(anyEnabled);
}

void AlarmLogWidget::onSetResolvedTimeClicked()
{
    DateTimeSetDialog dialog(this);
    dialog.setStartTime(ui->lineEditResolvedTime->text().trimmed());
    dialog.setEndTime(ui->lineEditResolvedEndTime->text().trimmed());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString startText = dialog.isStartTimeEnabled() ? dialog.getStartTime() : QString();
    const QString endText   = dialog.isEndTimeEnabled()   ? dialog.getEndTime()   : QString();
    ui->lineEditResolvedTime->setText(startText);
    ui->lineEditResolvedEndTime->setText(endText);

    const bool anyEnabled = !startText.isEmpty() || !endText.isEmpty();
    ui->checkBoxResolvedTime->setChecked(anyEnabled);
}

void AlarmLogWidget::onSearchClicked()
{
    // 从 UI 采集查询条件并缓存，供翻页复用
    m_lastAlarmLevel = -1;
    m_lastQRCode.clear();
    m_lastAlarmType.clear();
    m_lastIsResolved = -1;
    m_lastStartTime.clear();
    m_lastEndTime.clear();

    if (ui->checkBoxQRCode->isChecked()) {
        m_lastQRCode = QString::number(ui->spinBoxQRCode->value());
    }

    if (ui->checkBoxAlarmLevel->isChecked()) {
        QVariant data = ui->comboBoxAlarmLevel->currentData();
        bool ok = false;
        int level = data.isValid() ? data.toInt(&ok)
                                   : ui->comboBoxAlarmLevel->currentText().toInt(&ok);
        if (ok) {
            m_lastAlarmLevel = level;
        }
    }

    if (ui->checkBoxAlarmType->isChecked()) {
        QVariant data = ui->comboBoxAlarmType->currentData();
        bool ok = false;
        int typeValue = data.isValid() ? data.toInt(&ok) : 0;
        if (ok && typeValue > 0) {
            // alarm_type 列为 TEXT，以枚举整数的字符串形式存入 / 查询
            m_lastAlarmType = QString::number(typeValue);
        }
    }

    if (ui->checkBoxIsResolved->isChecked()) {
        QVariant data = ui->comboBoxIsResolved->currentData();
        bool ok = false;
        int v = data.isValid() ? data.toInt(&ok)
                               : ui->comboBoxIsResolved->currentText().toInt(&ok);
        if (ok) {
            m_lastIsResolved = v;
        }
    }

    if (ui->checkBoxStartTime->isChecked()) {
        QString startTime = ui->lineEditStartTime->text().trimmed();
        QString endTime   = ui->lineEditEndTime->text().trimmed();
        if (!startTime.isEmpty() || !endTime.isEmpty()) {
            m_lastStartTime = startTime;
            m_lastEndTime   = endTime;
        }
    }

    // 点击 Search 总是回到第 1 页
    m_currentPage = 1;
    submitQuery(m_currentPage);
}

void AlarmLogWidget::onPaginationPageChanged(int page)
{
    if (page <= 0) return;
    m_currentPage = page;
    submitQuery(page);
}

void AlarmLogWidget::submitQuery(int page)
{
    AlarmLogQueryTask* task = new AlarmLogQueryTask();
    task->setPageNumber(page);
    task->setPageSize(m_pageSize);

    if (m_lastAlarmLevel != -1)      task->setAlarmLevel(m_lastAlarmLevel);
    if (!m_lastQRCode.isEmpty())     task->setQRCode(m_lastQRCode);
    if (!m_lastAlarmType.isEmpty())  task->setAlarmType(m_lastAlarmType);
    if (m_lastIsResolved != -1)      task->setIsResolved(m_lastIsResolved);
    if (!m_lastStartTime.isEmpty() || !m_lastEndTime.isEmpty()) {
        QString s = m_lastStartTime;
        QString e = m_lastEndTime;
        // 自动纠正顺序颠倒：开始时间晚于结束时间会让 BETWEEN 返回空集
        if (!s.isEmpty() && !e.isEmpty() && s > e) {
            qSwap(s, e);
        }
        task->setOccurTimeRange(s, e);
    }

    connect(task, &AlarmLogQueryTask::pageWithConditionsResult,
            this, &AlarmLogWidget::onPageWithConditionsResult, Qt::QueuedConnection);
    connect(task, &AlarmLogQueryTask::totalCountWithConditionsResult,
            this, &AlarmLogWidget::onTotalCountWithConditionsResult, Qt::QueuedConnection);

    Scheduler::instance()->submitTask(task);
}

void AlarmLogWidget::onPageWithConditionsResult(const QList<AlarmRecord>& records)
{
    setHistoryLogData(records);
}

void AlarmLogWidget::onTotalCountWithConditionsResult(int totalCount)
{
    const int pageSize = (m_pageSize > 0) ? m_pageSize : 1;
    m_totalPages = (totalCount + pageSize - 1) / pageSize;
    ui->widgetPaginate->setTotalPages(m_totalPages);
    ui->widgetPaginate->setCurrentPage(m_currentPage);
    qDebug() << "[AlarmLogWidget] 条件查询 当前页/总页数:"
             << m_currentPage << "/" << m_totalPages
             << " 总记录数:" << totalCount;
}

void AlarmLogWidget::setHistoryLogData(const QList<AlarmRecord>& data)
{
    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(ui->tableViewHistoryLog->model());
    if (!model) {
        model = new QStandardItemModel(this);
        ui->tableViewHistoryLog->setModel(model);
    }

    model->clear();

    if (data.isEmpty()) {
        return;
    }

    // 权限过滤：仅保留 record.userPermission <= currentPerm 的记录
    const int currentPerm = static_cast<int>(UserManager::instance()->currentPermission());
    QList<AlarmRecord> filtered;
    filtered.reserve(data.size());
    for (const AlarmRecord& r : data) {
        if (r.userPermission <= currentPerm) {
            filtered.append(r);
        }
    }

    QStringList headers;
    headers << "ID" << "Alarm Level" << "Occur Time" << "QRCode" << "Alarm Type"
            << "Is Resolved" << "Resolve Time" << "Description";
    model->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < filtered.size(); ++row) {
        const AlarmRecord& r = filtered[row];
        // alarm_level 以枚举名称显示
        const QString levelText = alarmLevelName(r.alarmLevel);
        // alarm_type 存为 TEXT（枚举整数字符串），转成名称显示
        const QString typeText = alarmTypeName(r.alarmType);
        // is_resolved 以枚举名称显示
        const QString resolvedText = alarmResolvedStatusName(r.isResolved);

        model->setItem(row, 0, new QStandardItem(QString::number(r.id)));
        model->setItem(row, 1, new QStandardItem(levelText));
        model->setItem(row, 2, new QStandardItem(r.occurTime));
        model->setItem(row, 3, new QStandardItem(r.qrCode));
        model->setItem(row, 4, new QStandardItem(typeText));
        model->setItem(row, 5, new QStandardItem(resolvedText));
        model->setItem(row, 6, new QStandardItem(r.resolveTime));
        model->setItem(row, 7, new QStandardItem(r.description));

        // 为 Is Resolved 字段（第 5 列）设置背景色
        constexpr int kColIsResolved = 5;
        QColor resolvedColor;
        switch (r.isResolved) {
            case static_cast<int>(AlarmResolvedStatus::Unresolved):
                resolvedColor = QColor(255, 100, 100);  // 鲜艳红色
                break;
            case static_cast<int>(AlarmResolvedStatus::Resolved):
                resolvedColor = QColor(200, 255, 200);  // 绿色
                break;
            case static_cast<int>(AlarmResolvedStatus::NoNeed):
                resolvedColor = QColor(255, 255, 200);  // 黄色
                break;
            default:
                resolvedColor = QColor(255, 255, 255);  // 白色
                break;
        }
        if (auto* it = model->item(row, kColIsResolved)) {
            it->setBackground(resolvedColor);
        }
    }

    ui->tableViewHistoryLog->resizeColumnsToContents();
}

void AlarmLogWidget::onLiveLogClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // Live log 的 Description 是第 6 列，QRCode 是第 2 列
    constexpr int kColDescription = 6;
    constexpr int kColQRCode = 2;
    if (index.column() != kColDescription) return;

    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewLiveLog->model());
    if (!model) return;

    auto* item = model->item(index.row(), kColDescription);
    auto* qrCodeItem = model->item(index.row(), kColQRCode);
    if (!item) return;

    const QString description = item->text();
    if (description.isEmpty()) return;

    // 弹出模态框显示完整内容，标题栏显示 QRCode
    QMessageBox msgBox(this);
    const QString title = qrCodeItem ? tr("Description - %1").arg(qrCodeItem->text()) : tr("Description");
    msgBox.setWindowTitle(title);
    msgBox.setText(description);
    msgBox.setTextInteractionFlags(Qt::TextSelectableByMouse);
    msgBox.exec();
}

void AlarmLogWidget::onHistoryLogClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // History log 的 Description 是第 7 列，QRCode 是第 3 列（ID, Alarm Level, Occur Time, QRCode, Alarm Type, Is Resolved, Resolve Time, Description）
    constexpr int kColDescription = 7;
    constexpr int kColQRCode = 3;
    if (index.column() != kColDescription) return;

    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewHistoryLog->model());
    if (!model) return;

    auto* item = model->item(index.row(), kColDescription);
    auto* qrCodeItem = model->item(index.row(), kColQRCode);
    if (!item) return;

    const QString description = item->text();
    if (description.isEmpty()) return;

    // 弹出模态框显示完整内容，标题栏显示 QRCode
    QMessageBox msgBox(this);
    const QString title = qrCodeItem ? tr("Description - %1").arg(qrCodeItem->text()) : tr("Description");
    msgBox.setWindowTitle(title);
    msgBox.setText(description);
    msgBox.setTextInteractionFlags(Qt::TextSelectableByMouse);
    msgBox.exec();
}

AlarmLogWidget::~AlarmLogWidget()
{
    delete ui;
}

void AlarmLogWidget::onCheckBoxAllStateChanged(int state)
{
    bool checked = (state == Qt::Checked);
    ui->checkBoxQRCode->setChecked(checked);
    ui->checkBoxAlarmLevel->setChecked(checked);
    ui->checkBoxAlarmType->setChecked(checked);
    ui->checkBoxIsResolved->setChecked(checked);
    ui->checkBoxStartTime->setChecked(checked);
    ui->checkBoxResolvedTime->setChecked(checked);
}
