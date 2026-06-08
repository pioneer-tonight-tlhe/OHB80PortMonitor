#ifndef HIGHLIGHTCALENDAR_H
#define HIGHLIGHTCALENDAR_H

#include <QCalendarWidget>
#include <QDate>
#include <QSet>

// ====================================================================
// HighlightCalendar —— 自定义日历控件
//   有日志记录的日期：蓝色背景
//   选中的日期：红色背景 + 白色文字
//   无日志记录的日期：浅灰色文字
// ====================================================================
class HighlightCalendar : public QCalendarWidget
{
    Q_OBJECT
public:
    explicit HighlightCalendar(QWidget *parent = nullptr);

    void setAvailableDates(const QSet<QDate> &dates);
    void setClickedDate(const QDate &date);

protected:
    void paintCell(QPainter *painter, const QRect &rect,
                   const QDate &date) const override;

private:
    QSet<QDate> m_availableDates;
    QDate       m_clickedDate;
};

#endif // HIGHLIGHTCALENDAR_H
