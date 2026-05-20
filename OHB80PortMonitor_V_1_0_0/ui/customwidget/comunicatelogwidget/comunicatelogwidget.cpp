#include "comunicatelogwidget.h"
#include "ui_comunicatelogwidget.h"
#include <QStandardItemModel>
#include <QDebug>
#include "scheduler/scheduler.h"
#include "scheduler/tasks/communicatelogquerytask.h"
#include "paginationwidget/paginationwidget.h"
#include "datetimesetdialog.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/communicatelogdb/communicatelogdbcon.h"
#include "usermanager.h"
#include "app/shareddata.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbuscommand/commandpool.h"
#include <QHeaderView>
#include <QScroller>
#include <QScrollerProperties>
#include <QMessageBox>

// execStatus 数字转可读字符串
static QString execStatusToString(int status)
{
    switch (status) {
    case 0: return QStringLiteral("Success");
    case 1: return QStringLiteral("Timeout");
    case 2: return QStringLiteral("Retry");
    case 3: return QStringLiteral("Send Failed");
    default: return QString::number(status);
    }
}

// 固定列头（包含完整的通讯日志字段）
const QStringList ComunicateLogWidget::kLiveHeaders = {
    QStringLiteral("QRCode"),
    QStringLiteral("Send Time"),
    QStringLiteral("Response Time"),
    QStringLiteral("Command ID"),
    QStringLiteral("Duration Ms"),
    QStringLiteral("Exec Status"),
    QStringLiteral("Retry Count"),
    QStringLiteral("Request"),
    QStringLiteral("Response"),
    QStringLiteral("Description")
};

ComunicateLogWidget::ComunicateLogWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ComunicateLogWidget)
    , m_currentPage(1)
    , m_pageSize(500)
    , m_totalPages(0)
    , m_lastQueryHadConditions(false)
    , m_lastExecStatus(-1)
    , m_lastRetryCount(-1)
{
    ui->setupUi(this);

    connect(ui->checkBoxAll, &QCheckBox::stateChanged,
            this, &ComunicateLogWidget::onCheckBoxAllStateChanged);

    connect(ui->pushButtonSearch, &QPushButton::clicked,
            this, &ComunicateLogWidget::onSearchClicked);

    connect(ui->widgetPaginate, &PaginationWidget::currentPageChanged,
            this, &ComunicateLogWidget::onPaginationPageChanged);

    connect(ui->pushButtonSetTime, &QPushButton::clicked,
            this, &ComunicateLogWidget::onSetTimeClicked);

    // ---- checkBox 联动：勾选才启用对应输入控件 ----
    auto bindEnable = [](QCheckBox* cb, const QList<QWidget*>& targets) {
        auto apply = [cb, targets]() {
            const bool on = cb->isChecked();
            for (QWidget* w : targets) if (w) w->setEnabled(on);
        };
        QObject::connect(cb, &QCheckBox::toggled, cb, apply);
        apply();
    };
    bindEnable(ui->checkBoxQRCode,     { ui->spinBoxQRCode });
    bindEnable(ui->checkBoxCommandId,  { ui->comboBoxCommandId });
    bindEnable(ui->checkBoxExecStatus, { ui->comboBoxExecStatus });
    bindEnable(ui->checkBoxRetryCount, { ui->spinBoxRetryCount });
    bindEnable(ui->checkBoxSendTime,   { ui->lineEditSendTime, ui->lineEditSendEndTime, ui->pushButtonSetTime });

    // ---- 时间 lineEdit 设为只读（仅通过 DateTimeSetDialog 设值）----
    ui->lineEditSendTime->setReadOnly(true);
    ui->lineEditSendEndTime->setReadOnly(true);

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
            this, &ComunicateLogWidget::onLiveLogClicked);
    connect(ui->tableViewHistoryLog, &QTableView::clicked,
            this, &ComunicateLogWidget::onHistoryLogClicked);
}

