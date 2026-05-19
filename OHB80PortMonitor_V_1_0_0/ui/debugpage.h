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
class VEFCFlowUnitMediumStatusWidget;
class BoardEnableStatusWidget;

class DebugPage : public QWidget
{
    Q_OBJECT

public:
    explicit DebugPage(QWidget *parent = nullptr);
    ~DebugPage();

    void initUI();

    // 导航栏
    void initNav();

private slots:
    void navBtnClicked();

private:
    Ui::DebugPage *ui;
    FirmwareUpdateConfigSettingWidget *m_firmwareConfigWidget;
    FirmwareUpdateSettingWidget       *m_firmwareUpdateWidget;
    VEFCGasTypeSettingWidget          *m_vefcGasTypeWidget;
    UIRefreshTimeSettingWidget        *m_uiRefreshTimeWidget;
    VEFCFlowUnitMediumStatusWidget    *m_vefcFlowUnitMediumStatusWidget;
    BoardEnableStatusWidget            *m_boardEnableStatusWidget;
};

#endif // DEBUGPAGE_H
