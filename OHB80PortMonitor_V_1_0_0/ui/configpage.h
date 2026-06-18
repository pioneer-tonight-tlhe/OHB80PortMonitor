#ifndef CONFIGPAGE_H
#define CONFIGPAGE_H

#include <QWidget>

namespace Ui {
class ConfigPage;
}

class IdlePurgeSettingWidget;
class PneumaticValvePressureSettingWidget;
class SH85PeriodicSelfCheckSettingWidget;
class SH85SelfCheckSettingWidget;
class PurgeFlowSettingWidget;
class DeviceEnableSettingWidget;
class DeviceInfoSettingWidget;

class ConfigPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigPage(QWidget *parent = nullptr);
    ~ConfigPage();

    void initUI();
    void initNav();

private slots:
    void navBtnClicked();
    void onSelfCheckRunningStateChanged(bool running);
    void onPeriodicSelfCheckRunningStateChanged(bool running);

private:
    Ui::ConfigPage *ui;
    IdlePurgeSettingWidget *m_idlePurgeWidget;
    PneumaticValvePressureSettingWidget *m_pneumaticValvePressureWidget;
    SH85PeriodicSelfCheckSettingWidget *m_sh85PeriodicSelfCheckWidget;
    SH85SelfCheckSettingWidget *m_sh85SelfCheckWidget;
    DeviceEnableSettingWidget *m_deviceEnableWidget;
    PurgeFlowSettingWidget *m_purgeFlowWidget;
    DeviceInfoSettingWidget *m_deviceInfoWidget;
};

#endif // CONFIGPAGE_H
