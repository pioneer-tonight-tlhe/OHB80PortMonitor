#include "firmwareupgrader.h"
#include "modbustcpmaster.h"
#include "modbuscrc.h"
#include "modbuslogger.h"
#include "binfilereader.h"
#include "qthelper.h"

#include <QTcpSocket>
#include <QRegularExpression>
#include <QFileInfo>
#include <QThread>

// ======== 固定指令帧定义 ========
const QByteArray FirmwareUpgrader::PREPARE_CMD       = QByteArray::fromHex("010600AA0000A9EA");
const QByteArray FirmwareUpgrader::PREPARE_RESP      = QByteArray::fromHex("010600AA0000A9EA");
const QByteArray FirmwareUpgrader::TRANSFER_OK_RESP  = QByteArray::fromHex("010600BB0000F9EF");
const QByteArray FirmwareUpgrader::TRANSFER_ERR_SIZE = QByteArray::fromHex("010600BB0001382F");
const QByteArray FirmwareUpgrader::TRANSFER_ERR_CRC  = QByteArray::fromHex("010600BB0002782E");
const QByteArray FirmwareUpgrader::TRANSFER_ERR_FRAME= QByteArray::fromHex("010600BB0003B9EE");

FirmwareUpgrader::FirmwareUpgrader(ModbusTcpMaster *master, QObject *parent)
    : QObject(parent)
    , m_master(master)
    , m_socket(master->m_socket)
    , m_firmwareVersion(master->m_firmwareVersion)
{
    m_timeoutTimer = new QTimer(this);
    m_sendTimer    = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_sendTimer->setSingleShot(false);

    connect(m_timeoutTimer, &QTimer::timeout,
            this, &FirmwareUpgrader::onTimeout);
    connect(m_sendTimer, &QTimer::timeout,
            this, &FirmwareUpgrader::onSendTimer);
}

FirmwareUpgrader::~FirmwareUpgrader()
{
    stop();
}

// ======================================================
// setBinFileReader
// ======================================================
void FirmwareUpgrader::setBinFileReader(BinFileReader *reader)
{
    m_binFileReader = reader;
}

// ======================================================
// isRunning / 参数设置器
// ======================================================
bool FirmwareUpgrader::isRunning() const { return m_running; }

void FirmwareUpgrader::setPrepareTimeout(int ms)     { m_commandTimeout      = ms; }
void FirmwareUpgrader::setWaitingTime(int ms)        { m_waitingTime         = ms; }
void FirmwareUpgrader::setSendInterval(int ms)       { m_sendInterval        = ms; }
void FirmwareUpgrader::setTransferTimeout(int ms)    { m_transferTimeout     = ms; }
void FirmwareUpgrader::setPostTransferWaitTime(int ms) { m_postTransferWaitMs = ms; }

// ======================================================
// emitState — 统一状态更新 + 信号 + 日志
// ======================================================
void FirmwareUpgrader::emitState(UpgradeState state, const QString &msg, const QByteArray &frame)
{
    m_state = state;
    emit stateChanged(m_master->ID, state, msg, frame);
    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "emitState", msg);
}

