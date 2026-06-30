#ifndef FOUPINAUTOPURGEENABLEWIDGET_H
#define FOUPINAUTOPURGEENABLEWIDGET_H

#include "settingwidget.h"

#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>

class SettingItemWidget;

class FoupInAutoPurgeEnableWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit FoupInAutoPurgeEnableWidget(QWidget *parent = nullptr);
    ~FoupInAutoPurgeEnableWidget() override;

    void setInitialConfigValue(int enableValue);

private slots:
    void onSetSingleBtnClicked();
    void onSetAllBtnClicked();

private:
    void initUI();
    void initTargetItem();
    void initControlItem();
    void loadEnableFromConfig(const QString &qrCode);
    void submitWriteTask(const QString &qrcode, quint16 enableValue);
    void submitWriteAllTask(const QStringList &qrcodes, quint16 enableValue);
    void setButtonsEnabled(bool enabled);

    quint16 currentEnableValue() const;
    static QString valueDisplayText(quint16 value);

private:
    QSpinBox *m_qrcodeSpinBox = nullptr;
    QComboBox *m_enableComboBox = nullptr;
    QPushButton *m_setSingleBtn = nullptr;
    QPushButton *m_setAllBtn = nullptr;
    SettingItemWidget *m_targetItem = nullptr;
    SettingItemWidget *m_controlItem = nullptr;
};

#endif // FOUPINAUTOPURGEENABLEWIDGET_H
