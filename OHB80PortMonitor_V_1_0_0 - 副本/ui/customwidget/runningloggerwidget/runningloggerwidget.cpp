#include "runningloggerwidget.h"
#include "runningloggercollector.h"
#include "customlogger.h"
#include "alarmid.h"
#include "logdatabases/databasemanager.h"
#include "logdatabases/operationlogdb/operationlogdbcon.h"
#include "logdatabases/dbtypes.h"

#include <QVBoxLayout>
#include <QDateTime>
#include <QFontMetrics>

// =====================================================================
// 静态常量
// =====================================================================
const QStringList RunningLoggerWidget::kHeaders = {
    QStringLiteral("Type"),
    QStringLiteral("Time"),
    QStringLiteral("QRCode"),
    QStringLiteral("Alarm ID"),
    QStringLiteral("Resolved"),
    QStringLiteral("Resolve Time"),
    QStringLiteral("Message")
};

// =====================================================================
// 静态方法
// =====================================================================
QString RunningLoggerWidget::msgTypeToString(MsgType type)
{
    switch (type) {
    case MsgType::Message: return QStringLiteral("Message");
    case MsgType::Warn:    return QStringLiteral("Warn");
    case MsgType::Error:   return QStringLiteral("Error");
    default:               return QStringLiteral("Unknown");
    }
}

// =====================================================================
// 构造 / 析构
// =====================================================================

RunningLoggerWidget::RunningLoggerWidget(QWidget *parent)
    : QWidget(parent)
{
    // 自动设置日志根目录为运行日志路径
    m_rootPath = CustomLogger::RunningLoggerPath();

    // ---- 按钮 ----
    m_btn = new QPushButton(tr("No logs"), this);
    m_btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_btn->setMinimumHeight(36);
    m_btn->setCursor(Qt::PointingHandCursor);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_btn);

    // ---- LoggerWidget（parent 为 this，生命周期跟随本控件）----
    m_loggerWidget = new LoggerWidget(this);
    m_loggerWidget->setVisible(false);

    // ---- 模态对话框（不拥有 LoggerWidget 的生命周期）----
    m_dialog = new QDialog(this);
    m_dialog->setWindowTitle(tr("Log Viewer"));
    m_dialog->resize(900, 620);
    QVBoxLayout *dlgLayout = new QVBoxLayout(m_dialog);
    dlgLayout->setContentsMargins(0, 0, 0, 0);

    // ---- 跑马灯定时器（构造时即启动，无需等待 initialize）----
    m_scrollTimer = new QTimer(this);
    m_scrollTimer->setInterval(150);
    connect(m_scrollTimer, &QTimer::timeout,
            this, &RunningLoggerWidget::onScrollTick);
    m_scrollTimer->start();

    // ---- 警报轮播定时器 ----
    m_alarmCycleTimer = new QTimer(this);
    m_alarmCycleTimer->setInterval(3000);
    connect(m_alarmCycleTimer, &QTimer::timeout,
            this, &RunningLoggerWidget::onAlarmCycleTick);

    // ---- 按钮点击 ----
    connect(m_btn, &QPushButton::clicked,
            this, &RunningLoggerWidget::onBtnClicked);
}

RunningLoggerWidget::~RunningLoggerWidget()
{
    // m_loggerWidget 和 m_dialog 均由 this 管理
}

// =====================================================================
// 配置
// =====================================================================

void RunningLoggerWidget::setRootPath(const QString &path)
{
    m_rootPath = path;
}

void RunningLoggerWidget::setPageSize(int size)
{
    m_pageSize = qMax(1, size);
}

// =====================================================================
// 初始化
// =====================================================================

void RunningLoggerWidget::initialize()
{
    // 1. 设置表头
    m_loggerWidget->setHeaders(kHeaders);

    // 2. 设置 list view 显示格式
    //    除了 "Message" 格式为 ": message"，其他字段都用 [] 包裹
    m_loggerWidget->setFormat(
        QStringLiteral("[{}][{}][{}][{}][{}][{}]: {}"),
        kHeaders
    );

    // 3. 设置三种消息类型的字体颜色样式
    m_loggerWidget->setItemStyler(
        [](const QStringList &record, ItemStyle &style) {
            if (record.isEmpty()) return;
            const QString &type = record[0];
            if (type == QStringLiteral("Message")) {
                style.setForeground(QColor(Qt::black));
            } else if (type == QStringLiteral("Warn")) {
                style.setForeground(QColor("#DAA520"));   // 警报黄色（金菊色）
            } else if (type == QStringLiteral("Error")) {
                style.setForeground(QColor("#DC143C"));   // 警报红色（深红色）
            }
        }
    );

    // 4. 设置根目录 / 页大小并初始化
    m_loggerWidget->setRootPath(m_rootPath);
    m_loggerWidget->setPageSize(m_pageSize);
    m_loggerWidget->initialize();

    // 将自身注册到日志采集器，此后任意线程的 logMessage() 将提交到本控件
    RunningLoggerCollector::instance()->setTarget(this);
}

