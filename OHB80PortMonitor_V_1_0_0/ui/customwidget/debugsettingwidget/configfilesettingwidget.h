#ifndef CONFIG_FILE_SETTING_WIDGET_H
#define CONFIG_FILE_SETTING_WIDGET_H

#include "../settingwidget/settingwidget.h"
#include "ohbdeviceconfiginfo.h"

#include <QMap>
#include <QString>
#include <QVector>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTabWidget;
class SettingItemWidget;

class ConfigFileSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit ConfigFileSettingWidget(QWidget *parent = nullptr);
    ~ConfigFileSettingWidget() override = default;

private:
    enum class ConfigPageKind {
        AppIni,
        AlarmIni,
        LoggerConfigIni,
        FirmwareIni,
        ModulePermissionIni,
        OhbDeviceIni
    };

    struct GenericPage {
        QString fileName;
        QWidget *page = nullptr;
        QTableWidget *table = nullptr;
        SettingItemWidget *navItem = nullptr;
    };

    void initUI();
    void initNavItem(ConfigPageKind kind, const QString &title, const QString &fileName);
    QWidget *createGenericPage(ConfigPageKind kind, const QString &fileName);
    QWidget *createOhbDevicePage();
    QWidget *createOhbGlobalTab();
    QWidget *createOhbDeviceTab();

    void showPage(ConfigPageKind kind);
    void loadGenericPage(ConfigPageKind kind);
    void submitGenericPage(ConfigPageKind kind);
    void addGenericRow(ConfigPageKind kind);

    void loadOhbGlobalValues();
    void submitOhbGlobalValues();

    void reloadDeviceCache();
    QStringList availableQrcodes() const;
    QStringList selectedDeviceQrcodes() const;
    void refreshSelectedQrcodeList();
    void showDeviceConfig(const QString &qrcode);
    OHBDeviceConfigInfo collectDeviceConfig() const;
    void submitOhbDeviceValues();

    QSpinBox *createIntSpinBox(int minValue, int maxValue, QWidget *parent) const;
    QDoubleSpinBox *createDoubleSpinBox(double minValue, double maxValue, QWidget *parent) const;
    int pageIndex(ConfigPageKind kind) const;

private:
    QStackedWidget *m_stack;
    QMap<ConfigPageKind, GenericPage> m_genericPages;
    QMap<ConfigPageKind, int> m_pageIndexes;

    SettingItemWidget *m_ohbNavItem;

    QCheckBox *m_sh85EnabledCheck;
    QSpinBox *m_sh85PeriodSpin;
    QLineEdit *m_masterDevicesEdit;

    QCheckBox *m_singleCheck;
    QSpinBox *m_singleQrcodeSpin;
    QCheckBox *m_rangeCheck;
    QSpinBox *m_rangeStartSpin;
    QSpinBox *m_rangeEndSpin;
    QComboBox *m_selectedQrcodeCombo;

    QLineEdit *m_qrcodeEdit;
    QLineEdit *m_ipEdit;
    QSpinBox *m_portSpin;
    QCheckBox *m_deviceEnabledCheck;
    QSpinBox *m_purgeFlowSpin;
    QDoubleSpinBox *m_vppePressureSpin;
    QSpinBox *m_logoTimeSpin;
    QSpinBox *m_pageSwitchIntervalSpin;
    QSpinBox *m_pageTotalTimeSpin;
    QDoubleSpinBox *m_humidityOffsetSpin;
    QDoubleSpinBox *m_humidityLowerLimitSpin;
    QComboBox *m_foupInAutoPurgeCombo;
    QCheckBox *m_idlePurgeEnabledCheck;
    QSpinBox *m_idlePurgeDurationSpin;
    QSpinBox *m_idlePurgeIntervalSpin;

    QString m_currentDeviceQrCode;
    QVector<OHBDeviceConfigInfo> m_devices;
};

#endif // CONFIG_FILE_SETTING_WIDGET_H
