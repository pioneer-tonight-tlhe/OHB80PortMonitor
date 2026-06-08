#include "historycalendardialog.h"
#include "ui_historycalendardialog.h"
#include "highlightcalendar.h"
#include <QComboBox>
#include <QCalendarWidget>
#include <QTextCharFormat>

HistoryCalendarDialog::HistoryCalendarDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::HistoryCalendarDialog)
{
    ui->setupUi(this);
    initCalendarStyle();
    connectSignals();

    ui->btnCancel->setStyleSheet(
        "QPushButton {"
        "    background-color: white;"
        "    border: 1px solid #d0d0d0;"
        "    border-radius: 5px;"
        "    padding: 8px 20px;"
        "    font-size: 11pt;"
        "}"
        "QPushButton:hover {"
        "    background-color: #f5f5f5;"
        "}"
    );
}

HistoryCalendarDialog::~HistoryCalendarDialog()
{
    delete ui;
}

void HistoryCalendarDialog::initCalendarStyle()
{
    // 表头浅灰色
    QTextCharFormat headerFormat;
    headerFormat.setForeground(QColor(200, 200, 200));
    ui->calendarWidget->setHeaderTextFormat(headerFormat);
}

void HistoryCalendarDialog::connectSignals()
{
    connect(ui->comboYear, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryCalendarDialog::onYearChanged);
    connect(ui->comboMonth, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryCalendarDialog::onMonthChanged);
    connect(ui->calendarWidget, &QCalendarWidget::currentPageChanged,
            this, &HistoryCalendarDialog::onCalendarPageChanged);
    connect(ui->calendarWidget, &QCalendarWidget::clicked,
            this, &HistoryCalendarDialog::onDateClicked);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(ui->btnConfirm, &QPushButton::clicked, this, &HistoryCalendarDialog::onConfirm);
}

void HistoryCalendarDialog::setAvailableDates(const QSet<QDate> &dates)
{
    m_availableDates = dates;
    ui->calendarWidget->setAvailableDates(dates);
    populateYearCombo();
    populateMonthCombo();
    updateCalendarFormat();
}

QDate HistoryCalendarDialog::selectedDate() const
{
    return ui->calendarWidget->selectedDate();
}

void HistoryCalendarDialog::setSelectedDate(const QDate &date)
{
    if (date.isValid()) {
        int yearIdx = ui->comboYear->findData(date.year());
        if (yearIdx >= 0) {
            ui->comboYear->setCurrentIndex(yearIdx);
            populateMonthCombo();
            int monthIdx = ui->comboMonth->findData(date.month());
            if (monthIdx >= 0) {
                ui->comboMonth->setCurrentIndex(monthIdx);
            }
        }
        ui->calendarWidget->setSelectedDate(date);
        if (m_availableDates.contains(date)) {
            m_lastValidDate = date;
            m_selectedDate = date;
            ui->calendarWidget->setClickedDate(date);
        }
        updateCalendarFormat();
    }
}

void HistoryCalendarDialog::onYearChanged(int index)
{
    if (index < 0) return;
    populateMonthCombo();
    int year = ui->comboYear->currentData().toInt();
    int month = ui->comboMonth->currentData().toInt();
    QDate currentDate = ui->calendarWidget->selectedDate();
    QDate newDate(year, month, qMin(currentDate.day(),
                  QDate(year, month, 1).daysInMonth()));
    ui->calendarWidget->setSelectedDate(newDate);
    updateCalendarFormat();
}

void HistoryCalendarDialog::onMonthChanged(int index)
{
    if (index < 0) return;
    int year = ui->comboYear->currentData().toInt();
    int month = ui->comboMonth->currentData().toInt();
    QDate currentDate = ui->calendarWidget->selectedDate();
    QDate newDate(year, month, qMin(currentDate.day(),
                  QDate(year, month, 1).daysInMonth()));
    ui->calendarWidget->setSelectedDate(newDate);
    updateCalendarFormat();
}

void HistoryCalendarDialog::onCalendarPageChanged(int year, int month)
{
    ui->comboYear->blockSignals(true);
    ui->comboMonth->blockSignals(true);
    int yearIdx = ui->comboYear->findData(year);
    if (yearIdx >= 0) {
        ui->comboYear->setCurrentIndex(yearIdx);
        populateMonthCombo();
        int monthIdx = ui->comboMonth->findData(month);
        if (monthIdx >= 0) {
            ui->comboMonth->setCurrentIndex(monthIdx);
        }
    }
    ui->comboYear->blockSignals(false);
    ui->comboMonth->blockSignals(false);
    updateCalendarFormat();
}

void HistoryCalendarDialog::onConfirm()
{
    emit dateSelected(ui->calendarWidget->selectedDate());
    accept();
}

void HistoryCalendarDialog::onDateClicked(const QDate &date)
{
    if (m_availableDates.contains(date)) {
        m_lastValidDate = date;
        m_selectedDate = date;
        ui->calendarWidget->setClickedDate(date);
    } else {
        ui->calendarWidget->setSelectedDate(m_lastValidDate);
    }
}

void HistoryCalendarDialog::populateYearCombo()
{
    QSet<int> years;
    for (const QDate &date : m_availableDates) {
        years.insert(date.year());
    }
    QList<int> sortedYears = years.values();
    std::sort(sortedYears.begin(), sortedYears.end());

    ui->comboYear->blockSignals(true);
    ui->comboYear->clear();
    for (int y : sortedYears) {
        ui->comboYear->addItem(QString::number(y), y);
    }
    ui->comboYear->blockSignals(false);
}

void HistoryCalendarDialog::populateMonthCombo()
{
    int year = ui->comboYear->currentData().toInt();
    QSet<int> months;
    for (const QDate &date : m_availableDates) {
        if (date.year() == year) {
            months.insert(date.month());
        }
    }
    QList<int> sortedMonths = months.values();
    std::sort(sortedMonths.begin(), sortedMonths.end());

    ui->comboMonth->blockSignals(true);
    ui->comboMonth->clear();
    for (int m : sortedMonths) {
        ui->comboMonth->addItem(QString("Month %1").arg(m), m);
    }
    ui->comboMonth->blockSignals(false);
}

void HistoryCalendarDialog::updateCalendarFormat()
{
    int year = ui->comboYear->currentData().toInt();
    int month = ui->comboMonth->currentData().toInt();
    if (year == 0 || month == 0) return;

    ui->calendarWidget->setCurrentPage(year, month);
}