// ======================================================
// start()  ——  固件升级入口
// ======================================================
void FirmwareUpgrader::start(const QString &binFilePath)
{
    // 跨线程安全：若调用线程非所属线程，转发到所属线程执行
    if (QThread::currentThread() != thread()) {
        qDebug() << "[FirmwareUpgrader][start] 设备ID=" << m_master->ID
                 << "跨线程调用, 当前线程=" << QThread::currentThread()
                 << "目标线程=" << thread()
                 << "目标线程运行中=" << thread()->isRunning();
        bool ok = QMetaObject::invokeMethod(this, [this, binFilePath]() {
            qDebug() << "[FirmwareUpgrader][start] 设备ID=" << m_master->ID
                     << "lambda 已在 worker 线程执行, 线程=" << QThread::currentThread();
            start(binFilePath);
        }, Qt::QueuedConnection);
        qDebug() << "[FirmwareUpgrader][start] 设备ID=" << m_master->ID
                 << "invokeMethod 结果=" << ok;
        return;
    }

    qDebug() << "[FirmwareUpgrader][start] 设备ID=" << m_master->ID
             << "进入 start() 主体, 线程=" << QThread::currentThread();

    if (m_running) {
        qDebug() << "[FirmwareUpgrader][start] 设备ID=" << m_master->ID << "已在运行中，忽略";
        ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "start",
            "升级器已在运行中，忽略重复启动");
        return;
    }

    // 检查 master 连接状态
    if (!m_master || !m_master->isConnected()) {
        qDebug() << "[FirmwareUpgrader][start] 设备ID=" << m_master->ID << "设备未连接";
        finishWithResult(false, UpgradeState::Preparing,
                         "[固件升级] 执行状态: Preparing - 设备未连接，无法开始固件升级");
        return;
    }

    m_running = true;
    m_receiveBuffer.clear();
    m_currentPacketIndex = 0;

    // 1. 进入准备阶段
    emitState(UpgradeState::Preparing,
              QString("[固件升级] 准备阶段开始，文件: %1").arg(binFilePath));

    // 2. 从文件名解析目标版本号
    m_targetVersion = parseVersionFromFilename(binFilePath);
    if (m_targetVersion.isEmpty()) {
        finishWithResult(false, UpgradeState::Preparing,
                         QString("[固件升级] 执行状态: Preparing - 无法从文件名解析版本号: %1").arg(binFilePath));
        return;
    }
    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "start",
        QString("目标版本号=%1").arg(m_targetVersion));

    // ============================================================================
    // 3. 检查 BinFileReader（必须由外部预先设置）
    // ============================================================================
    // 【严重警告】禁止在此处自动创建 BinFileReader 对象！
    // 
    // 原因：当系统中有 1000 个 ModbusTcpMaster 设备需要同时升级时，
    //       如果每个 FirmwareUpgrader 都自动创建自己的 BinFileReader，
    //       将会导致：
    //       1. 同一个 bin 文件被读取 1000 次（严重的磁盘 I/O 浪费）
    //       2. 内存中存在 1000 份相同的 bin 文件数据副本（内存占用爆炸）
    //       3. 1000 次文件解析和分包操作（CPU 资源浪费）
    //
    // 正确做法：
    //       在批量升级场景下，应由 调度层 或其他相对上层模块（例如：ModbusTcpMasterManager）：
    //       1. 创建一个共享的 BinFileReader 实例
    //       2. 读取并分包一次 bin 文件
    //       3. 通过 setBinFileReader() 将同一个实例传递给所有设备的 upgrader
    //       4. 所有设备共享同一份文件数据，节省内存和 I/O
    // ============================================================================
    if (!m_binFileReader) {
        finishWithResult(false, UpgradeState::Preparing,
                         "[固件升级] 执行状态: Preparing - BinFileReader 未设置，必须在调用 start() 前通过 setBinFileReader() 设置");
        return;
    }

    // 验证 BinFileReader 状态（必须由上层保证已读取完成）
    if (m_binFileReader->readState() != BinFileReader::ReadComplete) {
        finishWithResult(false, UpgradeState::Preparing,
                         "[固件升级] 执行状态: Preparing - BinFileReader 状态异常，必须由上层保证文件已读取完成");
        return;
    }
    
    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "start",
        "使用预设的 BinFileReader（已读取完成）");

    m_totalPackets  = m_binFileReader->packetCount();
    m_fileTotalSize = m_binFileReader->getAllBytes().size();

    if (m_totalPackets == 0 || m_fileTotalSize == 0) {
        finishWithResult(false, UpgradeState::Preparing, "[固件升级] 执行状态: Preparing - bin 文件为空");
        return;
    }

    // 4. 预计算文件 CRC
    // CRC 覆盖范围：01 06 00 BB + Len(2) + 55 AA + 全部 bin 数据
    {
        quint16 lenValue = static_cast<quint16>(m_fileTotalSize + 2);
        QByteArray crcInput;
        crcInput.append(QByteArray::fromHex("010600BB"));
        crcInput.append(static_cast<char>((lenValue >> 8) & 0xFF));
        crcInput.append(static_cast<char>(lenValue & 0xFF));
        crcInput.append(QByteArray::fromHex("55AA"));
        crcInput.append(m_binFileReader->getAllBytes());
        m_fileCrc = ModbusCrc::modbusCRC16(crcInput);
    }

    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "start",
        QString("文件大小=%1字节，分包数=%2，CRC=%3")
            .arg(m_fileTotalSize).arg(m_totalPackets).arg(QString(m_fileCrc.toHex(' ').toUpper())));

    // 5. 暂停 master 普通收发子模块（含断开 receiver 的 socket 信号槽），防止干扰固件升级期间的 socket 通信
    m_master->pauseChildren();
    m_masterSubModulesPaused = true;

    // 5.5 排空 socket 缓冲区中可能残留的旧 Modbus 响应帧
    // receiver 暂停时 socket 内部 Qt 缓冲区可能仍有上轮正常通信遗留的数据，
    // 不清除会在下一次 readyRead 时被 readAll() 一并读入 m_receiveBuffer，
    // 导致准备升级响应校验失败（"准备升级指令响应异常"）
    if (m_socket && m_socket->bytesAvailable() > 0) {
        ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "start",
            QString("排空 socket 残留数据 %1 字节").arg(m_socket->bytesAvailable()));
        m_socket->readAll();
    }

    // 6. 连接 socket 信号
    connectSocketSignals();

    // 7. 发送准备升级指令
    sendPrepareCommand();
}

