#include "configfilesettingwidget.h"

#include "../settingwidget/settingitemwidget.h"
#include "app/appconfig.h"
#include "app/shareddata.h"
#include "idlepurgeconfig.h"
#include "ohbdeviceconfig.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/config_file_task/set_config_file_task.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int MaxUInt16ConfigValue = 65534;

QString filePath(const QString &fileName)
{
    return QDir(AppConfig::getInstance().getConfigDir()).filePath(fileName);
}

QStringList sortedNumericQrcodes(QStringList qrcodes)
{
    qrcodes.removeDuplicates();
    std::sort(qrcodes.begin(), qrcodes.end(), [](const QString &lhs, const QString &rhs) {
        bool leftOk = false;
        bool rightOk = false;
        const int left = lhs.toInt(&leftOk);
        const int right = rhs.toInt(&rightOk);
        if (leftOk && rightOk) {
            return left < right;
        }
        return lhs < rhs;
    });
    return qrcodes;
}

void addFormRow(QFormLayout *layout, const QString &label, QWidget *field)
{
    auto *caption = new QLabel(label, field ? field->parentWidget() : nullptr);
    caption->setMinimumWidth(190);
    layout->addRow(caption, field);
}
}

ConfigFileSettingWidget::ConfigFileSettingWidget(QWidget *parent)
    : SettingWidget(parent)
    , m_stack(nullptr)
    , m_ohbNavItem(nullptr)
    , m_idleEnabledCheck(nullptr)
    , m_purgeDurationSpin(nullptr)
    , m_purgeIntervalSpin(nullptr)
    , m_sh85EnabledCheck(nullptr)
    , m_sh85PeriodSpin(nullptr)
    , m_masterDevicesEdit(nullptr)
    , m_singleCheck(nullptr)
    , m_singleQrcodeSpin(nullptr)
    , m_rangeCheck(nullptr)
    , m_rangeStartSpin(nullptr)
    , m_rangeEndSpin(nullptr)
    , m_selectedQrcodeCombo(nullptr)
    , m_qrcodeEdit(nullptr)
    , m_ipEdit(nullptr)
    , m_portSpin(nullptr)
    , m_deviceEnabledCheck(nullptr)
    , m_purgeFlowSpin(nullptr)
    , m_vppePressureSpin(nullptr)
    , m_logoTimeSpin(nullptr)
    , m_pageSwitchIntervalSpin(nullptr)
    , m_pageTotalTimeSpin(nullptr)
    , m_humidityOffsetSpin(nullptr)
    , m_humidityLowerLimitSpin(nullptr)
    , m_foupInAutoPurgeCombo(nullptr)
{
    setTitle(QStringLiteral("Config Files"));
    initUI();
}

void ConfigFileSettingWidget::initUI()
{
    initNavItem(ConfigPageKind::AppIni, QStringLiteral("app.ini"), QStringLiteral("app.ini"));
    initNavItem(ConfigPageKind::AlarmIni, QStringLiteral("alarm.ini"), QStringLiteral("alarm.ini"));
    initNavItem(ConfigPageKind::LoggerConfigIni, QStringLiteral("logger_config.ini"), QStringLiteral("logger_config.ini"));
    initNavItem(ConfigPageKind::FirmwareIni, QStringLiteral("firmware.ini"), QStringLiteral("firmware.ini"));
    initNavItem(ConfigPageKind::ModulePermissionIni, QStringLiteral("module_permission.ini"), QStringLiteral("module_permission.ini"));
    initNavItem(ConfigPageKind::OhbDeviceIni, QStringLiteral("ohb_device.ini"), QStringLiteral("ohb_device.ini"));

    m_stack = new QStackedWidget(this);
    m_stack->setMinimumHeight(520);

    for (auto it = m_genericPages.begin(); it != m_genericPages.end(); ++it) {
        QWidget *page = createGenericPage(it.key(), it.value().fileName);
        it->page = page;
        m_pageIndexes.insert(it.key(), m_stack->addWidget(page));
    }

    m_pageIndexes.insert(ConfigPageKind::OhbDeviceIni, m_stack->addWidget(createOhbDevicePage()));
    addCustomWidget(m_stack);

    showPage(ConfigPageKind::AppIni);
}

