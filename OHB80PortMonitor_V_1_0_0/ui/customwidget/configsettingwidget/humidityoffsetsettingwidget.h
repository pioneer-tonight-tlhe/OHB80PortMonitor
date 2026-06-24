#ifndef HUMIDITYOFFSETSETTINGWIDGET_H
#define HUMIDITYOFFSETSETTINGWIDGET_H

#include "settingwidget.h"

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QString>
#include <QStringList>

class SettingItemWidget;

// ====================================================================
// HumidityOffsetSettingWidget — 湿度校准（offset）配置控件
//
//   1. Target Device QRCode SpinBox（int，初始值来自 SharedData::getAllQrcodes() 第一个）
//   2. 湿度校准触发阈值（%）+ Set + Set All
//   3. 湿度 offset 参数（%）  + Set + Set All
//
//   按钮说明：
//     - Set     → 仅对 SpinBox 中的设备 ID 生效
//     - Set All → 对 SharedData::getAllQrcodes() 全部设备生效
//   两个参数独立工作：
//     - Threshold → SetHumidityOffsetTask::setThreshold(...) only
//     - Offset    → SetHumidityOffsetTask::setOffset(...) only
//
//   寄存器值 = 百分比 × 100（例：18% → 1800）
// ====================================================================
class HumidityOffsetSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit HumidityOffsetSettingWidget(QWidget *parent = nullptr);
    ~HumidityOffsetSettingWidget();

    void setInitialConfigValues(double humidityLowerLimitPercent,
                                double humidityOffsetPercent);

private slots:
    void onSetThresholdClicked();
    void onSetThresholdAllClicked();
    void onSetOffsetClicked();
    void onSetOffsetAllClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initThresholdItem();
    void initOffsetItem();

    // 提交任务（仅设置一项）
    void submitTask(const QStringList &qrcodes,
                    bool isThreshold,
                    double valuePct,
                    SettingItemWidget *targetItem);

    // 设置任务运行期间所有 Set 按钮的启用状态
    void setAllSetButtonsEnabled(bool enabled);

private:
    QSpinBox        *m_qrcodeSpinBox     = nullptr;
    QDoubleSpinBox  *m_thresholdSpinBox  = nullptr;
    QDoubleSpinBox  *m_offsetSpinBox     = nullptr;

    // Set 按钮指针（任务期间禁用，避免并发任务提交）
    QPushButton *m_thresholdSetBtn;
    QPushButton *m_thresholdSetAllBtn;
    QPushButton *m_offsetSetBtn;
    QPushButton *m_offsetSetAllBtn;

    SettingItemWidget *m_qrcodeItem    = nullptr;
    SettingItemWidget *m_thresholdItem = nullptr;
    SettingItemWidget *m_offsetItem    = nullptr;
};

#endif // HUMIDITYOFFSETSETTINGWIDGET_H
