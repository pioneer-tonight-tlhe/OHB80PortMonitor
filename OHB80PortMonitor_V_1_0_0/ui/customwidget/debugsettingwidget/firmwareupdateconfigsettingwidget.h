#ifndef FIRMWAREUPDATECONFIGSETTINGWIDGET_H
#define FIRMWAREUPDATECONFIGSETTINGWIDGET_H

#include "../settingwidget/settingwidget.h"

#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <functional>

class SettingItemWidget;
class SetFirmwareConfigTask;

class FirmwareUpdateConfigSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit FirmwareUpdateConfigSettingWidget(QWidget *parent = nullptr);
    ~FirmwareUpdateConfigSettingWidget() override;

    QString binFilePath() const;

signals:
    void binFilePathChanged(const QString &filePath);

private slots:
    void onLoadBinFileBtnClicked();
    void onPrepareTimeoutSetBtnClicked();
    void onWaitingTimeSetBtnClicked();
    void onSendIntervalSetBtnClicked();
    void onTransferTimeoutSetBtnClicked();
    void onPostTransferWaitTimeSetBtnClicked();

private:
    void initUI();
    void initLoadBinFileItem();
    void initPrepareTimeoutItem();
    void initWaitingTimeItem();
    void initSendIntervalItem();
    void initTransferTimeoutItem();
    void initPostTransferWaitTimeItem();
    void loadConfigValues();
    void submitConfigTask(SettingItemWidget *item,
                          const std::function<void(SetFirmwareConfigTask *)> &configSetter,
                          const QString &paramName,
                          int value);

private:
    QLineEdit *m_binFileLineEdit = nullptr;
    QSpinBox *m_prepareTimeoutSpinBox = nullptr;
    QSpinBox *m_waitingTimeSpinBox = nullptr;
    QSpinBox *m_sendIntervalSpinBox = nullptr;
    QSpinBox *m_transferTimeoutSpinBox = nullptr;
    QSpinBox *m_postTransferWaitTimeSpinBox = nullptr;

    SettingItemWidget *m_prepareTimeoutItem = nullptr;
    SettingItemWidget *m_waitingTimeItem = nullptr;
    SettingItemWidget *m_sendIntervalItem = nullptr;
    SettingItemWidget *m_transferTimeoutItem = nullptr;
    SettingItemWidget *m_postTransferWaitTimeItem = nullptr;
};

#endif // FIRMWAREUPDATECONFIGSETTINGWIDGET_H