void ConfigFileSettingWidget::initNavItem(ConfigPageKind kind,
                                          const QString &title,
                                          const QString &fileName)
{
    auto *item = new SettingItemWidget(this);
    item->setTitle(title);
    item->setTip(QStringLiteral("Open config editor"));

    auto *button = new QPushButton(QStringLiteral("Open"), item);
    item->addWidget(QStringLiteral("open_btn"), button);
    connect(button, &QPushButton::clicked, this, [this, kind]() {
        showPage(kind);
    });

    addItem(item);

    if (kind == ConfigPageKind::OhbDeviceIni) {
        m_ohbNavItem = item;
    } else {
        GenericPage page;
        page.fileName = fileName;
        page.navItem = item;
        m_genericPages.insert(kind, page);
    }
}

QWidget *ConfigFileSettingWidget::createGenericPage(ConfigPageKind kind, const QString &fileName)
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("Editing ") + fileName, page);
    title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 15px;"));
    layout->addWidget(title);

    auto *table = new QTableWidget(page);
    table->setColumnCount(3);
    table->setHorizontalHeaderLabels(QStringList{QStringLiteral("Group"), QStringLiteral("Key"), QStringLiteral("Value")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    layout->addWidget(table);

    auto *buttonRow = new QWidget(page);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch();

    auto *reloadButton = new QPushButton(QStringLiteral("Reload"), buttonRow);
    auto *addRowButton = new QPushButton(QStringLiteral("Add Row"), buttonRow);
    auto *saveButton = new QPushButton(QStringLiteral("Set"), buttonRow);
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addWidget(addRowButton);
    buttonLayout->addWidget(saveButton);
    layout->addWidget(buttonRow);

    m_genericPages[kind].table = table;
    connect(reloadButton, &QPushButton::clicked, this, [this, kind]() { loadGenericPage(kind); });
    connect(addRowButton, &QPushButton::clicked, this, [this, kind]() { addGenericRow(kind); });
    connect(saveButton, &QPushButton::clicked, this, [this, kind]() { submitGenericPage(kind); });

    loadGenericPage(kind);
    return page;
}

QWidget *ConfigFileSettingWidget::createOhbDevicePage()
{
    auto *tabs = new QTabWidget(this);
    tabs->addTab(createOhbGlobalTab(), QStringLiteral("Global"));
    tabs->addTab(createOhbDeviceTab(), QStringLiteral("OHB Device"));
    return tabs;
}

QWidget *ConfigFileSettingWidget::createOhbGlobalTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignTop);

    m_idleEnabledCheck = new QCheckBox(QStringLiteral("Enabled"), page);
    m_purgeDurationSpin = createIntSpinBox(0, MaxUInt16ConfigValue, page);
    m_purgeIntervalSpin = createIntSpinBox(0, MaxUInt16ConfigValue, page);
    m_sh85EnabledCheck = new QCheckBox(QStringLiteral("Enabled"), page);
    m_sh85PeriodSpin = createIntSpinBox(1, 86400, page);
    m_masterDevicesEdit = new QLineEdit(page);
    m_masterDevicesEdit->setPlaceholderText(QStringLiteral("12001,12002,12003"));

    addFormRow(form, QStringLiteral("[IdleConfig] Enabled"), m_idleEnabledCheck);
    addFormRow(form, QStringLiteral("[IdleConfig] PurgeDuration_s"), m_purgeDurationSpin);
    addFormRow(form, QStringLiteral("[IdleConfig] PurgeInterval_s"), m_purgeIntervalSpin);
    addFormRow(form, QStringLiteral("[SH85SelfCheckTask] Enabled"), m_sh85EnabledCheck);
    addFormRow(form, QStringLiteral("[SH85SelfCheckTask] Period_s"), m_sh85PeriodSpin);
    addFormRow(form, QStringLiteral("[MasterDevices] List"), m_masterDevicesEdit);
    layout->addLayout(form);

    auto *buttonRow = new QWidget(page);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch();

    auto *reloadButton = new QPushButton(QStringLiteral("Reload"), buttonRow);
    auto *saveButton = new QPushButton(QStringLiteral("Set Global"), buttonRow);
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addWidget(saveButton);
    layout->addWidget(buttonRow);
    layout->addStretch();

    connect(reloadButton, &QPushButton::clicked, this, &ConfigFileSettingWidget::loadOhbGlobalValues);
    connect(saveButton, &QPushButton::clicked, this, &ConfigFileSettingWidget::submitOhbGlobalValues);
    loadOhbGlobalValues();
    return page;
}

