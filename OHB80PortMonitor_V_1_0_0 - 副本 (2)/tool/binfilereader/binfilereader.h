#ifndef BINFILEREADER_H
#define BINFILEREADER_H

#include <QObject>
#include <QByteArray>
#include <QVector>
#include <QMutex>
#include <QFutureWatcher>

/**
 * @brief BinFileReader 二进制文件读取器
 * 
 * 功能：
 * - 异步读取 bin 文件
 * - 按自定义规则分包
 * - 线程安全
 * - 信号反馈进度与结果
 */
class BinFileReader : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 文件读取状态枚举
     */
    enum ReadState {
        NotRead = 0,      ///< 未读取
        Reading,          ///< 读取中
        ReadComplete,     ///< 读取完毕
        ReadFailed        ///< 读取失败
    };
    Q_ENUM(ReadState)

    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit BinFileReader(QObject *parent = nullptr);
    ~BinFileReader();

    /**
     * @brief 设置统一分包大小
     * @param bytesPerPacket 每帧字节数（<=0 时使用默认 256）
     */
    void setPacketSize(int bytesPerPacket);

    /**
     * @brief 设置自定义分包规则
     * @param bytesPerPacket 每帧字节数向量
     * 
     * 说明：
     * - 按顺序设置每一帧的字节数
     * - 遍历完毕后，剩余帧按照 vector 最后一个数量设置
     * - 最后一帧不足则直接成帧
     * 
     * 例：{23, 256}
     * - 第1帧：23字节
     * - 第2帧起：256字节
     * - 最后一帧：<=256字节
     */
    void setPacketSize(const QVector<int>& bytesPerPacket);

    /**
     * @brief 异步读取 bin 文件
     * @param filePath 文件路径
     * 
     * 流程：
     * - 状态设为 Reading
     * - 使用 QtConcurrent 异步读取
     * - 读取完成后按分包规则分包
     * - 发射进度与结果信号
     */
    void readBinFile(const QString& filePath);

    /**
     * @brief 查询当前读取状态
     * @return ReadState 枚举值
     */
    ReadState readState() const;

    /**
     * @brief 清理读取器
     * 
     * 操作：
     * - 状态恢复为 NotRead
     * - 释放原始数据与分包内存
     */
    void clean();

    /**
     * @brief 获取分包总数
     * @return 分包数量
     */
    int packetCount() const;

    /**
     * @brief 根据索引获取分包数据
     * @param index 索引（0-based）
     * @return 对应分包数据，越界返回空 QByteArray
     */
    QByteArray packetAt(int index) const;

    /**
     * @brief 获取读取到的文件的所有字节内容
     * @return 原始文件数据的常量引用，未读取时返回空 QByteArray
     * @warning 返回的是内部数据引用，请不要在外部修改返回的数据
     */
    const QByteArray& getAllBytes() const;

    /**
     * @brief 判断当前获取的第 index 块是不是最后一块数据包
     * @param index 索引（0-based）
     * @return true 是最后一块，false 不是最后一块或索引无效
     */
    bool isLastPacket(int index) const;

signals:
    /**
     * @brief 文件读取进度信号
     * @param percent 进度百分比（0~100）
     */
    void sigReadProgress(int percent);

    /**
     * @brief 文件读取完毕信号
     * @param success 是否成功
     * @param errorMsg 失败消息（成功时为空）
     */
    void sigReadFinished(bool success, const QString& errorMsg);

private:
    /**
     * @brief 按分包规则将原始数据分包
     */
    void splitData();

    ReadState m_state;                  ///< 当前读取状态

    QByteArray m_rawData;               ///< 原始文件数据
    QVector<QByteArray> m_packets;      ///< 分包数据

    QVector<int> m_packetSizes;         ///< 自定义分包规则
    int m_defaultPacketSize;            ///< 默认分包大小（256）

    QFutureWatcher<void>* m_watcher;    ///< 异步任务监视器
    mutable QMutex m_mutex;             ///< 线程安全互斥锁
};

#endif // BINFILEREADER_H
