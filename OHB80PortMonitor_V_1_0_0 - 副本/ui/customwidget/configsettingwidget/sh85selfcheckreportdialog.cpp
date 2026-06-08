#include "sh85selfcheckreportdialog.h"

#include <QAbstractItemView>
#include <QColor>
#include <QHeaderView>
#include <QScroller>
#include <QScrollerProperties>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>

namespace {
constexpr int kLiveColCount    = 5;
constexpr int kHistoryColCount = 5;

// Live Log 鍒楃储寮?enum LiveCol {
    LiveColQRCode       = 0,
    LiveColState        = 1,
    LiveColCountdown    = 2,
    LiveColSuccess      = 3,
    LiveColParticipated = 4
};

// History Log 鍒楃储寮?enum HistoryCol {
    HistColStartTime    = 0,
    HistColSuccess      = 1,
    HistColFailure      = 2,
    HistColParticipated = 3,
    HistColDescription  = 4
};

QString stateText(SH85SelfChecker::State s) {
    const QString cn = SH85SelfChecker::stateToString(s);
    if (cn == QStringLiteral("绌洪棽")) return QStringLiteral("Idle");
    if (cn == QStringLiteral("涓嬪彂鑷鎸囦护涓?)) return QStringLiteral("Sending self-check command");
    if (cn == QStringLiteral("绛夊緟 5s锛堥樁娈?鍓嶏級")) return QStringLiteral("Waiting 5s (before phase 1)");
    if (cn == QStringLiteral("闃舵1璇诲彇鑷鐘舵€佷腑")) return QStringLiteral("Reading self-check status (phase 1)");
    if (cn == QStringLiteral("绛夊緟 55s锛堥樁娈?鍓嶏級")) return QStringLiteral("Waiting 55s (before phase 2)");
    if (cn == QStringLiteral("杞鑷鐘舵€佷腑")) return QStringLiteral("Polling self-check status");
    if (cn == QStringLiteral("缁撴潫")) return QStringLiteral("Done");
    return cn; // fallback
}
} // namespace

// ============================================================
// 鏋勯€?/ 鏋愭瀯
// ============================================================

SH85SelfCheckReportDialog::SH85SelfCheckReportDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("SH85 Periodic Self-check Report"));
    resize(900, 600);
    initUI();
}

SH85SelfCheckReportDialog::~SH85SelfCheckReportDialog() = default;

// ============================================================
// UI 鍒濆鍖?// ============================================================

void SH85SelfCheckReportDialog::initUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    m_tabWidget = new QTabWidget(this);
    initLiveLogTab();
    initHistoryLogTab();

    m_tabWidget->addTab(m_liveTable,    QStringLiteral("Live Log"));
    m_tabWidget->addTab(m_historyTable, QStringLiteral("History Log"));

    mainLayout->addWidget(m_tabWidget);

    // 鍚敤瑙︽懜/榧犳爣鎷栧姩婊氬姩鎵嬪娍锛堟敮鎸佽Е灞忔粦鍔ㄨ〃鏍硷級鍚屾椂璁剧疆婊氬姩鏉￠粯璁?hover 鑹?    auto enableTouchScroll = [](QAbstractItemView* view) {
        if (!view) return;
        QScroller::grabGesture(view->viewport(), QScroller::LeftMouseButtonGesture);
        QScroller* scroller = QScroller::scroller(view->viewport());
        QScrollerProperties props = scroller->scrollerProperties();
        props.setScrollMetric(QScrollerProperties::DragStartDistance, 0.005);
        props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.3);
        props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0.1);
        scroller->setScrollerProperties(props);
        // 婊氬姩鏉?handle 榛樿鍗充负 hover 鑹诧紝鏂逛究鐢ㄦ埛鐪嬪埌婊氬姩浣嶇疆
        const QString scrollHandleStyle =
            "QScrollBar::handle:vertical{background:#D4D0C8;}"
            "QScrollBar::handle:horizontal{background:#D4D0C8;}";
        view->setStyleSheet(view->styleSheet() + scrollHandleStyle);
    };
    enableTouchScroll(m_liveTable);
    enableTouchScroll(m_historyTable);
}