QWidget *ConfigFileSettingWidget::createOhbDeviceTab()
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(10);

    auto *controlRow = new QWidget(page);
    auto *controlLayout = new QGridLayout(controlRow);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setHorizontalSpacing(8);

    m_singleCheck = new QCheckBox(QStringLiteral("Single QRCode"), controlRow);
    m_singleQrcodeSpin = createIntSpinBox(0, 99999, controlRow);
    m_rangeCheck = new QCheckBox(QStringLiteral("QRCode Range"), controlRow);
    m_rangeStartSpin = createIntSpinBox(0, 99999, controlRow);
    m_rangeEndSpin = createIntSpinBox(0, 99999, controlRow);
    auto *applySelectionButton = new QPushButton(QStringLiteral("Apply Selection"), controlRow);

    m_singleCheck->setChecked(true);
    controlLayout->addWidget(m_singleCheck, 0, 0);
    controlLayout->addWidget(m_singleQrcodeSpin, 0, 1);
    controlLayout->addWidget(m_rangeCheck, 0, 2);
    controlLayout->addWidget(m_rangeStartSpin, 0, 3);
    controlLayout->addWidget(new QLabel(QStringLiteral("~"), controlRow), 0, 4);
    controlLayout->addWidget(m_rangeEndSpin, 0, 5);
    controlLayout->addWidget(applySelectionButton, 0, 6);
    controlLayout->setColumnStretch(7, 1);
    layout->addWidget(controlRow);

    m_selectedQrcodeCombo = new QComboBox(page);
    m_selectedQrcodeCombo->setVisible(false);
    layout->addWidget(m_selectedQrcodeCombo);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignTop);

    m_qrcodeEdit = new QLineEdit(page);
    m_ipEdit = new QLineEdit(page);
    m_portSpin = createIntSpinBox(0, 65535, page);
    m_deviceEnabledCheck = new QCheckBox(QStringLiteral("Enabled"), page);
    m_purgeFlowSpin = createIntSpinBox(0, 65535, page);
    m_vppePressureSpin = createDoubleSpinBox(0.0, 10.0, page);
    m_logoTimeSpin = createIntSpinBox(0, MaxUInt16ConfigValue, page);
    m_pageSwitchIntervalSpin = createIntSpinBox(0, MaxUInt16ConfigValue, page);
    m_pageTotalTimeSpin = createIntSpinBox(0, MaxUInt16ConfigValue, page);
    m_humidityOffsetSpin = createDoubleSpinBox(-100.0, 100.0, page);
    m_humidityLowerLimitSpin = createDoubleSpinBox(0.0, 100.0, page);
    m_foupInAutoPurgeCombo = new QComboBox(page);
    m_foupInAutoPurgeCombo->addItem(QStringLiteral("0"), 0);
    m_foupInAutoPurgeCombo->addItem(QStringLiteral("1"), 1);

    addFormRow(form, QStringLiteral("QRCode"), m_qrcodeEdit);
    addFormRow(form, QStringLiteral("Ip"), m_ipEdit);
    addFormRow(form, QStringLiteral("Port"), m_portSpin);
    addFormRow(form, QStringLiteral("Enable"), m_deviceEnabledCheck);
    addFormRow(form, QStringLiteral("PurgeFlow_l_min"), m_purgeFlowSpin);
    addFormRow(form, QStringLiteral("VPPEPressure_bar"), m_vppePressureSpin);
    addFormRow(form, QStringLiteral("LogoTime_s"), m_logoTimeSpin);
    addFormRow(form, QStringLiteral("PageSwitchInterval_s"), m_pageSwitchIntervalSpin);
    addFormRow(form, QStringLiteral("PageTotalTime_s"), m_pageTotalTimeSpin);
    addFormRow(form, QStringLiteral("HumidityOffset_pct"), m_humidityOffsetSpin);
    addFormRow(form, QStringLiteral("HumidityLowerLimit_pct"), m_humidityLowerLimitSpin);
    addFormRow(form, QStringLiteral("FoupInAutoPurgeEnable"), m_foupInAutoPurgeCombo);
    layout->addLayout(form);

    auto *buttonRow = new QWidget(page);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch();

    auto *reloadButton = new QPushButton(QStringLiteral("Reload"), buttonRow);
    auto *saveButton = new QPushButton(QStringLiteral("Set Device"), buttonRow);
    buttonLayout->addWidget(reloadButton);
    buttonLayout->addWidget(saveButton);
    layout->addWidget(buttonRow);
    layout->addStretch();

    connect(m_singleCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) m_rangeCheck->setChecked(false);
    });
    connect(m_rangeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) m_singleCheck->setChecked(false);
    });
    connect(applySelectionButton, &QPushButton::clicked,
            this, &ConfigFileSettingWidget::refreshSelectedQrcodeList);
    connect(m_selectedQrcodeCombo, &QComboBox::currentTextChanged,
            this, &ConfigFileSettingWidget::showDeviceConfig);
    connect(reloadButton, &QPushButton::clicked, this, [this]() {
        reloadDeviceCache();
        refreshSelectedQrcodeList();
    });
    connect(saveButton, &QPushButton::clicked,
            this, &ConfigFileSettingWidget::submitOhbDeviceValues);

    reloadDeviceCache();
    const QStringList qrcodes = availableQrcodes();
    if (!qrcodes.isEmpty()) {
        bool ok = false;
        const int first = qrcodes.first().toInt(&ok);
        if (ok) {
            m_singleQrcodeSpin->setValue(first);
            m_rangeStartSpin->setValue(first);
            m_rangeEndSpin->setValue(first);
        }
    }
    refreshSelectedQrcodeList();
    return page;
}