void ComunicateLogWidget::initLiveLog()
{
    // 表头与老的 CommunicateLoggerWidget 一致
    auto* model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels(kLiveHeaders);
    ui->tableViewLiveLog->setModel(model);
    ui->tableViewLiveLog->horizontalHeader()->setStretchLastSection(true);
    ui->tableViewLiveLog->verticalHeader()->setVisible(false);
    ui->tableViewLiveLog->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 设置列宽：确保时间字段完整显示
    ui->tableViewLiveLog->setColumnWidth(0, 80);   // QRCode
    ui->tableViewLiveLog->setColumnWidth(1, 240);  // Send Time
    ui->tableViewLiveLog->setColumnWidth(2, 240);  // Response Time
    ui->tableViewLiveLog->setColumnWidth(3, 100);  // Command ID
    ui->tableViewLiveLog->setColumnWidth(4, 80);   // Duration Ms
    ui->tableViewLiveLog->setColumnWidth(5, 80);   // Exec Status
    ui->tableViewLiveLog->setColumnWidth(6, 80);   // Retry Count
    ui->tableViewLiveLog->setColumnWidth(7, 180);  // Request
    ui->tableViewLiveLog->setColumnWidth(8, 180);  // Response
    ui->tableViewLiveLog->setColumnWidth(9, 150);  // Description

    // 根据用户权限设置列的可见性
    updateColumnVisibility();
}

void ComunicateLogWidget::updateColumnVisibility()
{
    // 获取当前用户权限
    const int currentPerm = static_cast<int>(UserManager::instance()->currentPermission());

    // Engineer (3) 以下用户隐藏敏感列
    const bool hideSensitiveColumns = (currentPerm < 3);

    // Live log 列顺序：QRCode(0), Send Time(1), Response Time(2), Command ID(3),
    // Duration Ms(4), Exec Status(5), Retry Count(6), Request(7), Response(8), Description(9)
    ui->tableViewLiveLog->setColumnHidden(2, hideSensitiveColumns);   // Response Time
    ui->tableViewLiveLog->setColumnHidden(4, hideSensitiveColumns);   // Duration Ms
    ui->tableViewLiveLog->setColumnHidden(5, hideSensitiveColumns);   // Exec Status
    ui->tableViewLiveLog->setColumnHidden(6, hideSensitiveColumns);   // Retry Count
    ui->tableViewLiveLog->setColumnHidden(7, hideSensitiveColumns);   // Request
    ui->tableViewLiveLog->setColumnHidden(8, hideSensitiveColumns);   // Response

    // History log 列顺序：QRCode(0), Send Time(1), Response Time(2), Command ID(3),
    // Exec Status(4), Retry Count(5), Request(6), Response(7), Description(8)
    // 注意：History log 没有 Duration Ms 列和 ID 列
    ui->tableViewHistoryLog->setColumnHidden(2, hideSensitiveColumns); // Response Time
    ui->tableViewHistoryLog->setColumnHidden(4, hideSensitiveColumns); // Exec Status
    ui->tableViewHistoryLog->setColumnHidden(5, hideSensitiveColumns); // Retry Count
    ui->tableViewHistoryLog->setColumnHidden(6, hideSensitiveColumns); // Request
    ui->tableViewHistoryLog->setColumnHidden(7, hideSensitiveColumns); // Response
}

void ComunicateLogWidget::initQrcodeList(const QStringList& qrcodes)
{
    m_qrcodeRow.clear();
    m_liveRecords.clear();
    m_liveRecords.reserve(qrcodes.size());

    for (int i = 0; i < qrcodes.size(); ++i) {
        m_qrcodeRow[qrcodes[i]] = i;
        QStringList row;
        row.reserve(kLiveHeaders.size());
        row << qrcodes[i]; // 第 0 列：qrcode
        for (int col = 1; col < kLiveHeaders.size(); ++col)
            row << QString();
        m_liveRecords.append(row);
    }

    // 更新表格
    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewLiveLog->model());
    if (model) {
        model->clear();
        model->setHorizontalHeaderLabels(kLiveHeaders);
        for (int i = 0; i < m_liveRecords.size(); ++i) {
            QList<QStandardItem*> items;
            for (const QString& text : m_liveRecords[i]) {
                items << new QStandardItem(text);
            }
            // 设置文本对齐：除 Description 字段（第 9 列）外，其他字段居中
            for (int col = 0; col < items.size(); ++col) {
                if (col != 9) {  // Description 列不居中
                    items[col]->setTextAlignment(Qt::AlignCenter);
                }
            }
            model->appendRow(items);
        }
    }
}

