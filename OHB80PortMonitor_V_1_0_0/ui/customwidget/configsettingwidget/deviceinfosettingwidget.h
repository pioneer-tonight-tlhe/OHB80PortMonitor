/*******************************************************************************************
 * @file deviceinfosettingwidget.h
 * @author Simon <工号：13> 2026-06-24
 *
 * @class DeviceInfoSettingWidget
 * @brief 提供设备信息查看与设备网络参数修改入口的配置控件。
 *
 * 设计目标：
 *      1. 为 ConfigPage 提供统一的设备信息查看和修改界面。
 *      2. 将设备信息修改操作收口到调度任务，避免界面层直接变更运行态。
 *      3. 在查看设备信息时统一展示固件版本号、UI 屏幕版本号、IP 和 Port。
 *******************************************************************************************/
#ifndef DEVICEINFOSETTINGWIDGET_H
#define DEVICEINFOSETTINGWIDGET_H

#include "settingwidget.h"

#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QStringList>

class SettingItemWidget;

class DeviceInfoSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    // ============================ 构造函数 ============================
    explicit DeviceInfoSettingWidget(QWidget* parent = nullptr);
    ~DeviceInfoSettingWidget() override;

    // ============================ 权限注册访问 ============================
    QWidget* viewItem() const;
    QWidget* currentQrCodeItem() const;
    QWidget* qrCodeModifyItem() const;
    QWidget* endpointModifyItem() const;

private slots:
    // ---- 业务功能 ----
    void onViewClicked();
    void onSetQrCodeClicked();
    void onSetEndpointClicked();
    void onTargetQrCodeChanged(int value);

private:
    // ============================ 界面构建 ============================
    void initUI();
    void initViewItem();
    void initModifyItem();

    // ============================ 数据加载 ============================
    void refreshQrCodeRange(int preferredQrCode = -1);
    void loadDeviceInfo(int qrCode);

    // ============================ 输入校验 ============================
    bool validateCurrentQrCode(QString* errorMessage, QString* currentQrCode) const;
    bool validateNewQrCode(QString* errorMessage, const QString& currentQrCode, QString* newQrCode) const;
    bool validateEndpoint(QString* errorMessage, QString* ip, quint16* port) const;

    // ============================ 业务功能 ============================
    void submitDeviceInfoUpdate(const QString& oldQrCode,
                                const QString& newQrCode,
                                const QString& ip,
                                quint16 port,
                                SettingItemWidget* statusItem,
                                int successPreferredQrCode,
                                int failedPreferredQrCode);
    QStringList sortedQrCodes() const;
    bool qrCodeExists(const QString& qrCode) const;
    void setSetButtonEnabled(bool enabled);
    void showMessage(QMessageBox::Icon icon, const QString& title, const QString& text);

private:
    // ---- 状态成员 ----
    SettingItemWidget* m_viewItem = nullptr;
    SettingItemWidget* m_currentQrCodeItem = nullptr;
    SettingItemWidget* m_qrCodeModifyItem = nullptr;
    SettingItemWidget* m_endpointModifyItem = nullptr;

    // ---- 功能模块成员 ----
    QPushButton* m_viewButton = nullptr;
    QPushButton* m_setQrCodeButton = nullptr;
    QPushButton* m_setEndpointButton = nullptr;
    QSpinBox* m_targetQrCodeSpinBox = nullptr;
    QSpinBox* m_newQrCodeSpinBox = nullptr;
    QLineEdit* m_ipLineEdit = nullptr;
    QLineEdit* m_portLineEdit = nullptr;
};

#endif // DEVICEINFOSETTINGWIDGET_H