// =====================================================================
// 写入一条记录
// =====================================================================

void RunningLoggerWidget::writeRecord(RunningLoggerWidget::MsgType type,
                                       const QString &qrCode,
                                       const QString &alarmId,
                                       const QString &message)
{
    bool isAlarm = (type == MsgType::Warn || type == MsgType::Error);

    // 若为警报记录且 alarmId 已在待处理队列中，直接忽略
    if (isAlarm && !alarmId.isEmpty()) {
        for (const PendingAlarm &pa : m_pendingAlarms) {
            if (pa.alarmId == alarmId)
                return;
        }
    }

    // 通过 LoggerWidget 写入日志
    QString typeStr;
    switch (type) {
    case MsgType::Message: typeStr = QStringLiteral("Message"); break;
    case MsgType::Warn:    typeStr = QStringLiteral("Warn"); break;
    case MsgType::Error:   typeStr = QStringLiteral("Error"); break;
    }
    QString sendTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QJsonObject record{
        {kHeaders[0], typeStr},
        {kHeaders[1], sendTime},
        {kHeaders[2], qrCode},
        {kHeaders[3], alarmId},
        {kHeaders[4], isAlarm ? QStringLiteral("No") : QString()},
        {kHeaders[5], QString()},
        {kHeaders[6], message}
    };
    m_loggerWidget->writeLog(record);

    // 若为警报记录，加入待处理警报队列
    if (isAlarm && !alarmId.isEmpty()) {
        PendingAlarm pa;
        pa.alarmId  = alarmId;
        pa.type     = type;
        pa.qrCode   = qrCode;
        pa.message  = message;
        pa.sendTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        m_pendingAlarms.append(pa);

        // 启动轮播定时器（若尚未启动）
        if (!m_alarmCycleTimer->isActive())
            m_alarmCycleTimer->start();
    }

    // 若为 Message，更新最新消息文本
    if (type == MsgType::Message) {
        m_latestMessageText = QString("[Message] %1").arg(message);
    }

    if (!m_batchWriting)
        refreshDisplayText();

    // 同步写入 OperationLogDBCon（SQLite 持久化）
    if (auto* db = LogDB::DatabaseManager::instance().operationLogCon()) {
        int logType = static_cast<int>(LogDB::OperationLogType::Message);
        switch (type) {
        case MsgType::Message: logType = static_cast<int>(LogDB::OperationLogType::Message); break;
        case MsgType::Warn:    logType = static_cast<int>(LogDB::OperationLogType::Warn);    break;
        case MsgType::Error:   logType = static_cast<int>(LogDB::OperationLogType::Error);   break;
        }

        // 把 qrCode / alarmId 拼到 description 以保留上下文
        QString desc;
        if (!qrCode.isEmpty())  desc += QStringLiteral("[%1]").arg(qrCode);
        if (!alarmId.isEmpty()) desc += QStringLiteral("[%1]").arg(alarmId);
        if (!desc.isEmpty())    desc += QLatin1Char(' ');
        desc += message;

        db->insertRecord(sendTime, logType, desc);
    }
}

// =====================================================================
// 批量写入模式
// =====================================================================

void RunningLoggerWidget::beginBatchWrite()
{
    m_batchWriting = true;
}

void RunningLoggerWidget::endBatchWrite()
{
    m_batchWriting = false;
    refreshDisplayText();
}

// =====================================================================
// 接收 AlarmLoggerWidget 信号
// =====================================================================

static RunningLoggerWidget::MsgType alarmLevelToMsgType(AlarmLevel level)
{
    switch (level) {
    case AlarmLevel::Error: return RunningLoggerWidget::MsgType::Error;
    default:                return RunningLoggerWidget::MsgType::Warn;
    }
}

void RunningLoggerWidget::onAlarmPublished(const AlarmInfo &info)
{
    writeRecord(alarmLevelToMsgType(info.level()),
                info.qrCode(),
                alarmIdToString(info.alarmId()),
                info.message());
}

void RunningLoggerWidget::onAlarmResolved(const AlarmInfo &info)
{
    resolveAlarm(MsgType::Message,
                 info.qrCode(),
                 alarmIdToString(info.alarmId()),
                 info.message());
}

// =====================================================================
// 解决一条警报
// =====================================================================