void ConfigFileSettingWidget::showPage(ConfigPageKind kind)
{
    if (!m_stack) {
        return;
    }

    const int index = pageIndex(kind);
    if (index >= 0) {
        m_stack->setCurrentIndex(index);
        if (m_genericPages.contains(kind)) {
            loadGenericPage(kind);
        } else if (kind == ConfigPageKind::OhbDeviceIni) {
            loadOhbGlobalValues();
            reloadDeviceCache();
            refreshSelectedQrcodeList();
        }
    }
}

void ConfigFileSettingWidget::loadGenericPage(ConfigPageKind kind)
{
    if (!m_genericPages.contains(kind) || !m_genericPages[kind].table) {
        return;
    }

    GenericPage &page = m_genericPages[kind];
    QSettings settings(filePath(page.fileName), QSettings::IniFormat);
    const QStringList keys = settings.allKeys();

    page.table->setRowCount(keys.size());
    for (int row = 0; row < keys.size(); ++row) {
        const QString fullKey = keys.at(row);
        const int separator = fullKey.lastIndexOf('/');
        const QString group = separator > 0 ? fullKey.left(separator) : QString();
        const QString key = separator > 0 ? fullKey.mid(separator + 1) : fullKey;
        const QString value = settings.value(fullKey).toString();

        page.table->setItem(row, 0, new QTableWidgetItem(group));
        page.table->setItem(row, 1, new QTableWidgetItem(key));
        page.table->setItem(row, 2, new QTableWidgetItem(value));
    }
}

void ConfigFileSettingWidget::submitGenericPage(ConfigPageKind kind)
{
    if (!m_genericPages.contains(kind) || !m_genericPages[kind].table) {
        return;
    }

    GenericPage &page = m_genericPages[kind];
    QVector<SetConfigFileTask::IniEntry> entries;
    for (int row = 0; row < page.table->rowCount(); ++row) {
        SetConfigFileTask::IniEntry entry;
        entry.group = page.table->item(row, 0) ? page.table->item(row, 0)->text().trimmed() : QString();
        entry.key = page.table->item(row, 1) ? page.table->item(row, 1)->text().trimmed() : QString();
        entry.value = page.table->item(row, 2) ? page.table->item(row, 2)->text() : QString();
        if (!entry.key.isEmpty()) {
            entries.append(entry);
        }
    }

    auto *task = new SetConfigFileTask();
    task->setGenericIni(page.fileName, entries);
    if (page.navItem) page.navItem->setStatusWaiting();

    connect(task, &SetConfigFileTask::finished, this, [this, kind](bool success, const QString &msg) {
        GenericPage &page = m_genericPages[kind];
        if (page.navItem) {
            success ? page.navItem->setStatusOK() : page.navItem->setStatusFailed(msg);
        }
        if (success) {
            loadGenericPage(kind);
        }
    });

    Scheduler::instance()->submitTask(task);
}