void SH85SelfCheckReportDialog::initLiveLogTab()
{
    m_liveModel = new QStandardItemModel(0, kLiveColCount, this);
    m_liveModel->setHorizontalHeaderLabels({
        QStringLiteral("QRCode"),
        QStringLiteral("Execution Status"),
        QStringLiteral("Countdown(s)"),
        QStringLiteral("Success"),
        QStringLiteral("Participated")
    });

    m_liveTable = new QTableView(this);
    m_liveTable->setModel(m_liveModel);
    m_liveTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_liveTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_liveTable->verticalHeader()->setVisible(false);
    m_liveTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_liveTable->setAlternatingRowColors(true);
}

void SH85SelfCheckReportDialog::initHistoryLogTab()
{
    m_historyModel = new QStandardItemModel(0, kHistoryColCount, this);
    m_historyModel->setHorizontalHeaderLabels({
        QStringLiteral("Last Check Start Time"),
        QStringLiteral("Success Count"),
        QStringLiteral("Failure Count"),
        QStringLiteral("Participated"),
        QStringLiteral("Description")
    });

    m_historyTable = new QTableView(this);
    m_historyTable->setModel(m_historyModel);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setColumnWidth(HistColStartTime, 200);
    m_historyTable->horizontalHeader()->setSectionResizeMode(HistColStartTime, QHeaderView::Fixed);
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->setAlternatingRowColors(true);
}

// ============================================================
// 琛?/ qrcode 绠＄悊
// ============================================================

void SH85SelfCheckReportDialog::setQrcodes(const QStringList &qrcodes)
{
    m_qrcodes = qrcodes;
    m_qrcodeToRow.clear();
    m_deviceStats.clear();

    m_liveModel->removeRows(0, m_liveModel->rowCount());
    m_historyModel->removeRows(0, m_historyModel->rowCount());

    for (int row = 0; row < qrcodes.size(); ++row) {
        const QString &qrcode = qrcodes.at(row);
        m_qrcodeToRow.insert(qrcode, row);
        m_deviceStats.insert(qrcode, DeviceStat{});

        // Live Log 琛?        QList<QStandardItem*> liveRow;
        liveRow.append(new QStandardItem(qrcode));
        liveRow.append(new QStandardItem(QStringLiteral("-")));
        liveRow.append(new QStandardItem(QStringLiteral("-")));
        liveRow.append(new QStandardItem(QStringLiteral("-")));
        liveRow.append(new QStandardItem(QStringLiteral("-")));
        for (auto *item : liveRow) {
            item->setTextAlignment(Qt::AlignCenter);
        }
        m_liveModel->appendRow(liveRow);

        // History Log 琛?        QList<QStandardItem*> histRow;
        histRow.append(new QStandardItem(QStringLiteral("-")));
        histRow.append(new QStandardItem(QStringLiteral("0")));
        histRow.append(new QStandardItem(QStringLiteral("0")));
        histRow.append(new QStandardItem(QStringLiteral("-")));
        histRow.append(new QStandardItem(QStringLiteral("-")));
        m_historyModel->appendRow(histRow);
    }
}

int SH85SelfCheckReportDialog::liveRowOf(const QString &qrcode) const
{
    return m_qrcodeToRow.value(qrcode, -1);
}

int SH85SelfCheckReportDialog::historyRowOf(const QString &qrcode) const
{
    return m_qrcodeToRow.value(qrcode, -1);
}

// ============================================================
// 妲斤細Live Log 鏇存柊
// ============================================================

void SH85SelfCheckReportDialog::onRoundStarted()
{
    // 娓呯┖褰撳墠杞鐨勭姸鎬?鍊掕鏃?鏄惁鎴愬姛锛屾墍鏈?Participated 榛樿涓?Yes
    for (int row = 0; row < m_liveModel->rowCount(); ++row) {
        if (auto *it = m_liveModel->item(row, LiveColState))
            it->setText(QStringLiteral("-"));
        if (auto *it = m_liveModel->item(row, LiveColCountdown))
            it->setText(QStringLiteral("-"));
        if (auto *it = m_liveModel->item(row, LiveColSuccess)){
            it->setText(QStringLiteral("-"));
            it->setBackground(QColor(Qt::transparent));
            it->setForeground(Qt::black);
        }
        if (auto *it = m_liveModel->item(row, LiveColParticipated)) it->setText(QStringLiteral("Yes"));
    }
}

