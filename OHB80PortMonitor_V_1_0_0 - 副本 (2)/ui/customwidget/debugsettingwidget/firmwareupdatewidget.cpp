#include "firmwareupdatewidget.h"
#include "ui_firmwareupdatewidget.h"
#include "modbustcpmastermanager/modbustcpmastermanager.h"
#include "modbustcpmastermanager/modbustcpmaster/modbustcpmaster.h"
#include "scheduler/scheduler.h"
#include "scheduler/tasks/firmware_upgrade_task.h"
#include "app/customlogger.h"

#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFile>
#include <QSet>
#include <QPointer>
#include <QDebug>
#include <QRegularExpression>
#include <algorithm>
#include <QPixmap>
#include <QPainter>
#include <QDir>
#include <QScrollBar>
#include <QApplication>
#include <QJsonObject>

// ======================================================
// upgradeStateToPhase
// ======================================================
QString FirmwareUpdateWidget::upgradeStateToPhase(FirmwareUpgrader::UpgradeState state)
{
    using S = FirmwareUpgrader::UpgradeState;
    switch (state) {
    case S::Preparing:
    case S::PrepareCmdSent:
    case S::PrepareCmdFinished:     return QStringLiteral("准备阶段");
    case S::WaitingDevice:
    case S::WaitingDeviceFinished:  return QStringLiteral("等待设备");
    case S::DataTransferStarted:
    case S::SendingDataFrame:
    case S::SendingLastFrame:
    case S::DataTransferFinished:   return QStringLiteral("数据传输");
    case S::VersionCmdSent:
    case S::VersionCmdFinished:     return QStringLiteral("版本验证");
    case S::Finished:               return QStringLiteral("完成");
    }
    return QStringLiteral("未知");
}

// ======================================================
// 构造 / 析构
// ======================================================
FirmwareUpdateWidget::FirmwareUpdateWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FirmwareUpdateWidget)
    , m_totalDevices(0)
    , m_completedDevices(0)
    , m_successCount(0)
    , m_failCount(0)
    , m_captureDirectory(CustomLogger::FirmwareUpgradeCapturePath())
{
    ui->setupUi(this);
    initUI();
}

FirmwareUpdateWidget::~FirmwareUpdateWidget()
{
    delete ui;
}

// ======================================================
// initUI
// ======================================================
void FirmwareUpdateWidget::initUI()
{
    // 初始化表格
    initTableWidgetSelectedDevices();

    // 初始化进度条
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->progressBar->setAlignment(Qt::AlignCenter);
    ui->progressBar->setFormat("0/0");

    // 按钮连接
    connect(ui->btnAddDevice, &QPushButton::clicked, this, &FirmwareUpdateWidget::onAddDeviceForUpdate);
    connect(ui->btnAddAllDevices, &QPushButton::clicked, this, &FirmwareUpdateWidget::onAddAllDevices);
    connect(ui->btnClear, &QPushButton::clicked, this, &FirmwareUpdateWidget::onClear);
    connect(ui->btnUdateSelected, &QPushButton::clicked, this, &FirmwareUpdateWidget::onUpdateSelectedDevices);

    // 初始化日志组件
    initLoggerWidget();
}

// ======================================================
// initLoggerWidget — 配置 Logger Tab
// ======================================================
void FirmwareUpdateWidget::initLoggerWidget()
{
    ui->tabLogger->setRootPath(CustomLogger::FirmwareUpgradeLoggerPath());
    ui->tabLogger->setHeaders({
        QStringLiteral("level"),
        QStringLiteral("QRCode"),
        QStringLiteral("Phase"),
        QStringLiteral("Message")
    });
    ui->tabLogger->setFormat(
        QStringLiteral("[{}][{}][{}]:{}"),
        {QStringLiteral("level"),
         QStringLiteral("QRCode"),
         QStringLiteral("Phase"),
         QStringLiteral("Message")});

    // 按 level 字段设置每行样式
    ui->tabLogger->setItemStyler([](const QStringList &record, ItemStyle &style) {
        const QString &level = record.value(0);
        if (level == QStringLiteral("error")) {
            style.setForeground(QColor(255, 80,  80));
        } else if (level == QStringLiteral("warn")) {
            style.setForeground(QColor(255, 165,  0));
        }
    });

    ui->tabLogger->setPageSize(200);
    ui->tabLogger->initialize();
}