void RunningLoggerWidget::resolveAlarm(RunningLoggerWidget::MsgType type,
                                        const QString &qrCode,
                                        const QString &alarmId,
                                        const QString &message)
{
    // 在待处理队列中查找并移除
    int idx = -1;
    for (int i = 0; i < m_pendingAlarms.size(); ++i) {
        if (m_pendingAlarms[i].alarmId == alarmId) {
            idx = i;
            break;
        }
    }
    if (idx >= 0)
        m_pendingAlarms.removeAt(idx);

    // 通过 LoggerWidget 写入"已解决"记录
    QString typeStr;
    switch (type) {
    case MsgType::Message: typeStr = QStringLiteral("Message"); break;
    case MsgType::Warn:    typeStr = QStringLiteral("Warn"); break;
    case MsgType::Error:   typeStr = QStringLiteral("Error"); break;
    }
    QString sendTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString resolveTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QJsonObject record{
        {kHeaders[0], typeStr},
        {kHeaders[1], sendTime},
        {kHeaders[2], qrCode},
        {kHeaders[3], alarmId},
        {kHeaders[4], QStringLiteral("Yes")},
        {kHeaders[5], resolveTime},
        {kHeaders[6], message}
    };
    m_loggerWidget->writeLog(record);

    // 调整轮播索引
    if (m_pendingAlarms.isEmpty()) {
        m_alarmCycleTimer->stop();
        m_currentAlarmIdx = 0;
    } else if (m_currentAlarmIdx >= m_pendingAlarms.size()) {
        m_currentAlarmIdx = 0;
    }

    refreshDisplayText();
}

// =====================================================================
// 私有槽
// =====================================================================

void RunningLoggerWidget::onBtnClicked()
{
    // 临时将 LoggerWidget 放入对话框布局中显示
    m_dialog->layout()->addWidget(m_loggerWidget);
    m_loggerWidget->show();
    m_dialog->exec();
    // 对话框关闭后，将 LoggerWidget 收回，恢复 parent 为 this
    m_dialog->layout()->removeWidget(m_loggerWidget);
    m_loggerWidget->setParent(this);
    m_loggerWidget->hide();
}

void RunningLoggerWidget::onScrollTick()
{
    if (m_fullDisplayText.isEmpty()) {
        m_btn->setText(tr("No logs"));
        return;
    }

    QFontMetrics fm = m_btn->fontMetrics();
    int btnWidth  = m_btn->width() - 24;   // 留出左右内边距
    int textWidth = fm.horizontalAdvance(m_fullDisplayText);

    // 文本能完整显示时，不滚动
    if (textWidth <= btnWidth) {
        m_btn->setText(m_fullDisplayText);
        return;
    }

    // 跑马灯：使用缓存的拼接字符串，避免每 tick 重新分配
    int avgCharW     = qMax(1, fm.averageCharWidth());
    int visibleChars = btnWidth / avgCharW;

    QString visible = m_scrollDoubled.mid(m_scrollOffset, visibleChars);
    m_btn->setText(visible);

    m_scrollOffset++;
    if (m_scrollOffset >= m_scrollTotalLen)
        m_scrollOffset = 0;
}

void RunningLoggerWidget::onAlarmCycleTick()
{
    if (m_pendingAlarms.isEmpty()) {
        m_alarmCycleTimer->stop();
        return;
    }
    m_currentAlarmIdx = (m_currentAlarmIdx + 1) % m_pendingAlarms.size();
    refreshDisplayText();
}

// =====================================================================
// 内部方法
// =====================================================================

void RunningLoggerWidget::refreshDisplayText()
{
    if (!m_pendingAlarms.isEmpty()) {
        // 显示当前轮播的警报消息
        if (m_currentAlarmIdx >= m_pendingAlarms.size())
            m_currentAlarmIdx = 0;
        const PendingAlarm &alarm = m_pendingAlarms[m_currentAlarmIdx];
        m_fullDisplayText = QString("[%1] %2")
                                .arg(msgTypeToString(alarm.type))
                                .arg(alarm.message);
    } else {
        // 无警报时显示最新 Message
        m_fullDisplayText = m_latestMessageText.isEmpty()
                                ? tr("No logs")
                                : m_latestMessageText;
    }

    // 缓存跑马灯拼接字符串，避免每 tick 重新分配
    QString padded   = m_fullDisplayText + QStringLiteral("    ");
    m_scrollTotalLen = padded.length();
    m_scrollDoubled  = padded + m_fullDisplayText;

    // 重置跑马灯偏移，并立即刷新按钮文字
    m_scrollOffset = 0;
    m_btn->setText(m_fullDisplayText);
}

