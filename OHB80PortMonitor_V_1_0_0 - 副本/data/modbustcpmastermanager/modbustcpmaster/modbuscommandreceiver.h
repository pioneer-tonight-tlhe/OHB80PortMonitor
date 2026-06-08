#ifndef MODBUSCOMMANDRECEIVER_H
#define MODBUSCOMMANDRECEIVER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>
#include "modbuscommand.h"

// ============================================================
// ModbusCommandReceiver - Modbus 指令接收器
//
// 负责接收并解析 Modbus TCP 响应帧，支持超时检测和 CRC 校验。
// 使用环形缓冲区处理 TCP 数据分片，支持帧头匹配和前缀丢弃。
// ============================================================
class ModbusCommandReceiver : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param socket 外部共享的 TCP Socket
     * @param masterId Master 设备 ID
     * @param parent 父对象
     */
    explicit ModbusCommandReceiver(QTcpSocket& socket, const QString& masterId = QString(), QObject* parent = nullptr);

    // 禁用拷贝和赋值（引用成员不支持）
    ModbusCommandReceiver(const ModbusCommandReceiver&) = delete;
    ModbusCommandReceiver& operator=(const ModbusCommandReceiver&) = delete;

    /**
     * @brief 开始接收指定指令的响应
     * @param cmd 待接收响应的指令（包含期望的响应帧模板）
     * @return 成功返回 true，失败返回 false（已有待处理命令时）
     * @details 保存指令模板，启动超时定时器，开始监听 socket 数据
     */
    bool beginReceive(const ModbusCommand& cmd);

    /**
     * @brief 取消当前待处理的命令
     * @details 停止超时定时器，清除待处理状态
     */
    void cancelPending();

    /**
     * @brief 检查是否有待处理的命令
     * @return 有待处理命令返回 true，否则返回 false
     */
    bool hasPendingCommand() const;

    /**
     * @brief 断开 socket 的 readyRead/disconnected 信号与本对象槽的连接
     * @details 固件升级期间需要独占 socket，调用此方法避免 receiver 抢读数据
     */
    void disconnectSocketSignalSlots();

    /**
     * @brief 重新连接 socket 的 readyRead/disconnected 信号到本对象槽
     */
    void reconnectSocketSignalSlots();

signals:
    /**
     * @brief 指令成功信号
     * @param cmd 成功的指令（包含接收到的响应帧）
     */
    void commandSucceeded(ModbusCommand cmd);

    /**
     * @brief 指令失败信号
     * @param cmd 失败的指令
     * @param timedOut 是否超时
     * @param checksumError 是否 CRC 校验错误
     */
    void commandFailed(ModbusCommand cmd, bool timedOut, bool checksumError);

private slots:
    /**
     * @brief Socket 有数据可读槽函数
     * @details 读取数据到环形缓冲区，尝试处理待处理帧
     */
    void onReadyRead();

    /**
     * @brief 响应超时槽函数
     * @details 停止定时器，标记当前命令超时失败
     */
    void onResponseTimeout();

    /**
     * @brief Socket 断开连接槽函数
     * @details 如果有待处理命令，标记为失败
     */
    void onSocketDisconnected();

private:
    /**
     * @brief 处理环形缓冲区中的待处理帧
     * @return 成功提取到有效帧返回 true，否则返回 false
     */
    bool processPendingFrame();

    /**
     * @brief 计算期望的响应帧长度
     * @param cmd 指令对象
     * @return 期望的字节长度
     */
    int expectedResponseLength(const ModbusCommand& cmd) const;

    /**
     * @brief 尝试从环形缓冲区提取匹配的帧
     * @param cmd 指令对象（包含期望的帧头）
     * @param frame 输出提取到的完整帧
     * @param discardedPrefixBytes 输出丢弃的前缀字节数
     * @param discardedData 输出被丢弃的数据
     * @return 成功提取返回 true，否则返回 false
     */
    bool tryExtractMatchedFrame(const ModbusCommand& cmd, QByteArray& frame, int& discardedPrefixBytes, QByteArray& discardedData);

    /**
     * @brief 检查帧头是否匹配
     * @param cmd 指令对象
     * @param frame 待检查的帧数据
     * @return 匹配返回 true，否则返回 false
     */
    bool isFrameHeaderMatch(const ModbusCommand& cmd, const QByteArray& frame) const;

    /**
     * @brief 验证响应帧的有效性
     * @param cmd 指令对象（包含期望的响应帧模板）
     * @param frame 待验证的响应帧
     * @param errorMessage 输出错误信息
     * @return 验证通过返回 true，否则返回 false
     */
    bool validateResponseFrame(const ModbusCommand& cmd, const QByteArray& frame, QString& errorMessage) const;

    /**
     * @brief 填充指令的响应帧
     * @param cmd 指令对象（输出）
     * @param frame 接收到的响应帧
     */
    void fillResponseFrame(ModbusCommand& cmd, const QByteArray& frame) const;

    /**
     * @brief 标记当前待处理命令失败
     * @param errorMessage 错误信息
     * @param timedOut 是否超时
     * @param checksumError 是否 CRC 校验错误
     */
    void failPendingCommand(const QString& errorMessage, bool timedOut, bool checksumError);

    /**
     * @brief 标记当前待处理命令成功
     * @param frame 接收到的响应帧
     */
    void succeedPendingCommand(const QByteArray& frame);

    /**
     * @brief 将原始收发帧写入 raw/{masterId}.log
     * @param cmdId 指令ID（如 ReadFoupStatus）
     * @param requestBytes 请求帧原始字节
     * @param responseBytes 响应帧原始字节（失败时为空）
     * @param errorInfo 失败时的错误描述（成功时为空）
     */
    void logRawFrame(const QString& cmdId,
                     const QByteArray& requestBytes,
                     const QByteArray& responseBytes,
                     const QString& errorInfo);

    /**
     * @brief 向环形缓冲区追加数据
     * @param data 要追加的数据
     */
    void ringAppend(const QByteArray& data);

    /**
     * @brief 从环形缓冲区读取数据
     * @param offset 偏移量
     * @param length 读取长度
     * @return 读取到的数据
     */
    QByteArray ringPeek(int offset, int length) const;

    /**
     * @brief 从环形缓冲区消费数据
     * @param length 消费长度
     */
    void ringConsume(int length);

    /**
     * @brief 获取环形缓冲区当前大小
     * @return 当前字节数
     */
    int ringSize() const;

    /**
     * @brief 清空环形缓冲区
     */
    void clearRingBuffer();

    /**
     * @brief 计算 CRC16 校验码
     * @param data 待校验数据
     * @return CRC16 校验码
     */
    static quint16 crc16(const QByteArray& data);

    // 成员变量
    QTcpSocket*      m_socket = nullptr;          // 外部共享的 Socket（不拥有）
    QString&         m_masterId;                 // Master 设备 ID（引用）
    QTimer*          m_responseTimer = nullptr;   // 响应超时定时器
    bool             m_hasPendingCommand = false; // 是否有待处理命令
    ModbusCommand    m_pendingCommand;           // 当前待处理的指令
    QVector<quint8>  m_ringBuffer;               // 环形缓冲区
    int              m_ringHead = 0;             // 环形缓冲区头指针
    int              m_ringSize = 0;             // 环形缓冲区当前大小
};

#endif // MODBUSCOMMANDRECEIVER_H
