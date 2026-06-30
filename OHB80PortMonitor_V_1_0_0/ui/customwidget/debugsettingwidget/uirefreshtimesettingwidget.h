#ifndef UIREFRESHTIMESETTINGWIDGET_H
#define UIREFRESHTIMESETTINGWIDGET_H

#include "settingwidget.h"

#include <QPushButton>
#include <QSpinBox>
#include <QStringList>

class SettingItemWidget;

class UIRefreshTimeSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit UIRefreshTimeSettingWidget(QWidget *parent = nullptr);
    ~UIRefreshTimeSettingWidget() override;

    void setInitialConfigValues(int logoTimeSeconds,
                                int pageTotalTimeSeconds,
                                int pageSwitchIntervalSeconds);

private slots:
    void onSetBtnClicked();
    void onSetAllBtnClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initLogoItem();
    void initParamTotalItem();
    void initParamSwitchItem();
    void loadConfigValues(const QString &qrCode);
    void submitTask(const QStringList &qrcodes, int logoSec, int paramTotalSec, int paramSwitchSec);

private:
    QSpinBox *m_qrcodeSpinBox = nullptr;
    QSpinBox *m_logoSecSpinBox = nullptr;
    QSpinBox *m_paramTotalSpinBox = nullptr;
    QSpinBox *m_paramSwitchSpinBox = nullptr;

    SettingItemWidget *m_qrcodeItem = nullptr;
    SettingItemWidget *m_logoItem = nullptr;
    SettingItemWidget *m_paramTotalItem = nullptr;
    SettingItemWidget *m_paramSwitchItem = nullptr;
};

#endif // UIREFRESHTIMESETTINGWIDGET_H
