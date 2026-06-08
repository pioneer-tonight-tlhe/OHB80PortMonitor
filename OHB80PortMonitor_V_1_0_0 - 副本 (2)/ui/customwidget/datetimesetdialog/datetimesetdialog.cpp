#include "datetimesetdialog.h"
#include "ui_datetimesetdialog.h"
#include <QMessageBox>

DateTimeSetDialog::DateTimeSetDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DateTimeSetDialog)
{
    ui->setupUi(this);

    connect(ui->pushButtonOK, &QPushButton::clicked, this, &DateTimeSetDialog::onOkClicked);
    connect(ui->pushButtonCancel, &QPushButton::clicked, this, &DateTimeSetDialog::onCancelClicked);
    
    connect(ui->checkBoxEnableStartTime, &QCheckBox::stateChanged, this, [this](int state) {
        bool enabled = (state == Qt::Checked);
        ui->dateEditStart->setEnabled(enabled);
        ui->timeEditStart->setEnabled(enabled);
    });
    
    connect(ui->checkBoxEnableEndTime, &QCheckBox::stateChanged, this, [this](int state) {
        bool enabled = (state == Qt::Checked);
        ui->dateEditEnd->setEnabled(enabled);
        ui->timeEditEnd->setEnabled(enabled);
    });
    
    // 设置默认时间：结束时间为当前时间，开始时间向前推移30分钟
    QDateTime current = QDateTime::currentDateTime();
    ui->dateEditEnd->setDate(current.date());
    ui->timeEditEnd->setTime(current.time());
    ui->dateEditStart->setDate(current.date());
    ui->timeEditStart->setTime(current.time().addSecs(-30 * 60));
    
    // 默认启用两个时间
    ui->checkBoxEnableStartTime->setChecked(true);
    ui->checkBoxEnableEndTime->setChecked(true);
}

DateTimeSetDialog::~DateTimeSetDialog()
{
    delete ui;
}

QString DateTimeSetDialog::getStartTime() const
{
    if (!ui->checkBoxEnableStartTime->isChecked()) {
        return QString();
    }
    // 防护：如果同时启用开始 / 结束且范围不合理（开始 >= 结束），返回空字符串
    if (ui->checkBoxEnableEndTime->isChecked()) {
        const QDateTime s(ui->dateEditStart->date(), ui->timeEditStart->time());
        const QDateTime e(ui->dateEditEnd->date(),   ui->timeEditEnd->time());
        if (s >= e) {
            return QString();
        }
    }
    QDateTime dateTime(ui->dateEditStart->date(), ui->timeEditStart->time());
    return dateTime.toString("yyyy-MM-dd HH:mm:ss");
}

QString DateTimeSetDialog::getEndTime() const
{
    if (!ui->checkBoxEnableEndTime->isChecked()) {
        return QString();
    }
    // 防护：如果同时启用开始 / 结束且范围不合理（开始 >= 结束），返回空字符串
    if (ui->checkBoxEnableStartTime->isChecked()) {
        const QDateTime s(ui->dateEditStart->date(), ui->timeEditStart->time());
        const QDateTime e(ui->dateEditEnd->date(),   ui->timeEditEnd->time());
        if (s >= e) {
            return QString();
        }
    }
    QDateTime dateTime(ui->dateEditEnd->date(), ui->timeEditEnd->time());
    return dateTime.toString("yyyy-MM-dd HH:mm:ss");
}

void DateTimeSetDialog::setStartTime(const QString& time)
{
    if (time.isEmpty()) {
        ui->checkBoxEnableStartTime->setChecked(false);
        return;
    }
    QDateTime dateTime = QDateTime::fromString(time, "yyyy-MM-dd HH:mm:ss");
    if (dateTime.isValid()) {
        ui->dateEditStart->setDate(dateTime.date());
        ui->timeEditStart->setTime(dateTime.time());
        ui->checkBoxEnableStartTime->setChecked(true);
    }
}

void DateTimeSetDialog::setEndTime(const QString& time)
{
    if (time.isEmpty()) {
        ui->checkBoxEnableEndTime->setChecked(false);
        return;
    }
    QDateTime dateTime = QDateTime::fromString(time, "yyyy-MM-dd HH:mm:ss");
    if (dateTime.isValid()) {
        ui->dateEditEnd->setDate(dateTime.date());
        ui->timeEditEnd->setTime(dateTime.time());
        ui->checkBoxEnableEndTime->setChecked(true);
    }
}

bool DateTimeSetDialog::isStartTimeEnabled() const
{
    return ui->checkBoxEnableStartTime->isChecked();
}

bool DateTimeSetDialog::isEndTimeEnabled() const
{
    return ui->checkBoxEnableEndTime->isChecked();
}

void DateTimeSetDialog::setStartTimeEnabled(bool enabled)
{
    ui->checkBoxEnableStartTime->setChecked(enabled);
}

void DateTimeSetDialog::setEndTimeEnabled(bool enabled)
{
    ui->checkBoxEnableEndTime->setChecked(enabled);
}

void DateTimeSetDialog::onOkClicked()
{
    // 同时启用开始与结束时间时，校验范围是否合理
    if (ui->checkBoxEnableStartTime->isChecked() && ui->checkBoxEnableEndTime->isChecked()) {
        const QDateTime s(ui->dateEditStart->date(), ui->timeEditStart->time());
        const QDateTime e(ui->dateEditEnd->date(),   ui->timeEditEnd->time());
        if (s >= e) {
            QMessageBox::warning(this,
                                 tr("Invalid Time Range"),
                                 tr("Start time must be earlier than end time. Please adjust and try again."));
            return; // 不关闭对话框，让用户继续修改
        }
    }
    accept();
}

void DateTimeSetDialog::onCancelClicked()
{
    reject();
}