void ComunicateLogWidget::writeLog(const QString& qrcode, const QString& sendTime,
                                    const QString& responseTime, const QString& commandId,
                                    const QString& durationMs, const QString& execStatus,
                                    const QString& retryCount, const QString& request,
                                    const QString& response, const QString& description)
{
    // 按 qrcode 更新实时表格对应行
    auto it = m_qrcodeRow.constFind(qrcode);
    if (it == m_qrcodeRow.constEnd()) return;

    int row = it.value();
    m_liveRecords[row][0] = qrcode;
    m_liveRecords[row][1] = sendTime;
    m_liveRecords[row][2] = responseTime;
    m_liveRecords[row][3] = commandId;
    m_liveRecords[row][4] = durationMs;
    m_liveRecords[row][5] = execStatus;
    m_liveRecords[row][6] = retryCount;
    m_liveRecords[row][7] = request;
    m_liveRecords[row][8] = response;
    m_liveRecords[row][9] = description;

    // 更新表格
    auto* model = qobject_cast<QStandardItemModel*>(ui->tableViewLiveLog->model());
    if (model && row < model->rowCount()) {
        model->setItem(row, 0, new QStandardItem(qrcode));
        model->setItem(row, 1, new QStandardItem(sendTime));
        model->setItem(row, 2, new QStandardItem(responseTime));
        model->setItem(row, 3, new QStandardItem(commandId));
        model->setItem(row, 4, new QStandardItem(durationMs));
        model->setItem(row, 5, new QStandardItem(execStatusToString(execStatus.toInt())));
        model->setItem(row, 6, new QStandardItem(retryCount));
        model->setItem(row, 7, new QStandardItem(request));
        model->setItem(row, 8, new QStandardItem(response));
        model->setItem(row, 9, new QStandardItem(description));

        // 设置文本对齐：除 Description 字段（第 9 列）外，其他字段居中
        for (int col = 0; col < 10; ++col) {
            if (col != 9) {  // Description 列不居中
                if (auto* item = model->item(row, col)) {
                    item->setTextAlignment(Qt::AlignCenter);
                }
            }
        }
    }
}

void ComunicateLogWidget::initUi()
{
    // QRCode 设备编号：使用 SharedData::getAllQrcodes 获取实际设备列表
    QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        // 如果没有获取到 qrcode 列表，使用默认范围
        ui->spinBoxQRCode->setRange(1, 80);
        ui->spinBoxQRCode->setValue(1);
    } else {
        // 使用实际设备编号范围，初始值设为第一个 qrcode 对应的数字
        ui->spinBoxQRCode->setRange(1, 99999);
        const int firstValue = qrcodes.first().toInt();
        ui->spinBoxQRCode->setValue(firstValue > 0 ? firstValue : 1);
    }

    // 重试次数范围 0~3
    ui->spinBoxRetryCount->setRange(0, 3);
    ui->spinBoxRetryCount->setValue(0);

    // 指令 ID 候选集合：使用 CommandPool::ids() 获取实际指令列表
    ui->comboBoxCommandId->clear();
    ModbusTcpMasterManager &mgr = ModbusTcpMasterManager::instance();
    CommandPool *pool = mgr.commandPool();
    if (pool) {
        const QStringList commandIds = pool->ids();
        for (const QString& cmd : commandIds) {
            ui->comboBoxCommandId->addItem(cmd);
        }
    }

    // 执行状态：0(成功) / 1(响应超时) / 2(重发指令) / 3(指令发送失败)
    ui->comboBoxExecStatus->clear();
    ui->comboBoxExecStatus->addItem(tr("Success"),     0);
    ui->comboBoxExecStatus->addItem(tr("Timeout"),     1);
    ui->comboBoxExecStatus->addItem(tr("Retry"),       2);
    ui->comboBoxExecStatus->addItem(tr("Send Failed"), 3);

    // 初始化实时日志表格（使用前面已获取的 qrcodes）
    if (qrcodes.isEmpty()) {
        // 如果没有获取到 qrcode 列表，使用默认的 1~80
        qrcodes.reserve(kDefaultRowCount);
        for (int i = 1; i <= kDefaultRowCount; ++i)
            qrcodes << QString::number(i);
    }
    initQrcodeList(qrcodes);
}

