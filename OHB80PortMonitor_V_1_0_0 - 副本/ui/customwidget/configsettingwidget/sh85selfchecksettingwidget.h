#ifndef SH85SELFCHECKSETTINGWIDGET_H
#define SH85SELFCHECKSETTINGWIDGET_H

#include "settingwidget.h"
#include "scheduler/tasks/sh85selfchecktask/sh85_periodic_self_check_task2.h"
#include "modbustcpmastermanager/modbustcpmaster/sh85selfchecker.h"

#include <QList>
#include <QMetaObject>
#include <QPushButton>
#include <QSpinBox>
#include <QString>

class SettingItemWidget;

class SH85SelfCheckSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit SH85SelfCheckSettingWidget(QWidget *parent = nullptr);
    ~SH85SelfCheckSettingWidget();

    void setEnabled(bool enabled);
    void setCheckActionEnabled(bool enabled);
    bool isRunning() const;

signals:
    void runningStateChanged(bool running);

private slots:
    void onCheckBtnClicked();
    void onCountdownTick(int remainingSeconds, const QString &qrcode);
    void onStatusChanged(const QString &text, const QString &qrcode);
    void onAllFinished(bool success,
                       SH85SelfChecker::Result result,
                       const QString &qrcode);

private:
    void initUI();
    void initDeviceIdItem();
    void initSelfCheckItem();
    void submitSelfCheckTask(const QString &qrcode);
    void resetButton();
    void refreshActionState();
    void disconnectTaskSignals();

    static QString resultToFriendlyText(SH85SelfChecker::Result r);

private:
    QSpinBox *m_deviceIdSpinBox = nullptr;
    QPushButton *m_checkBtn = nullptr;

    SettingItemWidget *m_deviceIdItem = nullptr;
    SettingItemWidget *m_selfCheckItem = nullptr;

    QString m_runningQrcode;
    bool m_checkActionEnabled = true;
    QList<QMetaObject::Connection> m_taskConnections;
};

#endif // SH85SELFCHECKSETTINGWIDGET_H
