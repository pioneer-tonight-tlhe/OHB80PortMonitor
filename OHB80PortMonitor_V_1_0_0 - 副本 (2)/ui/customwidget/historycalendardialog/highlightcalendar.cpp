#include "highlightcalendar.h"
#include <QPainter>

HighlightCalendar::HighlightCalendar(QWidget *parent)
    : QCalendarWidget(parent)
{
}

void HighlightCalendar::setAvailableDates(const QSet<QDate> &dates)
{
    m_availableDates = dates;
    updateCells();
}

void HighlightCalendar::setClickedDate(const QDate &date)
{
    m_clickedDate = date;
    updateCells();
}

void HighlightCalendar::paintCell(QPainter *painter, const QRect &rect,
                                   const QDate &date) const
{
    painter->save();

    const bool isCurrentMonth =
        (date.month() == monthShown() && date.year() == yearShown());
    const bool available = m_availableDates.contains(date);
    const bool clicked   = (date == m_clickedDate && available);

    // ---- 背景 ----
    if (clicked) {
        // 选中的日期：红色背景
        painter->fillRect(rect, QColor(220, 50, 50));
    } else if (available) {
        // 有日志的日期：蓝色背景
        painter->fillRect(rect, QColor(0x30, 0x80, 0xE8));
    }

    // ---- 文字颜色 ----
    QColor textColor;
    if (clicked) {
        textColor = Qt::white;
    } else if (available) {
        textColor = Qt::black;
    } else if (isCurrentMonth) {
        textColor = QColor(200, 200, 200);
    } else {
        textColor = QColor(220, 220, 220);
    }

    painter->setPen(textColor);
    painter->drawText(rect, Qt::AlignCenter, QString::number(date.day()));

    painter->restore();
}
