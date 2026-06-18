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
    explicit DeviceInfoSettingWidget(QWidget* parent = nullptr);
    ~DeviceInfoSettingWidget();

private slots:
    void onViewClicked();
    void onSetQrCodeClicked();
    void onSetEndpointClicked();
    void onTargetQrCodeChanged(int value);

private:
    void initUI();
    void initViewItem();
    void initModifyItem();
    void refreshQrCodeRange(int preferredQrCode = -1);
    void loadDeviceInfo(int qrCode);
    bool validateCurrentQrCode(QString* errorMessage, QString* currentQrCode) const;
    bool validateNewQrCode(QString* errorMessage, const QString& currentQrCode, QString* newQrCode) const;
    bool validateEndpoint(QString* errorMessage, QString* ip, quint16* port) const;
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
    SettingItemWidget* m_viewItem = nullptr;
    SettingItemWidget* m_currentQrCodeItem = nullptr;
    SettingItemWidget* m_qrCodeModifyItem = nullptr;
    SettingItemWidget* m_endpointModifyItem = nullptr;

    QPushButton* m_viewButton = nullptr;
    QPushButton* m_setQrCodeButton = nullptr;
    QPushButton* m_setEndpointButton = nullptr;
    QSpinBox* m_targetQrCodeSpinBox = nullptr;
    QSpinBox* m_newQrCodeSpinBox = nullptr;
    QLineEdit* m_ipLineEdit = nullptr;
    QLineEdit* m_portLineEdit = nullptr;
};

#endif // DEVICEINFOSETTINGWIDGET_H
