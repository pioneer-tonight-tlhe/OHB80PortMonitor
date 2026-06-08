#ifndef UIREFRESHTIMESETTINGWIDGET_H
#define UIREFRESHTIMESETTINGWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QPushButton>
#include <QString>
#include <QStringList>

class SettingItemWidget;

// ====================================================================
// UIRefreshTimeSettingWidget — UI 页面刷新时间配置控件（DebugPage）
//
//   1. Target Device QRCode SpinBox(int, 0~99999, 默认第一个设备)
//   2. Logo Duration SpinBox
//   3. Param Total Duration SpinBox
//   4. Param Switch Interval SpinBox + Set + Set All
//
//   Set     → 仅作用于 SpinBox 中的设备 ID
//   Set All → 作用于 SharedData::getAllQrcodes() 全部设备
//
//   底层指令：WriteUIRefreshTime（FC 0x10, addr 0x0004, 6 字节）
//     [0..1] logo 界面时长   [2..3] 参数界面总时长   [4..5] 参数界面切换时间
// ====================================================================
class UIRefreshTimeSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit UIRefreshTimeSettingWidget(QWidget *parent = nullptr);
    ~UIRefreshTimeSettingWidget();

private slots:
    void onSetBtnClicked();
    void onSetAllBtnClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initLogoItem();
    void initParamTotalItem();
    void initParamSwitchItem();

    void submitTask(const QStringList &qrcodes, int logoSec, int paramTotalSec, int paramSwitchSec);

private:
    QSpinBox *m_qrcodeSpinBox      = nullptr;
    QSpinBox *m_logoSecSpinBox     = nullptr;
    QSpinBox *m_paramTotalSpinBox  = nullptr;
    QSpinBox *m_paramSwitchSpinBox = nullptr;

    SettingItemWidget *m_qrcodeItem      = nullptr;
    SettingItemWidget *m_logoItem        = nullptr;
    SettingItemWidget *m_paramTotalItem  = nullptr;
    SettingItemWidget *m_paramSwitchItem = nullptr;
};

#endif // UIREFRESHTIMESETTINGWIDGET_H
