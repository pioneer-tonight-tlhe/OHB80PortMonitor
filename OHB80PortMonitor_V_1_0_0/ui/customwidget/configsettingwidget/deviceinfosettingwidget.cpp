#include "deviceinfosettingwidget.h"

#include "../settingwidget/settingitemwidget.h"
#include "app/ohbdeviceconfig.h"
#include "app/shareddata.h"
#include "modaltabledialog.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/set_device_info_task/set_device_info_task.h"

#include <QAbstractSocket>
#include <QDebug>
#include <QHostAddress>
#include <QIntValidator>
#include <QLabel>
#include <QSignalBlocker>

#include <algorithm>

DeviceInfoSettingWidget::DeviceInfoSettingWidget(QWidget* parent)
    : SettingWidget(parent)
{
    setTitle("Device Information Configuration");
    initUI();
}

DeviceInfoSettingWidget::~DeviceInfoSettingWidget() = default;

void DeviceInfoSettingWidget::initUI()
{
    initViewItem();
    initModifyItem();
    refreshQrCodeRange();
}

void DeviceInfoSettingWidget::initViewItem()
{
    m_viewItem = new SettingItemWidget(this);
    m_viewItem->setTitle("View Device Information");
    m_viewItem->setTip("Show QRCode, firmware version, IP and Port for all configured devices");

    m_viewButton = new QPushButton("View", m_viewItem);
    m_viewItem->addWidget("view_btn", m_viewButton);
    connect(m_viewButton, &QPushButton::clicked,
            this, &DeviceInfoSettingWidget::onViewClicked);

    addItem(m_viewItem);
}

void DeviceInfoSettingWidget::initModifyItem()
{
    m_currentQrCodeItem = new SettingItemWidget(this);
    m_currentQrCodeItem->setTitle("Current QRCode");
    m_currentQrCodeItem->setTip("Select the device to modify");

    m_targetQrCodeSpinBox = new QSpinBox(m_currentQrCodeItem);
    m_targetQrCodeSpinBox->setRange(0, 99999);
    m_targetQrCodeSpinBox->setFixedWidth(120);
    m_currentQrCodeItem->addWidget("target_qrcode_spin", m_targetQrCodeSpinBox);
    connect(m_targetQrCodeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &DeviceInfoSettingWidget::onTargetQrCodeChanged);

    addItem(m_currentQrCodeItem);

    m_qrCodeModifyItem = new SettingItemWidget(this);
    m_qrCodeModifyItem->setTitle("New QRCode");
    m_qrCodeModifyItem->setTip("Set a new QRCode from 0 to 99999; existing QRCode of another device is not allowed");

    m_newQrCodeSpinBox = new QSpinBox(m_qrCodeModifyItem);
    m_newQrCodeSpinBox->setRange(0, 99999);
    m_newQrCodeSpinBox->setFixedWidth(120);
    m_qrCodeModifyItem->addWidget("new_qrcode_spin", m_newQrCodeSpinBox);

    m_setQrCodeButton = new QPushButton("Set", m_qrCodeModifyItem);
    m_qrCodeModifyItem->addWidget("set_qrcode_btn", m_setQrCodeButton);
    connect(m_setQrCodeButton, &QPushButton::clicked,
            this, &DeviceInfoSettingWidget::onSetQrCodeClicked);

    addItem(m_qrCodeModifyItem);

    m_endpointModifyItem = new SettingItemWidget(this);
    m_endpointModifyItem->setTitle("IP / Port");
    m_endpointModifyItem->setTip("Set IP and Port for the selected current device");

    auto* ipLabel = new QLabel("IP", m_endpointModifyItem);
    m_endpointModifyItem->addWidget("ip_label", ipLabel);

    m_ipLineEdit = new QLineEdit(m_endpointModifyItem);
    m_ipLineEdit->setPlaceholderText("IPv4");
    m_ipLineEdit->setFixedWidth(150);
    m_endpointModifyItem->addWidget("ip_edit", m_ipLineEdit);

    auto* portLabel = new QLabel("Port", m_endpointModifyItem);
    m_endpointModifyItem->addWidget("port_label", portLabel);

    m_portLineEdit = new QLineEdit(m_endpointModifyItem);
    m_portLineEdit->setPlaceholderText("1-65535");
    m_portLineEdit->setValidator(new QIntValidator(1, 65535, m_portLineEdit));
    m_portLineEdit->setFixedWidth(90);
    m_endpointModifyItem->addWidget("port_edit", m_portLineEdit);

    m_setEndpointButton = new QPushButton("Set", m_endpointModifyItem);
    m_endpointModifyItem->addWidget("set_endpoint_btn", m_setEndpointButton);
    connect(m_setEndpointButton, &QPushButton::clicked,
            this, &DeviceInfoSettingWidget::onSetEndpointClicked);

    addItem(m_endpointModifyItem);
}

