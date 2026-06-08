#ifndef FIRMWARE_UPGRADER_H
#define FIRMWARE_UPGRADER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <QString>

class QTcpSocket;
class BinFileReader;
class ModbusTcpMaster;

class FirmwareUpgrader : public QObject
{
    Q_OBJECT
    friend class ModbusTcpMaster;

public:
    // ======== 执行状态枚举 ========
    enum class UpgradeState {
        Preparing = 0,          // 1.  固件升级准备阶段
        PrepareCmdSent,         // 2.  准备指令已发送
        PrepareCmdFinished,     // 3.  准备指令发送完毕
        WaitingDevice,          // 4.  等待设备准备
        WaitingDeviceFinished,  // 5.  等待设备准备完毕
        DataTransferStarted,    // 6.  开始分包发送bin文件
        SendingDataFrame,       // 7.  分包发送数据帧
        SendingLastFrame,       // 8.  发送最后一帧数据
        DataTransferFinished,   // 9.  分包发送数据帧完毕
        VersionCmdSent,         // 10. 发送获取版本号指令
        VersionCmdFinished,     // 11. 发送获取版本号指令完毕
        Finished                // 12. 固件升级完毕
    };
    Q_ENUM(UpgradeState)

    explicit FirmwareUpgrader(ModbusTcpMaster *master, QObject *parent = nullptr);
    ~FirmwareUpgrader();

    // 日志路径自动通过 AppLogger::ModbusMasterLoggerPath(m_master->ID) 获取

    // ======== 公开方法 ========

    // 开始升级，传入 bin 文件完整路径
    // 注意：BinFileReader 必须已预先完成文件读取（ReadComplete状态）
    //       且已按 {248, 256} 规则分包
    void start(const QString &binFilePath);

    // 停止/取消升级
    void stop();

    // 是否正在升级
    bool isRunning() const;

    // 准备指令响应超时时间(ms)
    void setPrepareTimeout(int ms);

    // 等待设备准备就绪的时间(ms)
    void setWaitingTime(int ms);

    // 分包发送时间间隔(ms)
    void setSendInterval(int ms);

    // bin文件数据发送指令的响应超时时间(ms)
    void setTransferTimeout(int ms);

    // 数据传输完成后等待设备重启加载新固件的时间(ms)
    void setPostTransferWaitTime(int ms);

    // ============================================================================
    // 设置 BinFileReader 引用（外部调用，必须设置）
    // ============================================================================
    // 【重要】在调用 start() 之前必须先调用此方法设置 BinFileReader！
    //
    // 批量升级场景（推荐）：
    //   - 上层模块创建一个共享的 BinFileReader 实例
    //   - 读取并分包一次 bin 文件
    //   - 将同一个实例传递给所有设备的 upgrader
    //   - 避免重复读取文件，节省内存和 I/O
    //
    // 示例：
    //   BinFileReader *sharedReader = new BinFileReader();
    //   sharedReader->setPacketSize({248, 256});
    //   sharedReader->readBinFile("/path/to/firmware.bin");
    //   
    //   for (auto *master : masters) {
    //       master->firmwareUpgrader()->setBinFileReader(sharedReader);
    //       master->firmwareUpgrader()->start("/path/to/firmware.bin");
    //   }
    // ============================================================================
    void setBinFileReader(BinFileReader *reader);

signals:
    // 执行状态变更信号（携带日志信息和相关数据帧）
    void stateChanged(const QString &masterId,
                      FirmwareUpgrader::UpgradeState state,
                      const QString &logMessage,
                      const QByteArray &frame);

    // 固件升级完成信号
    void finished(const QString &masterId,
                  bool success,
                  FirmwareUpgrader::UpgradeState state,
                  const QString &errorMessage);

    // 升级进度信号（百分比 0-100）
    void progress(const QString &masterId, int percent);

private slots:
    void onSocketReadyRead();
    void onTimeout();             // 通用超时槽（根据当前状态处理）
    void onSendTimer();
    void onSocketDisconnected();  // TCP断线 → 直接终止升级

private:
    // 根据文件名解析版本号（格式：xxx_1_0_0_xxx → "1.0.0"）
    QString parseVersionFromFilename(const QString &filePath);

    void sendPrepareCommand();
    void startDataTransfer();
    void sendVersionCommand();
    void finishWithResult(bool success, UpgradeState state, const QString &errorMsg);

    // onSocketReadyRead 分派子方法，返回 true 表示需要清理缓冲区
    bool handlePrepareResponse();
    bool handleTransferResponse();
    bool handleVersionResponse();
    void stopAllTimers();
    void connectSocketSignals();
    void disconnectSocketSignals();
    void emitState(UpgradeState state, const QString &msg,
                   const QByteArray &frame = QByteArray());

    // 固定指令帧
    static const QByteArray PREPARE_CMD;          // 01 06 00 AA 00 00 A9 EA
    static const QByteArray PREPARE_RESP;         // 01 06 00 AA 00 00 A9 EA
    static const QByteArray TRANSFER_OK_RESP;     // 01 06 00 BB 00 00 F9 EF
    static const QByteArray TRANSFER_ERR_SIZE;    // 01 06 00 BB 00 01 38 2F
    static const QByteArray TRANSFER_ERR_CRC;     // 01 06 00 BB 00 02 78 2E
    static const QByteArray TRANSFER_ERR_FRAME;   // 01 06 00 BB 00 03 B9 EE

    ModbusTcpMaster *m_master        = nullptr;
    QTcpSocket    *m_socket        = nullptr;
    BinFileReader *m_binFileReader = nullptr;
    QString       &m_firmwareVersion;              // 引用 ModbusTcpMaster::m_firmwareVersion
    bool           m_masterSubModulesPaused = false; // 标记是否已暂停 master 子模块

    UpgradeState m_state   = UpgradeState::Preparing;
    bool         m_running = false;

    // 配置参数
    int m_commandTimeout      = 3000;   // 指令响应超时(ms)，用于准备指令和版本指令
    int m_waitingTime         = 1000;   // 等待设备就绪时间(ms)
    int m_sendInterval        = 160;    // 分包发送间隔(ms)
    int m_transferTimeout     = 3000;   // 数据传输响应超时(ms)
    int m_postTransferWaitMs  = 2000;   // 数据传输完成后等待设备重启加载新固件的时间(ms)

    // 定时器（指针成员，以 this 为父对象，确保 moveToThread 时跟随迁移）
    QTimer *m_timeoutTimer;   // 通用超时定时器（准备/传输/版本指令共用）
    QTimer *m_sendTimer;      // 分包发送定时器

    // 升级过程状态数据
    QString    m_targetVersion;         // 从文件名解析的目标版本号
    QByteArray m_receiveBuffer;         // socket 接收缓冲区
    int        m_currentPacketIndex = 0;
    int        m_totalPackets       = 0;
    int        m_fileTotalSize      = 0;
    QByteArray m_fileCrc;              // 预计算的文件 CRC（2字节）
    QByteArray m_lastSentFrame;        // 最后发送的指令帧（用于超时时反馈）
};

#endif // FIRMWARE_UPGRADER_H
