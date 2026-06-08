#ifndef BOARDENABLESTATUSWIDGET_H
#define BOARDENABLESTATUSWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QPushButton>
#include <QString>
#include <QStringList>

class SettingItemWidget;

// ====================================================================
// BoardEnableStatusWidget — 读取板卡禁用状态（DebugPage）
//
//   1. Target Device QRCode SpinBox(int, 0~99999, 默认第一个设备)
//   2. Read 按钮 + Read All 按钮
//
//   Read     → 仅读取 SpinBox 中的设备 ID
//   Read All → 读取 SharedData::getAllQrcodes() 全部设备
//
//   读取结果：
//     - 弹窗显示每台设备的板卡状态（1=禁用，0=正常）
//
//   底层指令：ReadBoardEnable（FC 0x04, addr 0x00FF, 1 寄存器）
// ====================================================================
class BoardEnableStatusWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit BoardEnableStatusWidget(QWidget *parent = nullptr);
    ~BoardEnableStatusWidget();

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

#endif // BOARDENABLESTATUSWIDGET_H
