#ifndef DATETIMESETDIALOG_H
#define DATETIMESETDIALOG_H

#include <QDialog>
#include <QDateTime>

namespace Ui {
class DateTimeSetDialog;
}

class DateTimeSetDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DateTimeSetDialog(QWidget *parent = nullptr);
    ~DateTimeSetDialog();

    QString getStartTime() const;
    QString getEndTime() const;
    void setStartTime(const QString& time);
    void setEndTime(const QString& time);
    
    bool isStartTimeEnabled() const;
    bool isEndTimeEnabled() const;
    void setStartTimeEnabled(bool enabled);
    void setEndTimeEnabled(bool enabled);

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    Ui::DateTimeSetDialog *ui;
};

#endif // DATETIMESETDIALOG_H