// ======================================================
// stop()
// ======================================================
void FirmwareUpgrader::stop()
{
    // 跨线程安全：若调用线程非所属线程，转发到所属线程执行
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() {
            stop();
        }, Qt::QueuedConnection);
        return;
    }

    if (!m_running) return;

    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "stop", "升级流程被取消");

    stopAllTimers();
    disconnectSocketSignals();

    m_running = false;
    m_state   = UpgradeState::Preparing;
    m_receiveBuffer.clear();

    if (m_masterSubModulesPaused) {
        m_masterSubModulesPaused = false;
        m_master->resumeChildren();
    }
}

// ======================================================
// parseVersionFromFilename
// 文件名格式：xxx_1_0_xxx.bin → "1.0"
// ======================================================
QString FirmwareUpgrader::parseVersionFromFilename(const QString &filePath)
{
    QString fileName = QFileInfo(filePath).fileName();
    QRegularExpression re(R"(_(\d+)_(\d+)(?:[_.]|$))");
    QRegularExpressionMatch match = re.match(fileName);
    if (!match.hasMatch()) return QString();
    return QString("%1.%2")
        .arg(match.captured(1))
        .arg(match.captured(2));
}

// ======================================================
// sendPrepareCommand  (State 2: PrepareCmdSent)
// ======================================================
void FirmwareUpgrader::sendPrepareCommand()
{
    m_lastSentFrame = PREPARE_CMD;
    
    emitState(UpgradeState::PrepareCmdSent,
              QString("[固件升级] 发送准备升级指令: %1")
                  .arg(QString(PREPARE_CMD.toHex(' ').toUpper())),
              PREPARE_CMD);

    qDebug() << "[FirmwareUpgrader] [设备ID=" << m_master->ID << "] sendPrepareCommand:"
             << "socketState=" << m_socket->state()
             << "socketValid=" << (m_socket != nullptr)
             << "thread=" << QThread::currentThread()
             << "socketThread=" << m_socket->thread();

    const qint64 written = m_socket->write(PREPARE_CMD);
    const bool flushed = m_socket->flush();
    qDebug() << "[FirmwareUpgrader] [设备ID=" << m_master->ID << "] sendPrepareCommand:"
             << "written=" << written << "/" << PREPARE_CMD.size()
             << "flushed=" << flushed
             << "bytesToWrite=" << m_socket->bytesToWrite();

    m_timeoutTimer->start(m_commandTimeout);
}

