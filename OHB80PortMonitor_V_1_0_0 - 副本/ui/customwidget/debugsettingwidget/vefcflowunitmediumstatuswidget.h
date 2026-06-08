#ifndef VEFCFLOWUNITMEDIUMSTATUSWIDGET_H
#define VEFCFLOWUNITMEDIUMSTATUSWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QPushButton>
#include <QString>
#include <QStringList>

class SettingItemWidget;

// ====================================================================
// VEFCFlowUnitMediumStatusWidget — VEFC 流量单位 / 介质配置状态读取（DebugPage）
//
//   1. Target Device QRCode SpinBox(int, 0~99999, 默认第一个设备)
//   2. Read 按钮 + Read All 按钮
//
//   Read     → 仅读取 SpinBox 中的设备 ID
//   Read All → 读取 SharedData::getAllQrcodes() 全部设备
//
//   读取结果：
//     - 全部成功         → 弹 Information
//     - 任一设备失败     → 弹 Warning，逐行列出
//                           "Device <qrcode>: Unit/Medium/Communication FAILED"
//
//   底层指令：ReadVEFCFlowUnitAndMediumStatus（FC 0x04, addr 0x0011）
// ====================================================================
class VEFCFlowUnitMediumStatusWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit VEFCFlowUnitMediumStatusWidget(QWidget *parent = nullptr);
    ~VEFCFlowUnitMediumStatusWidget();

private slots:
    void onReadBtnClicked();
    void onReadAllBtnClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initReadItem();

    void submitTask(const QStringList &qrcodes);

private:
    QSpinBox *m_qrcodeSpinBox = nullptr;

    SettingItemWidget *m_qrcodeItem = nullptr;
    SettingItemWidget *m_readItem   = nullptr;
};

#endif // VEFCFLOWUNITMEDIUMSTATUSWIDGET_H