// ======================================================
// writeLog — 统一日志写入辅助
// ======================================================
void FirmwareUpdateWidget::writeLog(const QString &level, const QString &qrcode,
                                     const QString &phase, const QString &message)
{
    ui->tabLogger->writeLog(QJsonObject{
        {"level",   level},
        {"QRCode",  qrcode},
        {"Phase",   phase},
        {"Message", message}
    });
}

// ======================================================
// onAddDeviceForUpdate
// ======================================================
void FirmwareUpdateWidget::onAddDeviceForUpdate()
{
    QString qrCode = ui->lineEditQRCode->text().trimmed();
    if (qrCode.isEmpty()) {
        return;
    }

    // 检查表格中是否已存在该QRCode
    bool deviceExists = false;
    for (int row = 0; row < ui->tableWidgetSelectedDevices->rowCount(); ++row) {
        QLabel* qrCodeLabel = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(row, 0));
        if (qrCodeLabel && qrCodeLabel->text() == qrCode) {
            deviceExists = true;
            break;
        }
    }

    if (deviceExists) {
        QMessageBox::information(this, "Information", "Device already exists in the update list!");
        return;
    }

    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    ModbusTcpMaster *master = manager.getMaster(qrCode);
    if (!master) {
        QMessageBox::warning(this, "Warning", "Device not found!");
        return;
    }

    addDeviceToTable(master);
}

// ======================================================
// onAddAllDevices
// ======================================================
void FirmwareUpdateWidget::onAddAllDevices()
{
    ModbusTcpMasterManager &manager = ModbusTcpMasterManager::instance();
    QStringList allDeviceIds = manager.masterIds();

    if (allDeviceIds.isEmpty()) {
        return;
    }

    int previousCount = ui->tableWidgetSelectedDevices->rowCount();
    ui->tableWidgetSelectedDevices->setRowCount(0);
    m_qrcodeRowMap.clear();

    // 按数字排序 QRCode
    std::sort(allDeviceIds.begin(), allDeviceIds.end(), [](const QString &a, const QString &b) {
        QRegularExpression re("\\d+");
        QRegularExpressionMatch matchA = re.match(a);
        QRegularExpressionMatch matchB = re.match(b);
        if (matchA.hasMatch() && matchB.hasMatch()) {
            return matchA.captured().toInt() < matchB.captured().toInt();
        }
        return a < b;
    });

    int addedCount = 0;
    for (const QString &qrCode : allDeviceIds) {
        ModbusTcpMaster *master = manager.getMaster(qrCode);
        if (master) {
            addDeviceToTable(master);
            addedCount++;
        }
    }

    qDebug() << "[FirmwareUpdateWidget] Cleared" << previousCount << "devices, added" << addedCount << "devices in numerical order";

    updateTableHeight();
    resetProgress();
}

// ======================================================
// onClear
// ======================================================
void FirmwareUpdateWidget::onClear()
{
    int rowCount = ui->tableWidgetSelectedDevices->rowCount();
    if (rowCount == 0) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Clear All Devices",
        QString("Are you sure you want to clear all %1 device(s) from the table?").arg(rowCount),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        ui->tableWidgetSelectedDevices->setRowCount(0);
        m_qrcodeRowMap.clear();
        updateTableHeight();
        resetProgress();
    }
}