// ======================================================
// onTimeout  — 通用超时处理（根据当前状态分派）
// ======================================================
void FirmwareUpgrader::onTimeout()
{
    switch (m_state) {
    case UpgradeState::PrepareCmdSent:
        // 准备指令超时
        emitState(UpgradeState::PrepareCmdFinished,
                  "[固件升级] 准备升级指令超时，未收到响应",
                  m_lastSentFrame);
        finishWithResult(false, UpgradeState::PrepareCmdFinished,
                         "[固件升级] 执行状态: PrepareCmdFinished - 不响应固件升级准备升级指令");
        break;

    case UpgradeState::SendingLastFrame:
        // 数据传输响应超时
        emitState(UpgradeState::DataTransferFinished,
                  "[固件升级] 数据传输响应超时，未收到响应",
                  m_lastSentFrame);
        finishWithResult(false, UpgradeState::DataTransferFinished,
                         "[固件升级] 执行状态: DataTransferFinished - 数据传输失败，未收到数据传输响应帧");
        break;

    case UpgradeState::VersionCmdSent:
        // 版本指令超时
        emitState(UpgradeState::VersionCmdFinished,
                  "[固件升级] 获取版本号指令超时，未收到响应",
                  m_lastSentFrame);
        finishWithResult(false, UpgradeState::VersionCmdFinished,
                         "[固件升级] 执行状态: VersionCmdFinished - 版本校对超时，未收到 AreYouThere 响应");
        break;

    default:
        break;
    }
}

// ======================================================
// startDataTransfer  — 构建并发送第一帧 (State 6)
// 第一帧格式: 01 06 00 BB + Len(2) + 55 AA + packetAt(0)[248字节]
// ======================================================
void FirmwareUpgrader::startDataTransfer()
{
    QByteArray pkt0 = m_binFileReader->packetAt(0);
    quint16 lenValue = static_cast<quint16>(m_fileTotalSize + 2);

    QByteArray firstFrame;
    firstFrame.append(QByteArray::fromHex("010600BB"));
    firstFrame.append(static_cast<char>((lenValue >> 8) & 0xFF));
    firstFrame.append(static_cast<char>(lenValue & 0xFF));
    firstFrame.append(QByteArray::fromHex("55AA"));
    firstFrame.append(pkt0);

    bool isOnlyFrame = m_binFileReader->isLastPacket(0);

    if (isOnlyFrame) {
        // 只有一帧：第一帧同时也是最后帧，追加 CRC
        firstFrame.append(m_fileCrc);
        m_lastSentFrame = firstFrame;

        emitState(UpgradeState::SendingLastFrame,
                  QString("[固件升级] 单帧传输（%1 字节，含 CRC），等待响应")
                      .arg(firstFrame.size()),
                  firstFrame);

        m_socket->write(firstFrame);
        m_socket->flush();
        emit progress(m_master->ID, 100);
        m_timeoutTimer->start(m_transferTimeout);
    } else {
        emitState(UpgradeState::DataTransferStarted,
                  QString("[固件升级] 发送第一帧（%1 字节，数据区 %2 字节）")
                      .arg(firstFrame.size()).arg(pkt0.size()),
                  firstFrame);

        m_socket->write(firstFrame);
        m_socket->flush();
        emit progress(m_master->ID, (1 * 100) / qMax(1, m_totalPackets));

        m_currentPacketIndex = 1;
        m_sendTimer->setInterval(m_sendInterval);
        m_sendTimer->start();
    }
}

// ======================================================
// onSendTimer  — 按间隔发送后续数据帧 (State 7/8)
// 中间帧：直接发送 bin 数据分包
// 最后一帧：bin 数据 + CRC(2字节)
// ======================================================
void FirmwareUpgrader::onSendTimer()
{
    if (!m_binFileReader || m_currentPacketIndex >= m_totalPackets) {
        m_sendTimer->stop();
        return;
    }

    bool isLast    = m_binFileReader->isLastPacket(m_currentPacketIndex);
    QByteArray pkt = m_binFileReader->packetAt(m_currentPacketIndex);

    if (!isLast) {
        emitState(UpgradeState::SendingDataFrame,
                  QString("[固件升级] 发送数据帧 %1/%2（%3 字节）")
                      .arg(m_currentPacketIndex + 1).arg(m_totalPackets).arg(pkt.size()),
                  pkt);

        m_socket->write(pkt);
        m_socket->flush();
        emit progress(m_master->ID, ((m_currentPacketIndex + 1) * 100) / qMax(1, m_totalPackets));
        m_currentPacketIndex++;
    } else {
        m_sendTimer->stop();

        QByteArray lastFrame = pkt;
        lastFrame.append(m_fileCrc);
        m_lastSentFrame = lastFrame;

        emitState(UpgradeState::SendingLastFrame,
                  QString("[固件升级] 发送最后一帧（%1 字节，含 CRC %2 字节），等待响应")
                      .arg(lastFrame.size()).arg(m_fileCrc.size()),
                  lastFrame);

        m_socket->write(lastFrame);
        m_socket->flush();
        emit progress(m_master->ID, 100);
        m_currentPacketIndex++;

        m_timeoutTimer->start(m_transferTimeout);
    }
}