void ComunicateLogWidget::onSetTimeClicked()
{
    DateTimeSetDialog dialog(this);
    dialog.setStartTime(ui->lineEditSendTime->text().trimmed());
    dialog.setEndTime(ui->lineEditSendEndTime->text().trimmed());
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 仅当对话框中对应的开关启用时才回填文本，否则清空
    const QString startText = dialog.isStartTimeEnabled() ? dialog.getStartTime() : QString();
    const QString endText   = dialog.isEndTimeEnabled()   ? dialog.getEndTime()   : QString();
    ui->lineEditSendTime->setText(startText);
    ui->lineEditSendEndTime->setText(endText);

    // 用户在对话框中启用了任一时间 → 自动勾选 checkBoxSendTime；都未启用 → 取消勾选
    // 否则用户经常忘记勾选复选框，导致 Search 时时间条件被忽略
    const bool anyEnabled = !startText.isEmpty() || !endText.isEmpty();
    ui->checkBoxSendTime->setChecked(anyEnabled);
}

ComunicateLogWidget::~ComunicateLogWidget()
{
    delete ui;
}

void ComunicateLogWidget::onCheckBoxAllStateChanged(int state)
{
    bool checked = (state == Qt::Checked);
    ui->checkBoxQRCode->setChecked(checked);
    ui->checkBoxCommandId->setChecked(checked);
    ui->checkBoxExecStatus->setChecked(checked);
    ui->checkBoxRetryCount->setChecked(checked);
    ui->checkBoxSendTime->setChecked(checked);
}

void ComunicateLogWidget::onSearchClicked()
{
    // 从 UI 采集查询条件并缓存，供翻页复用
    m_lastCommandId.clear();
    m_lastQRCode.clear();
    m_lastExecStatus = -1;
    m_lastRetryCount = -1;
    m_lastStartTime.clear();
    m_lastEndTime.clear();

    bool hasConditions = false;

    if (ui->checkBoxCommandId->isChecked()) {
        QString commandId = ui->comboBoxCommandId->currentText().trimmed();
        if (!commandId.isEmpty()) {
            m_lastCommandId = commandId;
            hasConditions = true;
        }
    }

    if (ui->checkBoxQRCode->isChecked()) {
        m_lastQRCode = QString::number(ui->spinBoxQRCode->value());
        hasConditions = true;
    }

    if (ui->checkBoxRetryCount->isChecked()) {
        m_lastRetryCount = ui->spinBoxRetryCount->value();
        hasConditions = true;
    }

    if (ui->checkBoxExecStatus->isChecked()) {
        QVariant data = ui->comboBoxExecStatus->currentData();
        bool ok = false;
        int execStatus = data.isValid() ? data.toInt(&ok)
                                        : ui->comboBoxExecStatus->currentText().toInt(&ok);
        if (ok) {
            m_lastExecStatus = execStatus;
            hasConditions = true;
        }
    }

    if (ui->checkBoxSendTime->isChecked()) {
        QString startTime = ui->lineEditSendTime->text().trimmed();
        QString endTime   = ui->lineEditSendEndTime->text().trimmed();
        if (!startTime.isEmpty() || !endTime.isEmpty()) {
            m_lastStartTime = startTime;
            m_lastEndTime   = endTime;
            hasConditions = true;
        }
    }

    m_lastQueryHadConditions = hasConditions;

    // 点击 Search 总是回到第 1 页
    m_currentPage = 1;
    submitQuery(m_currentPage);
}

void ComunicateLogWidget::onPaginationPageChanged(int page)
{
    if (page <= 0) return;
    m_currentPage = page;
    submitQuery(page);
}

void ComunicateLogWidget::submitQuery(int page)
{
    CommunicateLogQueryTask* task = new CommunicateLogQueryTask();
    task->setPageNumber(page);
    task->setPageSize(m_pageSize);

    if (!m_lastCommandId.isEmpty())  task->setCommandId(m_lastCommandId);
    if (!m_lastQRCode.isEmpty())     task->setQRCode(m_lastQRCode);
    if (m_lastExecStatus != -1)      task->setExecStatus(m_lastExecStatus);
    if (m_lastRetryCount != -1)      task->setRetryCount(m_lastRetryCount);
    if (!m_lastStartTime.isEmpty() || !m_lastEndTime.isEmpty()) {
        QString s = m_lastStartTime;
        QString e = m_lastEndTime;
        // 自动纠正顺序颠倒：开始时间晚于结束时间会让 BETWEEN 返回空集
        if (!s.isEmpty() && !e.isEmpty() && s > e) {
            qSwap(s, e);
        }
        task->setSendTimeRange(s, e);
    }

    connect(task, &CommunicateLogQueryTask::pageWithConditionsResult,
            this, &ComunicateLogWidget::onPageWithConditionsResult, Qt::QueuedConnection);
    connect(task, &CommunicateLogQueryTask::totalCountWithConditionsResult,
            this, &ComunicateLogWidget::onTotalCountWithConditionsResult, Qt::QueuedConnection);

    Scheduler::instance()->submitTask(task);
}

