#ifndef VEFCGASTYPESETTINGWIDGET_H
#define VEFCGASTYPESETTINGWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QString>
#include <QStringList>

class SettingItemWidget;

// ====================================================================
// VEFCGasTypeSettingWidget — VEFC 气体介质类型配置控件（DebugPage）
//
//   1. Target Device QRCode SpinBox(int, 0~99999, 默认第一个设备)
//   2. Gas Type ComboBox (CDA/N2/Ar/CO2/O2) + Set + Set All
//
//   Set     → 仅对 SpinBox 中的设备 ID 生效
//   Set All → 对 SharedData::getAllQrcodes() 全部设备生效
//
//   底层指令：WriteVEFCGasType（FC 0x06, addr 0x0001, 掉电保持）
// ====================================================================
class VEFCGasTypeSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit VEFCGasTypeSettingWidget(QWidget *parent = nullptr);
    ~VEFCGasTypeSettingWidget();

private slots:
    void onSetBtnClicked();
    void onSetAllBtnClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initGasTypeItem();

    void submitTask(const QStringList &qrcodes, int gasType);

private:
    QSpinBox  *m_qrcodeSpinBox  = nullptr;
    QComboBox *m_gasTypeCombo   = nullptr;

    SettingItemWidget *m_qrcodeItem  = nullptr;
    SettingItemWidget *m_gasTypeItem = nullptr;
};

#endif // VEFCGASTYPESETTINGWIDGET_H
