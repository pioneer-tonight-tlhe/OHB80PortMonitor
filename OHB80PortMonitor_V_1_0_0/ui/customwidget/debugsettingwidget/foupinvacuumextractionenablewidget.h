/**
 * @file foupinvacuumextractionenablewidget.h
 * @brief FOUP IN 真空阀保持模式调试控件。
 * @author Simon（工号：13）
 * @date 2026-06-12
 */

#ifndef FOUPINVACUUMEXTRACTIONENABLEWIDGET_H
#define FOUPINVACUUMEXTRACTIONENABLEWIDGET_H

#include "settingwidget.h"

#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QStringList>

class ModbusCommand;
class SettingItemWidget;

/**
 * @class FoupInVacuumExtractionEnableWidget
 * @brief 通过 SendCommandTask 读取和设置设备 FOUP IN Vacuum Extraction Enable。
 *
 * 对应 ModbusTcpMasterConfig.xml 中的：
 * - ReadFoupInVacuumExtractionEnable
 * - WriteFoupInVacuumExtractionEnable
 */
class FoupInVacuumExtractionEnableWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit FoupInVacuumExtractionEnableWidget(QWidget* parent = nullptr);
    ~FoupInVacuumExtractionEnableWidget() override;

private slots:
    void onReadBtnClicked();
    void onSetBtnClicked();
    void onReadAllBtnClicked();
    void onSetAllBtnClicked();

private:
    void initUI();
    void initQrcodeItem();
    void initControlItem();
    void initBatchItem();

    void submitReadTask(const QString& qrcode);
    void submitWriteTask(const QString& qrcode, quint16 enableValue);
    void submitReadAllTask(const QStringList& qrcodes);
    void submitWriteAllTask(const QStringList& qrcodes, quint16 enableValue);
    void setButtonsEnabled(bool enabled);

    static quint16 parseRegisterValue(const ModbusCommand& command);
    static QString valueDisplayText(quint16 value);

    /// 目标设备二维码输入框，默认取 SharedData 中第一台设备。
    QSpinBox* m_qrcodeSpinBox = nullptr;

    /// 保持模式选项：0=默认 10s 后关闭 SW5，1=FOUP IN 时保持开启 SW5。
    QComboBox* m_enableComboBox = nullptr;

    /// 读取当前设备保持模式的按钮。
    QPushButton* m_readBtn = nullptr;

    /// 写入当前下拉框保持模式的按钮。
    QPushButton* m_setBtn = nullptr;

    /// 一次性读取全部设备保持模式的按钮。
    QPushButton* m_readAllBtn = nullptr;

    /// 一次性向全部设备写入保持模式的按钮。
    QPushButton* m_setAllBtn = nullptr;

    /// 目标设备二维码配置项容器。
    SettingItemWidget* m_qrcodeItem = nullptr;

    /// 保持模式读写配置项容器。
    SettingItemWidget* m_controlItem = nullptr;

    /// 全部设备批量读写配置项容器。
    SettingItemWidget* m_batchItem = nullptr;
};

#endif // FOUPINVACUUMEXTRACTIONENABLEWIDGET_H