void DeviceInfoSettingWidget::onViewClicked()
{
    const QStringList qrcodes = sortedQrCodes();
    if (qrcodes.isEmpty()) {
        showMessage(QMessageBox::Warning, "View Failed", "No device information is available");
        return;
    }

    QList<QStringList> rows;
    ModbusTcpMasterManager& manager = ModbusTcpMasterManager::instance();
    OHBDeviceConfig& config = OHBDeviceConfig::getInstance();

    for (const QString& qrCode : qrcodes) {
        ModbusTcpMaster* master = manager.getMaster(qrCode);
        const OHBDeviceInfo configInfo = config.getDeviceByQRCode(qrCode);

        QString firmwareVersion = master ? master->firmwareVersion() : QString();
        if (firmwareVersion.trimmed().isEmpty()) {
            firmwareVersion = QStringLiteral("-");
        }

        const QString ip = master ? master->ip() : configInfo.ip;
        const quint16 port = master ? master->port() : configInfo.port;

        rows.append(QStringList{
            qrCode,
            firmwareVersion,
            ip.isEmpty() ? QStringLiteral("-") : ip,
            port > 0 ? QString::number(port) : QStringLiteral("-")
        });
    }

    ModalTableDialog::showAsync(this,
                                "Device Information",
                                QStringList{"QRCode", QStringLiteral("固件版本号"), "IP", "Port"},
                                rows);
}

void DeviceInfoSettingWidget::onSetQrCodeClicked()
{
    QString errorMessage;
    QString currentQrCode;
    QString newQrCode;
    QString ip;
    quint16 port = 0;

    if (!validateCurrentQrCode(&errorMessage, &currentQrCode)
        || !validateNewQrCode(&errorMessage, currentQrCode, &newQrCode)
        || !validateEndpoint(&errorMessage, &ip, &port)) {
        m_qrCodeModifyItem->setStatusFailed();
        showMessage(QMessageBox::Warning, "Set QRCode Failed", errorMessage);
        return;
    }

    submitDeviceInfoUpdate(currentQrCode,
                           newQrCode,
                           ip,
                           port,
                           m_qrCodeModifyItem,
                           newQrCode.toInt(),
                           currentQrCode.toInt());
}

void DeviceInfoSettingWidget::onSetEndpointClicked()
{
    QString errorMessage;
    QString currentQrCode;
    QString ip;
    quint16 port = 0;

    if (!validateCurrentQrCode(&errorMessage, &currentQrCode)
        || !validateEndpoint(&errorMessage, &ip, &port)) {
        m_endpointModifyItem->setStatusFailed();
        showMessage(QMessageBox::Warning, "Set IP / Port Failed", errorMessage);
        return;
    }

    submitDeviceInfoUpdate(currentQrCode,
                           currentQrCode,
                           ip,
                           port,
                           m_endpointModifyItem,
                           currentQrCode.toInt(),
                           currentQrCode.toInt());
}

void DeviceInfoSettingWidget::onTargetQrCodeChanged(int value)
{
    loadDeviceInfo(value);
}

