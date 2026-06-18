#ifndef DEBUGPAGE_H
#define DEBUGPAGE_H

#include <QWidget>

namespace Ui {
class DebugPage;
}

class FirmwareUpdateConfigSettingWidget;
class FirmwareUpdateSettingWidget;
class VEFCGasTypeSettingWidget;
class UIRefreshTimeSettingWidget;
class HumidityOffsetSettingWidget;
class VEFCFlowUnitMediumStatusWidget;
class BoardEnableStatusWidget;
class FoupInVacuumExtractionEnableWidget;

class DebugPage : public QWidget
{
    Q_OBJECT

public:
    explicit DebugPage(QWidget *parent = nullptr);
    ~DebugPage();

    void initUI();
    void initNav();

private slots:
    void navBtnClicked();

private:
    Ui::DebugPage *ui;
    FirmwareUpdateConfigSettingWidget *m_firmwareConfigWidget;
    FirmwareUpdateSettingWidget *m_firmwareUpdateWidget;
    VEFCGasTypeSettingWidget *m_vefcGasTypeWidget;
    UIRefreshTimeSettingWidget *m_uiRefreshTimeWidget;
    HumidityOffsetSettingWidget *m_humidityOffsetWidget;
    VEFCFlowUnitMediumStatusWidget *m_vefcFlowUnitMediumStatusWidget;
    BoardEnableStatusWidget *m_boardEnableStatusWidget;
    FoupInVacuumExtractionEnableWidget *m_foupInVacuumExtractionEnableWidget;
};

#endif // DEBUGPAGE_H