// ======================================================
// sendVersionCommand  — 发送版本号查询 (State 10)
// 帧格式：01 04 00 15 00 01 CRC(2)，Modbus RTU 读输入寄存器
// ======================================================
void FirmwareUpgrader::sendVersionCommand()
{
    QByteArray frame;
    frame.append(char(0x01));  // Slave Addr
    frame.append(char(0x04));  // Function (读输入寄存器)
    frame.append(char(0x00));  // Start Addr high
    frame.append(char(0x15));  // Start Addr low
    frame.append(char(0x00));  // Register Count high
    frame.append(char(0x01));  // Register Count low
    frame.append(ModbusCrc::modbusCRC16(frame));
    m_lastSentFrame = frame;

    emitState(UpgradeState::VersionCmdSent,
              QString("[固件升级] 发送获取版本号指令 (Modbus RTU): %1")
                  .arg(QString(frame.toHex(' ').toUpper())),
              frame);

    qint64 written = m_socket->write(frame);
    bool flushed = m_socket->flush();

    qDebug() << "[FirmwareUpgrader] [sendVersionCommand] socket state:" << m_socket->state()
             << "isOpen:" << m_socket->isOpen()
             << "isValid:" << m_socket->isValid()
             << "bytesAvailable:" << m_socket->bytesAvailable()
             << "bytesToWrite:" << m_socket->bytesToWrite()
             << "written:" << written << "flushed:" << flushed
             << "frame:" << frame.toHex(' ').toUpper();

    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "sendVersionCommand",
        QString("发送版本号查询指令 socketState=%1 isOpen=%2 written=%3 flushed=%4")
            .arg(static_cast<int>(m_socket->state())).arg(m_socket->isOpen()).arg(written).arg(flushed));

    m_timeoutTimer->start(m_commandTimeout);
}

// ======================================================
// onSocketReadyRead  — 统一响应分派（基于当前状态）
// ======================================================
void FirmwareUpgrader::onSocketReadyRead()
{
    if (!m_socket) return;
    QByteArray incoming = m_socket->readAll();
    m_receiveBuffer.append(incoming);

    qDebug() << "[FirmwareUpgrader] [设备ID=" << m_master->ID << "] onSocketReadyRead:"
             << "incoming=" << incoming.size() << "bufTotal=" << m_receiveBuffer.size()
             << "raw=" << incoming.toHex(' ').toUpper();

    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "onSocketReadyRead",
        QString("readyRead state=%1 incoming=%2 bufTotal=%3")
            .arg(static_cast<int>(m_state)).arg(incoming.size()).arg(m_receiveBuffer.size()));

    bool shouldClearBuffer = false;

    switch (m_state) {
    case UpgradeState::PrepareCmdSent:    shouldClearBuffer = handlePrepareResponse();   break;
    case UpgradeState::SendingLastFrame:  shouldClearBuffer = handleTransferResponse();  break;
    case UpgradeState::VersionCmdSent:    shouldClearBuffer = handleVersionResponse();   break;
    default: break;
    }

    if (shouldClearBuffer) {
        m_receiveBuffer.clear();
        if (m_socket && m_socket->bytesAvailable() > 0)
            m_socket->readAll();
    }
}