void DeviceInfoSettingWidget::refreshQrCodeRange(int preferredQrCode)
{
    const QStringList qrcodes = sortedQrCodes();
    if (qrcodes.isEmpty()) {
        m_targetQrCodeSpinBox->setRange(0, 99999);
        m_newQrCodeSpinBox->setRange(0, 99999);
        m_targetQrCodeSpinBox->setValue(0);
        m_newQrCodeSpinBox->setValue(0);
        setSetButtonEnabled(false);
        return;
    }

    QVector<int> values;
    values.reserve(qrcodes.size());
    for (const QString& qrCode : qrcodes) {
        bool ok = false;
        const int value = qrCode.toInt(&ok);
        if (ok) {
            values.append(value);
        }
    }

    if (values.isEmpty()) {
        setSetButtonEnabled(false);
        return;
    }

    const auto minMax = std::minmax_element(values.constBegin(), values.constEnd());
    const int minValue = *minMax.first;
    const int maxValue = *minMax.second;
    int selectedValue = values.first();

    if (preferredQrCode >= minValue
        && preferredQrCode <= maxValue
        && qrcodes.contains(QString::number(preferredQrCode))) {
        selectedValue = preferredQrCode;
    }

    {
        QSignalBlocker targetBlocker(m_targetQrCodeSpinBox);
        QSignalBlocker newBlocker(m_newQrCodeSpinBox);
        m_targetQrCodeSpinBox->setRange(minValue, maxValue);
        m_newQrCodeSpinBox->setRange(0, 99999);
        m_targetQrCodeSpinBox->setValue(selectedValue);
        m_newQrCodeSpinBox->setValue(selectedValue);
    }

    setSetButtonEnabled(true);
    loadDeviceInfo(selectedValue);
}

void DeviceInfoSettingWidget::loadDeviceInfo(int qrCode)
{
    const QString qrCodeText = QString::number(qrCode);
    if (!qrCodeExists(qrCodeText)) {
        m_ipLineEdit->clear();
        m_portLineEdit->clear();
        m_newQrCodeSpinBox->setValue(qrCode);
        return;
    }

    ModbusTcpMaster* master = ModbusTcpMasterManager::instance().getMaster(qrCodeText);
    const OHBDeviceInfo configInfo = OHBDeviceConfig::getInstance().getDeviceByQRCode(qrCodeText);

    const QString ip = master ? master->ip() : configInfo.ip;
    const quint16 port = master ? master->port() : configInfo.port;

    QSignalBlocker blocker(m_newQrCodeSpinBox);
    m_newQrCodeSpinBox->setValue(qrCode);
    m_ipLineEdit->setText(ip);
    m_portLineEdit->setText(port > 0 ? QString::number(port) : QString());
}

bool DeviceInfoSettingWidget::validateCurrentQrCode(QString* errorMessage, QString* currentQrCode) const
{
    const QString currentCode = QString::number(m_targetQrCodeSpinBox->value());
    if (!qrCodeExists(currentCode)) {
        if (errorMessage) *errorMessage = QString("Current QRCode %1 does not exist").arg(currentCode);
        return false;
    }

    if (currentQrCode) *currentQrCode = currentCode;
    if (errorMessage) errorMessage->clear();
    return true;
}

bool DeviceInfoSettingWidget::validateNewQrCode(QString* errorMessage,
                                                const QString& currentQrCode,
                                                QString* newQrCode) const
{
    const QString newCode = QString::number(m_newQrCodeSpinBox->value());
    const int newValue = m_newQrCodeSpinBox->value();

    if (newValue < 0 || newValue > 99999) {
        if (errorMessage) *errorMessage = QStringLiteral("New QRCode must be 0-99999");
        return false;
    }

    if (currentQrCode != newCode && qrCodeExists(newCode)) {
        if (errorMessage) *errorMessage = QString("New QRCode %1 already belongs to another device").arg(newCode);
        return false;
    }

    if (newQrCode) *newQrCode = newCode;
    if (errorMessage) errorMessage->clear();
    return true;
}