// ======================================================
// onUpdateSelectedDevices
// ======================================================
void FirmwareUpdateWidget::onUpdateSelectedDevices()
{
    qDebug() << "[FirmwareUpdateWidget] onUpdateSelectedDevices called";

    // 1. 验证固件文件是否存在
    if (m_firmwareFilePath.isEmpty()) {
        qDebug() << "[FirmwareUpdateWidget] Firmware file path is empty";
        QMessageBox::warning(this, "Warning", "Please import .bin file first!");
        return;
    }

    QFile file(m_firmwareFilePath);
    if (!file.exists()) {
        qDebug() << "[FirmwareUpdateWidget] Firmware file not found:" << m_firmwareFilePath;
        QMessageBox::warning(this, "Warning",
            QString("Firmware file not found!\n%1\nPlease import .bin file first.").arg(m_firmwareFilePath));
        return;
    }

    qDebug() << "[FirmwareUpdateWidget] Firmware file OK:" << m_firmwareFilePath;

    // 2. 遍历表格获取所有设备的QRCode列表
    QStringList selectedDeviceIds;

    if (ui->tableWidgetSelectedDevices->rowCount() == 0) {
        qDebug() << "[FirmwareUpdateWidget] No devices in table";
        QMessageBox::information(this, "Information", "No devices in the update list!");
        return;
    }

    for (int row = 0; row < ui->tableWidgetSelectedDevices->rowCount(); ++row) {
        QLabel* qrCodeLabel = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(row, 0));
        if (qrCodeLabel) {
            selectedDeviceIds.append(qrCodeLabel->text());
        }
    }

    if (selectedDeviceIds.isEmpty()) {
        QMessageBox::warning(this, "Warning", "No valid devices selected!");
        return;
    }

    // 启动前确认
    QFileInfo fi(m_firmwareFilePath);
    QMessageBox::StandardButton confirm = QMessageBox::question(
        this, "Confirm Firmware Update",
        QString("Start upgrading %1 device(s)?\n\nFirmware: %2")
            .arg(selectedDeviceIds.count())
            .arg(fi.fileName()),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirm != QMessageBox::Yes) return;

    qDebug() << "[FirmwareUpdateWidget] Starting upgrade for" << selectedDeviceIds.count() << "devices";

    // 重置成功/失败计数
    m_successCount = 0;
    m_failCount = 0;

    // 3. 将表格中对应设备状态设置为 Wait
    for (int row = 0; row < ui->tableWidgetSelectedDevices->rowCount(); ++row) {
        QLabel* qrCodeLabel = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(row, 0));
        if (qrCodeLabel && selectedDeviceIds.contains(qrCodeLabel->text())) {
            QLabel* statusLabel = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(row, 2));
            if (statusLabel) {
                statusLabel->setText(getStatusText(Waiting));
                statusLabel->setStyleSheet(getStatusStyle(Waiting));
            }
        }
    }

    // 4. 设置进度跟踪
    m_totalDevices = selectedDeviceIds.count();
    m_completedDevices = 0;
    m_completedDeviceIds.clear();
    updateProgressBar();

    // 5. 创建固件升级调度任务并连接信号
    FirmwareUpgradeTask *task = new FirmwareUpgradeTask(selectedDeviceIds, m_firmwareFilePath);
    connect(task, &FirmwareUpgradeTask::deviceProgress,  this, &FirmwareUpdateWidget::onTaskDeviceProgress,  Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeTask::deviceStateLog,  this, &FirmwareUpdateWidget::onTaskDeviceStateLog,  Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeTask::deviceFinished,  this, &FirmwareUpdateWidget::onTaskDeviceFinished,  Qt::QueuedConnection);
    connect(task, &FirmwareUpgradeTask::allProgress,     this, &FirmwareUpdateWidget::onTaskAllProgress,     Qt::QueuedConnection);
    Scheduler::instance()->submitTask(task);
    qDebug() << "[FirmwareUpdateWidget] FirmwareUpgradeTask submitted to Scheduler";
}

// ======================================================
// 属性 getter/setter
// ======================================================
void FirmwareUpdateWidget::setFirmwareFilePath(const QString &filePath)
{
    m_firmwareFilePath = filePath;
}

QString FirmwareUpdateWidget::firmwareFilePath() const
{
    return m_firmwareFilePath;
}

void FirmwareUpdateWidget::setCaptureDirectory(const QString &dirPath)
{
    m_captureDirectory = dirPath;
}

QString FirmwareUpdateWidget::captureDirectory() const
{
    return m_captureDirectory;
}

// ======================================================
// 表格初始化
// ======================================================
void FirmwareUpdateWidget::initTableWidget(QTableWidget *table, const QStringList &headers)
{
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);

    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::MultiSelection);
    table->setAlternatingRowColors(true);

    table->setColumnWidth(0, 150);  // QRCode
    table->setColumnWidth(1, 200);  // 进度条
    table->setColumnWidth(2, 100);  // 状态
    table->setColumnWidth(3, 200);  // Result
    table->setColumnWidth(4, 80);   // Action

    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
}

