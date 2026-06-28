/**
 * @file foupinautopurgeenablewidget.h
 * @brief FOUPIN自动充气使能调试控件。
 * @author Simon（工号：13）
 * @date 2026-06-25
 */

#ifndef FOUPINAUTOPURGEENABLEWIDGET_H
#define FOUPINAUTOPURGEENABLEWIDGET_H

#include "settingwidget.h"

#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QStringList>

class SettingItemWidget;

/**
 * @class FoupInAutoPurgeEnableWidget
 * @brief 通过 SendCommandTask 下发 FOUPIN 自动充气使能写入指令。
 *
 * 对应 ModbusTcpMasterConfig.xml 中的 WriteFoupInAutoPurgeEnable：
 * - FC06
 * - addr 0x001C
 * - 0：FOUP到位后不执行充气及充气相关功能
 * - 1：FOUP到位后正常充气、开启30min湿度检测
 */
class FoupInAutoPurgeEnableWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit FoupInAutoPurgeEnableWidget(QWidget* parent = nullptr);
    ~FoupInAutoPurgeEnableWidget() override;

    void setInitialConfigValue(int enableValue);

private slots:
    void onSetSingleBtnClicked();
    void onSetAllBtnClicked();

private:
    void initUI();
    void initTargetItem();
    void initControlItem();

    void submitWriteTask(const QString& qrcode, quint16 enableValue);
    void submitWriteAllTask(const QStringList& qrcodes, quint16 enableValue);
    void setButtonsEnabled(bool enabled);

    quint16 currentEnableValue() const;
    static QString valueDisplayText(quint16 value);

    QSpinBox* m_qrcodeSpinBox = nullptr;        ///< 目标设备二维码输入框。
    QComboBox* m_enableComboBox = nullptr;      ///< 自动充气使能值选择框，0=关闭，1=开启。
    QPushButton* m_setSingleBtn = nullptr;      ///< 向当前二维码设备下发设置指令按钮。
    QPushButton* m_setAllBtn = nullptr;         ///< 向全部设备下发设置指令按钮。
    SettingItemWidget* m_targetItem = nullptr;  ///< 目标设备配置项容器。
    SettingItemWidget* m_controlItem = nullptr; ///< 自动充气使能配置项容器。
};

#endif // FOUPINAUTOPURGEENABLEWIDGET_H