void ConfigFileSettingWidget::addGenericRow(ConfigPageKind kind)
{
    if (!m_genericPages.contains(kind) || !m_genericPages[kind].table) {
        return;
    }

    QTableWidget *table = m_genericPages[kind].table;
    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, new QTableWidgetItem(QString()));
    table->setItem(row, 1, new QTableWidgetItem(QString()));
    table->setItem(row, 2, new QTableWidgetItem(QString()));
}

void ConfigFileSettingWidget::loadOhbGlobalValues()
{
    if (!m_idleEnabledCheck) {
        return;
    }

    IdlePurgeConfig &idleConfig = IdlePurgeConfig::getInstance();
    OHBDeviceConfig &ohbConfig = OHBDeviceConfig::getInstance();

    m_idleEnabledCheck->setChecked(idleConfig.isEnabled());
    m_purgeDurationSpin->setValue(idleConfig.getPurgeDurationSeconds());
    m_purgeIntervalSpin->setValue(idleConfig.getPurgeIntervalSeconds());
    m_sh85EnabledCheck->setChecked(ohbConfig.readSH85SelfCheckEnabled());
    m_sh85PeriodSpin->setValue(ohbConfig.readSH85SelfCheckPeriodSeconds());

    const QVector<QString> masterDevices = ohbConfig.readMasterDevices();
    QStringList values;
    for (const QString &device : masterDevices) {
        values << device;
    }
    m_masterDevicesEdit->setText(values.join(','));
}

void ConfigFileSettingWidget::submitOhbGlobalValues()
{
    QStringList masterItems = m_masterDevicesEdit->text().split(',', Qt::SkipEmptyParts);
    QVector<QString> masters;
    for (QString item : masterItems) {
        item = item.trimmed();
        if (!item.isEmpty()) {
            masters.append(item);
        }
    }

    auto *task = new SetConfigFileTask();
    task->setOhbGlobal(m_idleEnabledCheck->isChecked(),
                       m_purgeDurationSpin->value(),
                       m_purgeIntervalSpin->value(),
                       m_sh85EnabledCheck->isChecked(),
                       m_sh85PeriodSpin->value(),
                       masters);

    if (m_ohbNavItem) m_ohbNavItem->setStatusWaiting();
    connect(task, &SetConfigFileTask::finished, this, [this](bool success, const QString &msg) {
        if (m_ohbNavItem) {
            success ? m_ohbNavItem->setStatusOK() : m_ohbNavItem->setStatusFailed(msg);
        }
        if (success) {
            loadOhbGlobalValues();
        }
    });
    Scheduler::instance()->submitTask(task);
}

void ConfigFileSettingWidget::reloadDeviceCache()
{
    m_devices = OHBDeviceConfig::getInstance().readDevices();
}

QStringList ConfigFileSettingWidget::availableQrcodes() const
{
    QStringList qrcodes = SharedData::getAllQrcodes();
    if (qrcodes.isEmpty()) {
        for (const OHBDeviceConfigInfo &device : m_devices) {
            if (!device.getQrCode().isEmpty()) {
                qrcodes << device.getQrCode();
            }
        }
    }
    return sortedNumericQrcodes(qrcodes);
}

QStringList ConfigFileSettingWidget::selectedDeviceQrcodes() const
{
    if (m_singleCheck && m_singleCheck->isChecked()) {
        return QStringList{QString::number(m_singleQrcodeSpin->value())};
    }

    QStringList selected;
    const int start = qMin(m_rangeStartSpin->value(), m_rangeEndSpin->value());
    const int end = qMax(m_rangeStartSpin->value(), m_rangeEndSpin->value());
    for (const QString &qrcode : availableQrcodes()) {
        bool ok = false;
        const int value = qrcode.toInt(&ok);
        if (ok && value >= start && value <= end) {
            selected << qrcode;
        }
    }
    return sortedNumericQrcodes(selected);
}

void ConfigFileSettingWidget::refreshSelectedQrcodeList()
{
    if (!m_selectedQrcodeCombo) {
        return;
    }

    const QStringList qrcodes = selectedDeviceQrcodes();
    m_selectedQrcodeCombo->blockSignals(true);
    m_selectedQrcodeCombo->clear();
    m_selectedQrcodeCombo->addItems(qrcodes);
    m_selectedQrcodeCombo->setVisible(qrcodes.size() > 1);
    m_selectedQrcodeCombo->blockSignals(false);

    if (qrcodes.isEmpty()) {
        m_currentDeviceQrCode.clear();
        QMessageBox::warning(this, QStringLiteral("No Device"), QStringLiteral("No matched QRCode found."));
        return;
    }

    showDeviceConfig(qrcodes.first());
}