// ======================================================
// handlePrepareResponse  — 处理准备升级响应（State 2 → 3 → 4）
// KMP 搜索完整 PREPARE_RESP（8字节），跳过残留 Modbus 帧
// ======================================================
bool FirmwareUpgrader::handlePrepareResponse()
{
    int foundPos = QtHelper::kmpSearch(m_receiveBuffer, PREPARE_RESP);
    if (foundPos < 0) return false;

    m_timeoutTimer->stop();

    if (foundPos > 0) {
        ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "handlePrepareResponse",
            QString("跳过缓冲区头部 %1 字节残留帧，找到准备升级响应").arg(foundPos));
    }

    QByteArray resp = m_receiveBuffer.mid(foundPos, PREPARE_RESP.size());

    emitState(UpgradeState::PrepareCmdFinished,
              QString("[固件升级] 准备升级响应成功: %1")
                  .arg(QString(resp.toHex(' ').toUpper())),
              resp);

    emitState(UpgradeState::WaitingDevice,
              QString("[固件升级] 等待设备准备就绪 %1 ms").arg(m_waitingTime));

    QTimer::singleShot(m_waitingTime, this, [this]() {
        if (!m_running) return;
        emitState(UpgradeState::WaitingDeviceFinished,
                  QString("[固件升级] 设备等待完毕（%1 ms），开始数据传输").arg(m_waitingTime));
        startDataTransfer();
    });
    return true;
}

// ======================================================
// handleTransferResponse  — 处理数据传输完成响应（State 8 → 9）
// 对四种合法响应帧做 KMP 搜索，取最早命中位置
// ======================================================
bool FirmwareUpgrader::handleTransferResponse()
{
    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "handleTransferResponse",
        QString("SendingLastFrame readyRead bufSize=%1 buf=%2")
            .arg(m_receiveBuffer.size()).arg(QString(m_receiveBuffer.toHex(' ').toUpper())));

    struct { const QByteArray *pattern; int pos; } candidates[] = {
        { &TRANSFER_OK_RESP,   QtHelper::kmpSearch(m_receiveBuffer, TRANSFER_OK_RESP)   },
        { &TRANSFER_ERR_SIZE,  QtHelper::kmpSearch(m_receiveBuffer, TRANSFER_ERR_SIZE)  },
        { &TRANSFER_ERR_CRC,   QtHelper::kmpSearch(m_receiveBuffer, TRANSFER_ERR_CRC)   },
        { &TRANSFER_ERR_FRAME, QtHelper::kmpSearch(m_receiveBuffer, TRANSFER_ERR_FRAME) },
    };

    int foundPos = -1;
    const QByteArray *matchedPattern = nullptr;
    for (auto &c : candidates) {
        if (c.pos >= 0 && (foundPos < 0 || c.pos < foundPos)) {
            foundPos       = c.pos;
            matchedPattern = c.pattern;
        }
    }
    if (foundPos < 0) return false;

    m_timeoutTimer->stop();

    if (foundPos > 0) {
        ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "handleTransferResponse",
            QString("SendingLastFrame 跳过缓冲区头部 %1 字节残留帧").arg(foundPos));
    }

    QByteArray resp = m_receiveBuffer.mid(foundPos, matchedPattern->size());

    if (resp == TRANSFER_OK_RESP) {
        emitState(UpgradeState::DataTransferFinished,
                  QString("[固件升级] 数据传输完成响应成功: %1")
                      .arg(QString(resp.toHex(' ').toUpper())),
                  resp);

        // 设备完成数据传输后需要时间重启/加载新固件，等待后再发版本查询
        emitState(UpgradeState::DataTransferFinished,
                  QString("[固件升级] 等待设备重启加载新固件 %1 ms").arg(m_postTransferWaitMs));
        m_timeoutTimer->stop();  // 停掉旧定时器，避免误触发
        // 排空可能的残留数据
        m_receiveBuffer.clear();
        if (m_socket && m_socket->bytesAvailable() > 0) {
            m_socket->readAll();
        }
        QTimer::singleShot(m_postTransferWaitMs, this, [this]() {
            if (!m_running) return;
            sendVersionCommand();
        });
    } else {
        QString errMsg;
        if (resp == TRANSFER_ERR_SIZE)       errMsg = "Bin 字节数异常";
        else if (resp == TRANSFER_ERR_CRC)   errMsg = "Bin 校验异常";
        else if (resp == TRANSFER_ERR_FRAME) errMsg = "Bin 帧头帧尾异常";
        else errMsg = QString("数据传输响应异常: %1")
                          .arg(QString(resp.toHex(' ').toUpper()));

        emitState(UpgradeState::DataTransferFinished,
                  QString("[固件升级] 数据传输失败: %1，响应: %2")
                      .arg(errMsg, QString(resp.toHex(' ').toUpper())),
                  resp);
        finishWithResult(false, UpgradeState::DataTransferFinished,
                         QString("[固件升级] 执行状态: DataTransferFinished - %1").arg(errMsg));
    }
    return true;
}