void FirmwareUpdateWidget::initTableWidgetSelectedDevices()
{
    QStringList headers = {"QRCode", "Progress", "Status", "Result Message", "Action"};
    initTableWidget(ui->tableWidgetSelectedDevices, headers);
    ui->tableWidgetSelectedDevices->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void FirmwareUpdateWidget::addDeviceToTable(ModbusTcpMaster *master)
{
    int row = ui->tableWidgetSelectedDevices->rowCount();
    ui->tableWidgetSelectedDevices->insertRow(row);

    QString qrCode = master ? master->ID : "Unknown";

    // 第0列：QRCode
    QLabel *qrCodeLabel = new QLabel(qrCode);
    qrCodeLabel->setAlignment(Qt::AlignCenter);
    ui->tableWidgetSelectedDevices->setCellWidget(row, 0, qrCodeLabel);

    // 第1列：进度条
    QProgressBar *progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setAlignment(Qt::AlignCenter);
    ui->tableWidgetSelectedDevices->setCellWidget(row, 1, progressBar);

    // 第2列：状态
    QLabel *statusLabel = new QLabel(getStatusText(Idle));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet(getStatusStyle(Idle));
    ui->tableWidgetSelectedDevices->setCellWidget(row, 2, statusLabel);

    // 第3列：Result Message
    QLabel *resultMessageLabel = new QLabel("");
    resultMessageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    resultMessageLabel->setWordWrap(true);
    ui->tableWidgetSelectedDevices->setCellWidget(row, 3, resultMessageLabel);

    // 第4列：删除按钮
    QPushButton *deleteButton = new QPushButton("Delete");
    deleteButton->setMaximumWidth(70);
    ui->tableWidgetSelectedDevices->setCellWidget(row, 4, deleteButton);

    // O(1) 行索引缓存
    m_qrcodeRowMap[qrCode] = row;

    connect(deleteButton, &QPushButton::clicked, this, [this, qrCode]() {
        int r = m_qrcodeRowMap.value(qrCode, -1);
        if (r >= 0) {
            ui->tableWidgetSelectedDevices->removeRow(r);
            // 重建 hash（删除行后行号偏移）
            m_qrcodeRowMap.clear();
            for (int i = 0; i < ui->tableWidgetSelectedDevices->rowCount(); ++i) {
                QLabel *lbl = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(i, 0));
                if (lbl) m_qrcodeRowMap[lbl->text()] = i;
            }
            updateTableHeight();
        }
    });

    updateTableHeight();
}

// ======================================================
// 表格高度 / 进度条
// ======================================================
void FirmwareUpdateWidget::updateTableHeight()
{
    int rowCount = ui->tableWidgetSelectedDevices->rowCount();
    const int rowHeight = 30;
    int headerHeight = ui->tableWidgetSelectedDevices->horizontalHeader()->height();
    int totalHeight = headerHeight + (rowCount * rowHeight) + 4;

    ui->tableWidgetSelectedDevices->setMinimumHeight(totalHeight);
    ui->tableWidgetSelectedDevices->setMaximumHeight(totalHeight);

    if (rowCount == 0) {
        ui->tableWidgetSelectedDevices->setMinimumHeight(headerHeight + 20);
        ui->tableWidgetSelectedDevices->setMaximumHeight(headerHeight + 20);
    }

    ui->tableWidgetSelectedDevices->updateGeometry();
}

void FirmwareUpdateWidget::updateProgressBar()
{
    QString format = QString("%1/%2").arg(m_completedDevices).arg(m_totalDevices);
    ui->progressBar->setFormat(format);

    int percentage = 0;
    if (m_totalDevices > 0) {
        percentage = (m_completedDevices * 100) / m_totalDevices;
    }
    ui->progressBar->setValue(percentage);
}

void FirmwareUpdateWidget::resetProgress()
{
    m_totalDevices = 0;
    m_completedDevices = 0;
    m_successCount = 0;
    m_failCount = 0;
    m_completedDeviceIds.clear();
    updateProgressBar();
}

// ======================================================
// 状态文本 / 样式
// ======================================================
QString FirmwareUpdateWidget::getStatusText(DeviceStatus status) const
{
    switch (status) {
    case Idle:     return "Idle";
    case Waiting:  return "Waiting";
    case Updating: return "Updating";
    case Success:  return "Success";
    case Failed:   return "Failed";
    default:       return "Unknown";
    }
}

QString FirmwareUpdateWidget::getStatusStyle(DeviceStatus status) const
{
    switch (status) {
    case Idle:     return "QLabel { color: green; font-weight: bold; }";
    case Waiting:  return "QLabel { color: orange; font-weight: bold; }";
    case Updating: return "QLabel { color: blue; font-weight: bold; }";
    case Success:  return "QLabel { color: #00AA00; font-weight: bold; }";
    case Failed:   return "QLabel { color: red; font-weight: bold; }";
    default:       return "QLabel { color: gray; font-weight: bold; }";
    }
}