void ComunicateLogWidget::onPageWithConditionsResult(const QList<CommunicateRecord>& records)
{
    setHistoryLogData(records);
}

void ComunicateLogWidget::onTotalCountWithConditionsResult(int totalCount)
{
    const int pageSize = (m_pageSize > 0) ? m_pageSize : 1;
    m_totalPages = (totalCount + pageSize - 1) / pageSize;
    ui->widgetPaginate->setTotalPages(m_totalPages);
    ui->widgetPaginate->setCurrentPage(m_currentPage);
    qDebug() << "[ComunicateLogWidget] 条件查询 当前页/总页数:"
             << m_currentPage << "/" << m_totalPages
             << " 总记录数:" << totalCount;
}

void ComunicateLogWidget::setHistoryLogData(const QList<CommunicateRecord>& data)
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
    QList<CommunicateRecord> filtered;
    filtered.reserve(data.size());
    for (const CommunicateRecord& r : data) {
        if (r.userPermission <= currentPerm) {
            filtered.append(r);
        }
    }

    QStringList headers;
    headers << "QRCode" << "Send Time" << "Response Time" << "Command ID"
            << "Exec Status" << "Retry Count"
            << "Request" << "Response" << "Description";
    model->setHorizontalHeaderLabels(headers);

    for (int row = 0; row < filtered.size(); ++row) {
        const CommunicateRecord& r = filtered[row];
        model->setItem(row, 0, new QStandardItem(r.qrCode));
        model->setItem(row, 1, new QStandardItem(r.sendTime));
        model->setItem(row, 2, new QStandardItem(r.responseTime));
        model->setItem(row, 3, new QStandardItem(r.commandId));
        model->setItem(row, 4, new QStandardItem(execStatusToString(r.execStatus)));
        model->setItem(row, 5, new QStandardItem(QString::number(r.retryCount)));
        model->setItem(row, 6, new QStandardItem(QString(r.sendFrame.toHex(' ').toUpper())));
        model->setItem(row, 7, new QStandardItem(QString(r.responseFrame.toHex(' ').toUpper())));
        model->setItem(row, 8, new QStandardItem(r.description));

        // 设置文本对齐：除 Description（第 8 列）外，其他字段居中
        for (int col = 0; col < 8; ++col) {
            if (auto* item = model->item(row, col)) {
                item->setTextAlignment(Qt::AlignCenter);
            }
        }
    }

    // 设置列宽：确保时间字段完整显示
    ui->tableViewHistoryLog->setColumnWidth(0, 80);   // QRCode
    ui->tableViewHistoryLog->setColumnWidth(1, 240);  // Send Time
    ui->tableViewHistoryLog->setColumnWidth(2, 240);  // Response Time
    ui->tableViewHistoryLog->setColumnWidth(3, 100);  // Command ID
    ui->tableViewHistoryLog->setColumnWidth(4, 80);   // Exec Status
    ui->tableViewHistoryLog->setColumnWidth(5, 80);   // Retry Count
    ui->tableViewHistoryLog->setColumnWidth(6, 180);  // Request
    ui->tableViewHistoryLog->setColumnWidth(7, 180);  // Response
    ui->tableViewHistoryLog->setColumnWidth(8, 150);  // Description

    // 根据用户权限设置列的可见性
    updateColumnVisibility();
}

void ComunicateLogWidget::onLiveLogClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // Live log 的 Description 是第 9 列，QRCode 是第 0 列
    constexpr int kColDescription = 9;
    constexpr int kColQRCode = 0;
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

void ComunicateLogWidget::onHistoryLogClicked(const QModelIndex& index)
{
    if (!index.isValid()) return;

    // History log 的 Description 是第 8 列，QRCode 是第 0 列
    constexpr int kColDescription = 8;
    constexpr int kColQRCode = 0;
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