// ======================================================
// handleVersionResponse  — 处理版本号响应（State 10 → 11 → 12）
// Modbus RTU 响应：01 04 02 ver(2) crc(2) = 7 字节
// ======================================================
bool FirmwareUpgrader::handleVersionResponse()
{
    qDebug() << "[FirmwareUpgrader] [handleVersionResponse] receiveBuffer:"
             << m_receiveBuffer.toHex(' ').toUpper()
             << "size:" << m_receiveBuffer.size();

    if (m_receiveBuffer.size() < 7) {
        qDebug() << "[FirmwareUpgrader] [handleVersionResponse] buffer too small, need >= 7, got" << m_receiveBuffer.size();
        return false;
    }

    // KMP 搜索帧头 01 04，再验证字节计数（offset+2 == 0x02）
    static const QByteArray VERSION_RESP_PREFIX = QByteArray::fromHex("0104");
    int startPos = -1;
    int searchFrom = 0;
    while (true) {
        int hit = QtHelper::kmpSearch(m_receiveBuffer.mid(searchFrom), VERSION_RESP_PREFIX);
        if (hit < 0) break;
        int candidate = searchFrom + hit;
        if (candidate + 7 > m_receiveBuffer.size()) break;
        if (static_cast<quint8>(m_receiveBuffer[candidate + 2]) == 0x02) {
            startPos = candidate;
            break;
        }
        searchFrom = candidate + 1;
    }
    if (startPos < 0) {
        qDebug() << "[FirmwareUpgrader] [handleVersionResponse] frame header not found";
        return false;
    }
    if (m_receiveBuffer.size() < startPos + 7) {
        qDebug() << "[FirmwareUpgrader] [handleVersionResponse] incomplete frame, need" << (startPos + 7) << "got" << m_receiveBuffer.size();
        return false;
    }

    qDebug() << "[FirmwareUpgrader] [handleVersionResponse] found frame at pos:" << startPos;

    // 验证 CRC
    QByteArray resp = m_receiveBuffer.mid(startPos, 7);
    QByteArray payload = resp.mid(0, 5);  // 不包含 CRC (Slave+Function+ByteCount+Data)
    // Modbus RTU CRC：低字节在前，高字节在后
    quint16 receivedCrc = (static_cast<quint8>(resp[6]) << 8) | static_cast<quint8>(resp[5]);
    QByteArray crcBytes = ModbusCrc::modbusCRC16(payload);
    quint16 calculatedCrc = (static_cast<quint8>(crcBytes[1]) << 8) | static_cast<quint8>(crcBytes[0]);

    qDebug() << "[FirmwareUpgrader] [handleVersionResponse] resp:" << resp.toHex(' ').toUpper()
             << "payload:" << payload.toHex(' ').toUpper()
             << "receivedCrc:" << QString::number(receivedCrc, 16)
             << "calculatedCrc:" << QString::number(calculatedCrc, 16);

    // 无论 CRC 校验成功或失败，都打印响应帧
    ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "handleVersionResponse",
        QString("收到版本号响应帧=%1").arg(QString(resp.toHex(' ').toUpper())));

    if (receivedCrc != calculatedCrc) {
        ModbusLogger::masterWarn(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "handleVersionResponse",
            QString("CRC校验失败，接收=%1，计算=%2")
                .arg(receivedCrc, 4, 16, QChar('0')).arg(calculatedCrc, 4, 16, QChar('0')));
        return false;
    }

    m_timeoutTimer->stop();

    emitState(UpgradeState::VersionCmdFinished,
              QString("[固件升级] 收到版本号响应: %1")
                  .arg(QString(resp.toHex(' ').toUpper())),
              resp);

    // 解析版本字节：01 04 02 [ver1] [ver2] crc(2)
    // 版本号是 ASCII 字符，例如 31 31 = "1.1"
    quint8 ver1 = static_cast<quint8>(resp[3]);
    quint8 ver2 = static_cast<quint8>(resp[4]);
    QString currentVersion = QString("%1.%2").arg(ver1 - 0x30).arg(ver2 - 0x30);

    // 更新 ModbusTcpMaster 中的固件版本号
    m_firmwareVersion = currentVersion;

    bool matched = (currentVersion == m_targetVersion);
    emitState(UpgradeState::Finished,
              QString("[固件升级] 版本校对%1：当前 %2，目标 %3")
                  .arg(matched ? "成功" : "不匹配", currentVersion, m_targetVersion),
              resp);

    emit finished(m_master->ID, matched, UpgradeState::Finished,
                  matched ? QString()
                          : QString("[固件升级] 执行状态: Finished - 版本号不匹配：当前 %1，目标 %2")
                                .arg(currentVersion, m_targetVersion));
    m_running = false;
    disconnectSocketSignals();
    stopAllTimers();
    if (m_masterSubModulesPaused) {
        m_masterSubModulesPaused = false;
        m_master->resumeChildren();
    }
    return true;
}