// ======================================================
// 固件升级任务信号处理槽
// ======================================================
void FirmwareUpdateWidget::onTaskDeviceProgress(const QString &qrcode, int percent)
{
    int row = m_qrcodeRowMap.value(qrcode, -1);
    if (row < 0) return;
    QProgressBar *bar       = qobject_cast<QProgressBar*>(ui->tableWidgetSelectedDevices->cellWidget(row, 1));
    QLabel       *statusLbl = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(row, 2));
    if (bar) bar->setValue(percent);
    if (statusLbl && percent > 0 && percent < 100) {
        statusLbl->setText(getStatusText(Updating));
        statusLbl->setStyleSheet(getStatusStyle(Updating));
    }
}

void FirmwareUpdateWidget::onTaskDeviceStateLog(const QString &qrcode,
                                                FirmwareUpgrader::UpgradeState state,
                                                const QString &logMessage,
                                                const QByteArray &frame)
{
    qDebug() << "[FirmwareUpdateWidget][onTaskDeviceStateLog] 接收状态日志:" << qrcode
             << "状态:" << static_cast<int>(state) << "消息:" << logMessage;

    using S = FirmwareUpgrader::UpgradeState;

    // 所有状态均写入日志
    writeLog("info", qrcode, upgradeStateToPhase(state), logMessage);

    // 表格第3列实时刷新：SendingDataFrame 高频，跳过 QLabel 更新避免 UI 抖动
    if (state != S::SendingDataFrame) {
        int row = m_qrcodeRowMap.value(qrcode, -1);
        if (row >= 0) {
            QLabel *resultLbl = qobject_cast<QLabel*>(
                ui->tableWidgetSelectedDevices->cellWidget(row, 3));
            if (resultLbl) resultLbl->setText(logMessage);
        }
    }

    // 数据帧原始内容写入
    if (!frame.isEmpty() &&
        (state == S::DataTransferStarted ||
         state == S::SendingDataFrame    ||
         state == S::SendingLastFrame))
    {
        const int total = frame.size();
        for (int offset = 0; offset < total; offset += 128) {
            const int end = qMin(offset + 128, total);
            QString hex;
            hex.reserve((end - offset) * 3);
            for (int i = offset; i < end; ++i) {
                if (i > offset) hex += ' ';
                hex += QString::number(static_cast<quint8>(frame[i]), 16).rightJustified(2, '0').toUpper();
            }
            writeLog("info", qrcode, QStringLiteral("数据帧"), hex);
        }
    }
}

void FirmwareUpdateWidget::onTaskDeviceFinished(const QString &qrcode, bool success, const QString &message)
{
    qDebug() << "[FirmwareUpdateWidget][onTaskDeviceFinished] 接收设备完成信号:" << qrcode
             << "成功:" << success << "消息:" << message;

    int row = m_qrcodeRowMap.value(qrcode, -1);
    if (row >= 0) {
        QProgressBar *bar       = qobject_cast<QProgressBar*>(ui->tableWidgetSelectedDevices->cellWidget(row, 1));
        QLabel       *statusLbl = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(row, 2));
        QLabel       *resultLbl = qobject_cast<QLabel*>(ui->tableWidgetSelectedDevices->cellWidget(row, 3));
        if (success) {
            if (bar)       bar->setValue(100);
            if (statusLbl) { statusLbl->setText(getStatusText(Success)); statusLbl->setStyleSheet(getStatusStyle(Success)); }
            if (resultLbl) { resultLbl->setText("升级成功"); resultLbl->setStyleSheet("QLabel { color: green; }"); }
            ++m_successCount;
        } else {
            if (statusLbl) { statusLbl->setText(getStatusText(Failed)); statusLbl->setStyleSheet(getStatusStyle(Failed)); }
            if (resultLbl) { resultLbl->setText(message); resultLbl->setStyleSheet("QLabel { color: red; }"); }
            ++m_failCount;
        }
    } else {
        qDebug() << "[FirmwareUpdateWidget][onTaskDeviceFinished] 警告: 未找到设备" << qrcode << "在表格中的行";
    }
    // 设备完成日志
    writeLog(success ? "info" : "error", qrcode, QStringLiteral("完成"), message);
}

