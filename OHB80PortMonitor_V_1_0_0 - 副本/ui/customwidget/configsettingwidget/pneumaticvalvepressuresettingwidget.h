#ifndef PNEUMATICVALVEPRESSURESETTINGWIDGET_H
#define PNEUMATICVALVEPRESSURESETTINGWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QPushButton>
#include <QStringList>
#include <QComboBox>

class SettingItemWidget;

// ====================================================================
// PneumaticValvePressureSettingWidget — 气控阀压力配置控件
//   1. Target Device QRCode SpinBox（int, 0~99999, 默认第一个设备）
//   2. Pressure SpinBox（0~1000 bar）+ Set（设置当前 SpinBox 设备 ID）
//                                     + Set All（设置全部设备）
// ====================================================================
class PneumaticValvePressureSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit PneumaticValvePressureSettingWidget(QWidget *parent = nullptr);
    ~PneumaticValvePressureSettingWidget();

private slots:
    void onSetBtnClicked();      // 设置当前 SpinBox 设备 ID
    void onSetAllBtnClicked();   // 设置全部设备

private:
    void initUI();
    void initQrcodeItem();       // 设备选择项
    void initPressureItem();     // 压力设置项

    // 提交压力设置任务
    void submitPressureTask(const QStringList &qrcodes, double pressureBar);

    // 设置任务运行期间所有 Set 按钮的启用状态
    void setAllSetButtonsEnabled(bool enabled);

private:
    // 控件指针
    // QSpinBox  *m_qrcodeSpinBox;      // 设备 QRCode（int）
    QComboBox *m_comboBox;              // 主设备队列
    QSpinBox  *m_pressureSpinBox;    // 压力设置（bar）

    // Set 按钮指针（任务期间禁用，避免并发任务提交）
    QPushButton *m_pressureSetBtn;
    QPushButton *m_pressureSetAllBtn;

    // SettingItemWidget 指针（用于显示状态）
    SettingItemWidget *m_qrcodeItem;
    SettingItemWidget *m_pressureItem;
};

#endif // PNEUMATICVALVEPRESSURESETTINGWIDGET_H
