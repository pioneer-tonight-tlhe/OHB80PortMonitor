#ifndef PURGEFLOWSETTINGWIDGET_H
#define PURGEFLOWSETTINGWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QPushButton>
#include <QString>
#include <QStringList>

class SettingItemWidget;

// ====================================================================
// PurgeFlowSettingWidget — Purge 流量配置控件（ConfigPage）
//
//   1. Target Device QRCode SpinBox(int, 0~99999, 默认第一个设备)
//   2. Purge Flow SpinBox(int) + Set + Set All
//
//   Set     → 仅对 SpinBox 中的设备 ID 生效
//   Set All → 对 SharedData::getAllQrcodes() 全部设备生效
//
//   底层指令：WritePurgeFlow（FC 0x06, addr 0x0000）
//   寄存器值 = flow × 100
// ====================================================================
class PurgeFlowSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit PurgeFlowSettingWidget(QWidget *parent = nullptr);
    ~PurgeFlowSettingWidget();

    void setInitialConfigValue(int purgeFlowLitersPerMinute);

private slots:
    void onSetBtnClicked();
    void onSetAllBtnClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initFlowItem();

    void submitTask(const QStringList &qrcodes, int flowValue);

    // 设置任务运行期间所有 Set 按钮的启用状态
    void setAllSetButtonsEnabled(bool enabled);

private:
    QSpinBox *m_qrcodeSpinBox = nullptr;
    QSpinBox *m_flowSpinBox   = nullptr;

    // Set 按钮指针（任务期间禁用，避免并发任务提交）
    QPushButton *m_flowSetBtn;
    QPushButton *m_flowSetAllBtn;

    SettingItemWidget *m_qrcodeItem = nullptr;
    SettingItemWidget *m_flowItem   = nullptr;
};

#endif // PURGEFLOWSETTINGWIDGET_H