// ======================================================
// finishWithResult  — 统一失败/异常终止
// ======================================================
void FirmwareUpgrader::finishWithResult(bool success, UpgradeState state, const QString &errorMsg)
{
    stopAllTimers();
    disconnectSocketSignals();
    m_running = false;

    if (m_masterSubModulesPaused) {
        m_masterSubModulesPaused = false;
        m_master->resumeChildren();
    }

    QString logMsg;
    if (success) {
        logMsg = QString("[data][FirmwareUpgrader][finishWithResult]：设备ID=%1 固件升级成功完成").arg(m_master->ID);
    } else {
        logMsg = QString("[data][FirmwareUpgrader][finishWithResult]：设备ID=%1 固件升级失败终止（状态 %2）：%3")
                     .arg(m_master->ID).arg(static_cast<int>(state)).arg(errorMsg);
    }

    qDebug() << logMsg;
    if (success) {
        ModbusLogger::masterInfo(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "finishWithResult",
            "固件升级成功完成");
    } else {
        ModbusLogger::masterWarn(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "finishWithResult",
            QString("固件升级失败终止，状态=%1，原因=%2").arg(static_cast<int>(state)).arg(errorMsg));
    }

    emit finished(m_master->ID, success, state, errorMsg);
}

// ======================================================
// 辅助方法
// ======================================================
void FirmwareUpgrader::stopAllTimers()
{
    m_timeoutTimer->stop();
    m_sendTimer->stop();
}

void FirmwareUpgrader::connectSocketSignals()
{
    if (m_socket) {
        bool c1 = connect(m_socket, &QIODevice::readyRead,
                this, &FirmwareUpgrader::onSocketReadyRead,
                Qt::UniqueConnection);
        bool c2 = connect(m_socket, &QAbstractSocket::disconnected,
                this, &FirmwareUpgrader::onSocketDisconnected,
                Qt::UniqueConnection);
        qDebug() << "[FirmwareUpgrader] [设备ID=" << m_master->ID << "] connectSocketSignals:"
                 << "readyRead连接=" << c1 << "disconnected连接=" << c2;
    } else {
        qDebug() << "[FirmwareUpgrader] [设备ID=" << m_master->ID << "] connectSocketSignals: m_socket 为空!";
    }
}

void FirmwareUpgrader::disconnectSocketSignals()
{
    if (m_socket) {
        disconnect(m_socket, &QIODevice::readyRead,
                   this, &FirmwareUpgrader::onSocketReadyRead);
        disconnect(m_socket, &QAbstractSocket::disconnected,
                   this, &FirmwareUpgrader::onSocketDisconnected);
    }
}

// ======================================================
// onSocketDisconnected — TCP 断线，直接终止升级并报告失败
// ======================================================
void FirmwareUpgrader::onSocketDisconnected()
{
    if (!m_running) return;

    const QString errMsg = QString("[固件升级] TCP 连接断开（升级阶段: %1）")
                               .arg(static_cast<int>(m_state));
    QString logMsg = QString("[data][FirmwareUpgrader][onSocketDisconnected]：设备ID=%1 %2").arg(m_master->ID).arg(errMsg);
    qDebug() << logMsg;
    ModbusLogger::masterWarn(m_master->ID, "ModbusTcpMaster", "FirmwareUpgrader", "onSocketDisconnected", errMsg);

    finishWithResult(false, m_state, errMsg);
}