void FirmwareUpdateWidget::onTaskAllProgress(int completed, int total)
{
    m_completedDevices = completed;
    m_totalDevices     = total;
    updateProgressBar();

    if (completed >= total && total > 0) {
        // 先把表格最终状态渲染出来，再截图
        ui->tableWidgetSelectedDevices->viewport()->repaint();
        captureTableWidgetScreenshot();

        QString summary;
        summary += QString("Firmware update finished.\n\n");
        summary += QString("Total:   %1 device(s)\n").arg(total);
        summary += QString("Success: %1 device(s)\n").arg(m_successCount);
        summary += QString("Failed:  %1 device(s)").arg(m_failCount);

        if (m_failCount == 0) {
            QMessageBox::information(this, "Update Complete", summary);
        } else {
            QMessageBox::warning(this, "Update Complete (with failures)", summary);
        }
    }
}

// ======================================================
// 截图功能
// ======================================================
bool FirmwareUpdateWidget::ensureCaptureDirectoryExists()
{
    QDir dir(m_captureDirectory);
    if (!dir.exists()) {
        return dir.mkpath(".");
    }
    return true;
}

void FirmwareUpdateWidget::captureTableWidgetScreenshot()
{
    if (!ensureCaptureDirectoryExists()) {
        qDebug() << "[FirmwareUpdateWidget] 无法创建截图目录:" << m_captureDirectory;
        return;
    }

    QTableWidget *table = ui->tableWidgetSelectedDevices;
    if (table->rowCount() == 0) return;

    // 计算完整内容尺寸
    int headerH  = table->horizontalHeader()->isVisible()
                   ? table->horizontalHeader()->height() : 0;
    int vHeaderW = table->verticalHeader()->isVisible()
                   ? table->verticalHeader()->width() : 0;

    QVector<int> colW(table->columnCount());
    int totalW = vHeaderW;
    for (int c = 0; c < table->columnCount(); c++) {
        colW[c] = table->columnWidth(c);
        totalW += colW[c];
    }

    QVector<int> rowH(table->rowCount());
    int contentH = 0;
    for (int r = 0; r < table->rowCount(); r++) {
        rowH[r]   = table->rowHeight(r);
        contentH += rowH[r];
    }
    int totalH = headerH + contentH;

    // 构建全尺寸 Pixmap
    QPixmap fullPixmap(totalW, totalH);
    fullPixmap.fill(table->palette().color(QPalette::Base));
    QPainter painter(&fullPixmap);

    // 绘制表头
    if (headerH > 0) {
        table->horizontalHeader()->render(
            &painter, QPoint(vHeaderW, 0), QRegion(),
            QWidget::DrawWindowBackground | QWidget::DrawChildren);
    }

    // 逐行逐列渲染 cellWidget
    int yOff = headerH;
    const QColor gridColor = table->palette().color(QPalette::Mid);
    for (int row = 0; row < table->rowCount(); row++) {
        int xOff = vHeaderW;
        for (int col = 0; col < table->columnCount(); col++) {
            QWidget *w = table->cellWidget(row, col);
            if (w) {
                w->render(&painter, QPoint(xOff, yOff), QRegion(),
                          QWidget::DrawWindowBackground | QWidget::DrawChildren);
            } else {
                QTableWidgetItem *item = table->item(row, col);
                if (item) {
                    painter.drawText(
                        QRect(xOff + 4, yOff, colW[col] - 4, rowH[row]),
                        Qt::AlignVCenter | Qt::AlignLeft, item->text());
                }
            }
            xOff += colW[col];
        }
        painter.setPen(gridColor);
        painter.drawLine(0, yOff + rowH[row] - 1, totalW, yOff + rowH[row] - 1);
        yOff += rowH[row];
    }

    // 列分隔线
    painter.setPen(gridColor);
    int xLine = vHeaderW;
    for (int c = 0; c < table->columnCount() - 1; c++) {
        xLine += colW[c];
        painter.drawLine(xLine, headerH, xLine, totalH);
    }

    painter.end();

    // 保存文件
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString fileName  = QString("firmware_update_%1.png").arg(timestamp);
    QString filePath  = QDir(m_captureDirectory).filePath(fileName);

    if (fullPixmap.save(filePath, "PNG")) {
        qDebug() << "[FirmwareUpdateWidget] 截图已保存"
                 << QString("(%1x%2)").arg(totalW).arg(totalH) << filePath;
    } else {
        qDebug() << "[FirmwareUpdateWidget] 截图保存失败:" << filePath;
    }
}