void ConfigFileSettingWidget::showDeviceConfig(const QString &qrcode)
{
    if (qrcode.isEmpty()) {
        return;
    }

    for (const OHBDeviceConfigInfo &device : qAsConst(m_devices)) {
        if (device.getQrCode() == qrcode) {
            m_currentDeviceQrCode = qrcode;
            m_qrcodeEdit->setText(device.getQrCode());
            m_ipEdit->setText(device.getIp());
            m_portSpin->setValue(device.getPort());
            m_deviceEnabledCheck->setChecked(device.isEnabled());
            m_purgeFlowSpin->setValue(device.getPurgeFlowLitersPerMinute());
            m_vppePressureSpin->setValue(device.getVppePressureBar());
            m_logoTimeSpin->setValue(device.getLogoTimeSeconds());
            m_pageSwitchIntervalSpin->setValue(device.getPageSwitchIntervalSeconds());
            m_pageTotalTimeSpin->setValue(device.getPageTotalTimeSeconds());
            m_humidityOffsetSpin->setValue(device.getHumidityOffsetPercent());
            m_humidityLowerLimitSpin->setValue(device.getHumidityLowerLimitPercent());
            m_foupInAutoPurgeCombo->setCurrentIndex(device.getFoupInAutoPurgeEnable() == 1 ? 1 : 0);
            return;
        }
    }

    QMessageBox::warning(this, QStringLiteral("Device Missing"), QStringLiteral("QRCode not found in ohb_device.ini."));
}

OHBDeviceConfigInfo ConfigFileSettingWidget::collectDeviceConfig() const
{
    return OHBDeviceConfigInfo(m_qrcodeEdit->text().trimmed(),
                               m_ipEdit->text().trimmed(),
                               static_cast<quint16>(m_portSpin->value()),
                               m_deviceEnabledCheck->isChecked(),
                               m_purgeFlowSpin->value(),
                               m_logoTimeSpin->value(),
                               m_pageSwitchIntervalSpin->value(),
                               m_pageTotalTimeSpin->value(),
                               m_humidityOffsetSpin->value(),
                               m_humidityLowerLimitSpin->value(),
                               m_vppePressureSpin->value(),
                               m_foupInAutoPurgeCombo->currentData().toInt());
}

void ConfigFileSettingWidget::submitOhbDeviceValues()
{
    if (m_currentDeviceQrCode.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Set Failed"), QStringLiteral("No current QRCode selected."));
        return;
    }

    const OHBDeviceConfigInfo deviceInfo = collectDeviceConfig();
    if (deviceInfo.getQrCode().isEmpty() || deviceInfo.getIp().isEmpty() || deviceInfo.getPort() == 0) {
        QMessageBox::warning(this, QStringLiteral("Set Failed"), QStringLiteral("QRCode, Ip and Port are required."));
        return;
    }

    auto *task = new SetConfigFileTask();
    task->setOhbDevice(m_currentDeviceQrCode, deviceInfo);

    if (m_ohbNavItem) m_ohbNavItem->setStatusWaiting();
    connect(task, &SetConfigFileTask::finished, this, [this, deviceInfo](bool success, const QString &msg) {
        if (m_ohbNavItem) {
            success ? m_ohbNavItem->setStatusOK() : m_ohbNavItem->setStatusFailed(msg);
        }
        if (success) {
            reloadDeviceCache();
            m_currentDeviceQrCode = deviceInfo.getQrCode();
            refreshSelectedQrcodeList();
            showDeviceConfig(deviceInfo.getQrCode());
        }
    });
    Scheduler::instance()->submitTask(task);
}

QSpinBox *ConfigFileSettingWidget::createIntSpinBox(int minValue, int maxValue, QWidget *parent) const
{
    auto *spin = new QSpinBox(parent);
    spin->setRange(minValue, maxValue);
    spin->setFixedWidth(140);
    return spin;
}

QDoubleSpinBox *ConfigFileSettingWidget::createDoubleSpinBox(double minValue,
                                                             double maxValue,
                                                             QWidget *parent) const
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(minValue, maxValue);
    spin->setDecimals(2);
    spin->setFixedWidth(140);
    return spin;
}

int ConfigFileSettingWidget::pageIndex(ConfigPageKind kind) const
{
    return m_pageIndexes.value(kind, -1);
}
