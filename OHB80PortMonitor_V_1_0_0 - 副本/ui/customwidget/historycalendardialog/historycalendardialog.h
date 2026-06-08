#ifndef HISTORYCALENDARDIALOG_H
#define HISTORYCALENDARDIALOG_H

#include <QDialog>
#include <QDate>
#include <QSet>

QT_BEGIN_NAMESPACE
namespace Ui {
class HistoryCalendarDialog;
}
QT_END_NAMESPACE

class HistoryCalendarDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryCalendarDialog(QWidget *parent = nullptr);
    ~HistoryCalendarDialog();

    void setAvailableDates(const QSet<QDate> &dates);
    QDate selectedDate() const;
    void setSelectedDate(const QDate &date);

signals:
    void dateSelected(const QDate &date);

private slots:
    void onYearChanged(int index);
    void onMonthChanged(int index);
    void onCalendarPageChanged(int year, int month);
    void onConfirm();
    void onDateClicked(const QDate &date);
    void updateCalendarFormat();
    void populateYearCombo();
    void populateMonthCombo();

private:
    void initCalendarStyle();
    void connectSignals();

    Ui::HistoryCalendarDialog *ui;
    QSet<QDate> m_availableDates;
    QDate m_lastValidDate;
    QDate m_selectedDate;
};

#endif // HISTORYCALENDARDIALOG_H