bool DeviceInfoSettingWidget::validateEndpoint(QString* errorMessage, QString* ip, quint16* port) const
{
    const QString ipText = m_ipLineEdit->text().trimmed();
    const QString portText = m_portLineEdit->text().trimmed();

    QHostAddress address;
    if (!address.setAddress(ipText) || address.protocol() != QAbstractSocket::IPv4Protocol) {
        if (errorMessage) *errorMessage = QStringLiteral("IP must be a valid IPv4 address");
        return false;
    }

    bool portOk = false;
    const int portValue = portText.toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535) {
        if (errorMessage) *errorMessage = QStringLiteral("Port must be 1-65535");
        return false;
    }

    if (ip) *ip = ipText;
    if (port) *port = static_cast<quint16>(portValue);
    if (errorMessage) errorMessage->clear();
    return true;
}

void DeviceInfoSettingWidget::submitDeviceInfoUpdate(const QString& oldQrCode,
                                                     const QString& newQrCode,
                                                     const QString& ip,
                                                     quint16 port,
                                                     SettingItemWidget* statusItem,
                                                     int successPreferredQrCode,
                                                     int failedPreferredQrCode)
{
    auto* task = new SetDeviceInfoTask(this);
    task->setDeviceInfo(oldQrCode, newQrCode, ip, port);

    if (statusItem) {
        statusItem->setStatusWaiting();
    }
    setSetButtonEnabled(false);

    connect(task, &SetDeviceInfoTask::finished,
            this, [this, statusItem, successPreferredQrCode, failedPreferredQrCode](bool success, const QString& msg) {
                if (success) {
                    if (statusItem) {
                        statusItem->setStatusOK();
                    }
                    refreshQrCodeRange(successPreferredQrCode);
                    showMessage(QMessageBox::Information, "Set Succeeded", msg);
                } else {
                    if (statusItem) {
                        statusItem->setStatusFailed();
                    }
                    refreshQrCodeRange(failedPreferredQrCode);
                    showMessage(QMessageBox::Warning, "Set Failed", msg);
                }
                setSetButtonEnabled(true);
            },
            Qt::QueuedConnection);

    Scheduler::instance()->submitTask(task);
}

QStringList DeviceInfoSettingWidget::sortedQrCodes() const
{
    QStringList qrcodes = SharedData::getAllQrcodes();
    std::sort(qrcodes.begin(), qrcodes.end(), [](const QString& lhs, const QString& rhs) {
        bool lhsOk = false;
        bool rhsOk = false;
        const int lhsValue = lhs.toInt(&lhsOk);
        const int rhsValue = rhs.toInt(&rhsOk);
        if (lhsOk && rhsOk) {
            return lhsValue < rhsValue;
        }
        return lhs < rhs;
    });

    QStringList uniqueQrCodes;
    for (const QString& qrCode : qrcodes) {
        if (!uniqueQrCodes.contains(qrCode)) {
            uniqueQrCodes.append(qrCode);
        }
    }
    return uniqueQrCodes;
}

bool DeviceInfoSettingWidget::qrCodeExists(const QString& qrCode) const
{
    return sortedQrCodes().contains(qrCode);
}

void DeviceInfoSettingWidget::setSetButtonEnabled(bool enabled)
{
    if (m_setQrCodeButton) {
        m_setQrCodeButton->setEnabled(enabled);
    }
    if (m_setEndpointButton) {
        m_setEndpointButton->setEnabled(enabled);
    }
}

void DeviceInfoSettingWidget::showMessage(QMessageBox::Icon icon,
                                          const QString& title,
                                          const QString& text)
{
    auto* mb = new QMessageBox(icon, title, text, QMessageBox::Ok, this);
    mb->setAttribute(Qt::WA_DeleteOnClose);
    mb->setModal(false);
    mb->setWindowModality(Qt::NonModal);
    mb->show();
}