void SH85SelfCheckReportDialog::onCheckerCountdown(int remainingSeconds, const QString &masterId)
{
    const int row = liveRowOf(masterId);
    if (row < 0) return;
    if (auto *it = m_liveModel->item(row, LiveColCountdown)) {
        it->setText(QString::number(remainingSeconds));
    }
}

void SH85SelfCheckReportDialog::onCheckerStateChanged(SH85SelfChecker::State state, const QString &masterId)
{
    const int row = liveRowOf(masterId);
    if (row < 0) return;
    if (auto *it = m_liveModel->item(row, LiveColState)) {
        it->setText(stateText(state));
    }
}

void SH85SelfCheckReportDialog::onOneFinished(const QString &masterId, bool success, const QString &description)
{
    Q_UNUSED(description)
    const int row = liveRowOf(masterId);
    if (row < 0) return;

    if (auto *it = m_liveModel->item(row, LiveColSuccess)) {
        it->setText(success ? QStringLiteral("Yes") : QStringLiteral("No"));
        if (success) {
            it->setBackground(QColor("#32CD32")); // Lime Green
        } else {
            it->setBackground(QColor(220, 20, 60)); // Crimson red
            it->setForeground(Qt::white);
        }
    }
}

void SH85SelfCheckReportDialog::onDeviceParticipated(const QString &qrcode, bool participated)
{
    const int row = liveRowOf(qrcode);
    if (row < 0) return;

    if (auto *it = m_liveModel->item(row, LiveColParticipated)) {
        if (!participated) {
            it->setText(QStringLiteral("No"));
        }
        // participated 涓?true 鏃朵笉鍋氬鐞嗭紝鍥犱负榛樿宸茬粡鏄?Yes
    }
}

// ============================================================
// 妲斤細History Log 绱
// ============================================================

void SH85SelfCheckReportDialog::onAllFinished(const SH85PeriodicSelfCheckTask2::SelfCheckSummary &summary)
{
    // 涓€杞粨鏉熷悗锛屾妸 Live Log 鐨勫€掕鏃跺垪鍏ㄩ儴娓呴浂
    for (int row = 0; row < m_liveModel->rowCount(); ++row) {
        if (auto *it = m_liveModel->item(row, LiveColCountdown)) {
            it->setText(QStringLiteral("0"));
        }
    }

    for (const auto &dr : summary.details) {
        const int liveRow = liveRowOf(dr.qrcode);
        if (liveRow >= 0) {
            if (auto *it = m_liveModel->item(liveRow, LiveColParticipated)) {
                it->setText(dr.participated ? QStringLiteral("Yes") : QStringLiteral("No"));
            }
        }

        const int row = historyRowOf(dr.qrcode);
        if (row < 0) continue;

        DeviceStat &stat = m_deviceStats[dr.qrcode];
        stat.lastStartTime    = summary.startTime;
        stat.lastParticipated = dr.participated;
        stat.lastDescription  = dr.description;

        if (dr.participated) {
            if (dr.success) ++stat.successCount;
            else            ++stat.failureCount;
        }

        if (auto *it = m_historyModel->item(row, HistColStartTime))
            it->setText(stat.lastStartTime);
        if (auto *it = m_historyModel->item(row, HistColSuccess))
            it->setText(QString::number(stat.successCount));
        if (auto *it = m_historyModel->item(row, HistColFailure))
            it->setText(QString::number(stat.failureCount));
        if (auto *it = m_historyModel->item(row, HistColParticipated))
            it->setText(stat.lastParticipated ? QStringLiteral("Yes") : QStringLiteral("No"));
        if (auto *it = m_historyModel->item(row, HistColDescription))
            it->setText(stat.lastDescription);
    }
}

