#include "scrollingtiplabel.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPixmap>
#include "app/alarmtype.h"
#include "data/logdatabases/dbtypes.h"

ScrollingTipLabel::ScrollingTipLabel(QWidget *parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setWordWrap(false);

    m_scrollTimer = new QTimer(this);
    connect(m_scrollTimer, &QTimer::timeout, this, &ScrollingTipLabel::onScrollTimer);

    // 初始状态：无消息，隐藏控件
    updateDisplay();
}

ScrollingTipLabel::~ScrollingTipLabel()
{
    stopScrolling();
}

void ScrollingTipLabel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit clicked();
    QLabel::mousePressEvent(event);
}

static QString makeAlarmKey(const AlarmRecord& record)
{
    return QStringLiteral("%1|%2").arg(record.qrCode).arg(record.alarmType);
}

void ScrollingTipLabel::submitAlarmLog(const AlarmRecord& record)
{
    const QString key = makeAlarmKey(record);
    m_alarmLogs[key] = record;
    if (!m_alarmQueue.contains(key))
        m_alarmQueue.enqueue(key);

    updateDisplay();
}

void ScrollingTipLabel::submitAlarmResolved(const AlarmRecord& record)
{
    const QString key = makeAlarmKey(record);

    // 从警报队列中移除对应的警报记录
    QQueue<QString> newQueue;
    while (!m_alarmQueue.isEmpty()) {
        QString k = m_alarmQueue.dequeue();
        if (k != key) {
            newQueue.enqueue(k);
        }
    }
    m_alarmQueue = newQueue;

    // 从映射中移除
    m_alarmLogs.remove(key);

    // 更新显示
    updateDisplay();
}

void ScrollingTipLabel::submitOperationLog(const OperationRecord& record)
{
    m_operationLog = record;
    updateDisplay();
}

QString ScrollingTipLabel::getCurrentDisplayText()
{
    if (!m_alarmQueue.isEmpty())
        return formatAlarmRecord(m_alarmLogs.value(m_alarmQueue.last()));

    return formatOperationRecord(m_operationLog);
}

void ScrollingTipLabel::updateDisplay()
{
    QString text = getCurrentDisplayText();

    if (text.isEmpty()) {
        setText("");
        stopScrolling();
        updateStyle(false);
        hide();  // 没有消息时隐藏控件
        return;
    }

    show();  // 有消息时显示控件

    // 判断是否为警报模式
    bool isAlarm = !m_alarmQueue.isEmpty();
    m_isAlarmMode = isAlarm;

    // 更新样式
    updateStyle(isAlarm);

    QFontMetrics fm(font());
    int labelWidth = width();
    m_textWidth = fm.horizontalAdvance(text);

    // 字符串长度大于 label 显示范围，滚动显示
    if (m_textWidth > labelWidth) {
        m_currentText = text;
        m_scrollOffset = 0;
        // 立即渲染初始帧（文本从左边缘开始），避免先显示静态文本再跳到滚动状态
        renderScrollFrame();
        startScrolling();
    }
    // 字符串长度小于等于 label 显示范围，居中显示
    else {
        setAlignment(Qt::AlignCenter);
        setText(text);
        stopScrolling();
    }
}

void ScrollingTipLabel::updateStyle(bool isAlarm)
{
    if (isAlarm) {
        // 警报模式样式：红色背景，更显眼
        setStyleSheet(
            "QLabel {"
            "  background-color: #fff5f5;"
            "  border: 1px solid #ff6b6b;"
            "  border-radius: 4px;"
            "  padding: 6px 10px;"
            "  color: #c92a2a;"
            "  font-size: 13px;"
            "  font-weight: 500;"
            "}"
        );
    } else {
        // 普通模式样式：浅色背景，简洁
        setStyleSheet(
            "QLabel {"
            "  background-color: #f8f9fa;"
            "  border: 1px solid #dee2e6;"
            "  border-radius: 4px;"
            "  padding: 6px 10px;"
            "  color: #495057;"
            "  font-size: 13px;"
            "}"
        );
    }
}

void ScrollingTipLabel::startScrolling()
{
    if (m_scrollTimer && !m_scrollTimer->isActive()) {
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        m_scrollTimer->start(kScrollIntervalMs);
    }
}

void ScrollingTipLabel::stopScrolling()
{
    if (m_scrollTimer && m_scrollTimer->isActive()) {
        m_scrollTimer->stop();
    }
    m_scrollOffset = 0;
}

QString ScrollingTipLabel::formatAlarmRecord(const AlarmRecord& record) const
{
    if (record.occurTime.isEmpty() && record.description.isEmpty())
        return {};
    return QString("[%1][%2] %3: %4")
        .arg(record.occurTime,
             alarmLevelName(record.alarmLevel),
             alarmTypeName(record.alarmType),
             record.description);
}

QString ScrollingTipLabel::formatOperationRecord(const OperationRecord& record) const
{
    if (record.occurTime.isEmpty() && record.description.isEmpty())
        return {};
    return QString("[%1] %2: %3")
        .arg(record.occurTime,
             LogDB::operationLogTypeName(record.logType),
             record.description);
}

void ScrollingTipLabel::onScrollTimer()
{
    if (m_currentText.isEmpty()) {
        stopScrolling();
        return;
    }

    int labelWidth = width();

    // 更新滚动偏移
    m_scrollOffset += kScrollStep;

    // 检查是否滚动完毕（文本尾部到达标签中间）
    // 需要滚动到：文本宽度 + 标签宽度/2
    if (m_scrollOffset > m_textWidth + labelWidth / 2) {
        // 整个字符串完全滚出，从头开始
        m_scrollOffset = 0;
    }

    renderScrollFrame();
}

void ScrollingTipLabel::renderScrollFrame()
{
    QFontMetrics fm(font());
    int labelWidth = width();

    // 使用 QPainter 实现像素级精确滚动
    QPixmap pixmap(labelWidth, height());
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::WindowText));

    // 计算文本绘制位置（从左向右方向向左滚动）
    // offset=0: 文本左边缘对齐标签左边缘（显示 [12345...]）
    // offset=textWidth: 文本右边缘到达标签左边缘（文本完全滚出左侧）
    // offset=textWidth-labelWidth/2: 文本尾部字符到达标签中间
    int xPos = -m_scrollOffset;

    // 计算垂直居中位置
    int yPos = (height() + fm.ascent() - fm.descent()) / 2;

    // 绘制文本（当前副本）
    painter.drawText(xPos, yPos, m_currentText);

    // 绘制文本（下一个副本，紧跟在当前副本之后，实现无缝循环）
    // 两个副本之间的间距为 labelWidth/2，确保尾部到达中间时头部刚好从后端出现
    int nextXPos = xPos + m_textWidth + labelWidth / 2;
    painter.drawText(nextXPos, yPos, m_currentText);

    painter.end();

    setPixmap(pixmap);
}
