#include "binfilereader.h"
#include "loggermanager.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtConcurrent>

BinFileReader::BinFileReader(QObject *parent)
    : QObject(parent)
    , m_state(NotRead)
    , m_defaultPacketSize(256)
    , m_watcher(new QFutureWatcher<void>(this))
{
    LoggerManager::getInstance()->log("debug", Level::INFO, "BinFileReader 构造完成, 默认分包大小: {}", m_defaultPacketSize);
}

BinFileReader::~BinFileReader()
{
    LoggerManager::getInstance()->log("debug", Level::INFO, "BinFileReader 析构开始");
    clean();
    LoggerManager::getInstance()->log("debug", Level::INFO, "BinFileReader 析构完成");
}

QString BinFileReader::parseVersionFromFileName(const QString& filePath)
{
    const QString fileName = QFileInfo(filePath).fileName();
    static const QRegularExpression versionRe(
        QStringLiteral(R"((?:^|[^A-Za-z0-9])V_(\d+)_(\d+)_(\d+)(?=\D|$))"));

    const QRegularExpressionMatch match = versionRe.match(fileName);
    if (!match.hasMatch()) {
        return QString();
    }

    return QStringLiteral("V%1.%2.%3")
        .arg(match.captured(1), match.captured(2), match.captured(3));
}

void BinFileReader::setPacketSize(int bytesPerPacket)
{
    QMutexLocker locker(&m_mutex);
    m_packetSizes.clear();
    m_defaultPacketSize = (bytesPerPacket > 0) ? bytesPerPacket : 256;
    LoggerManager::getInstance()->log("debug", Level::INFO, "setPacketSize(int) 统一分包大小设置为: {}", m_defaultPacketSize);
}

void BinFileReader::setPacketSize(const QVector<int>& bytesPerPacket)
{
    QMutexLocker locker(&m_mutex);
    m_packetSizes = bytesPerPacket;
    if (!bytesPerPacket.isEmpty()) {
        m_defaultPacketSize = bytesPerPacket.last();
    }
    LoggerManager::getInstance()->log("debug", Level::INFO, "setPacketSize(QVector) 自定义分包规则, 规则数: {}, 默认尾帧大小: {}", m_packetSizes.size(), m_defaultPacketSize);
}

void BinFileReader::readBinFile(const QString& filePath)
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_state == Reading) {
            LoggerManager::getInstance()->log("debug", Level::WARN, "readBinFile 被调用但当前正在读取中, 忽略本次请求");
            return;
        }
        m_state = Reading;
    }

    LoggerManager::getInstance()->log("debug", Level::INFO, "readBinFile 开始读取文件: {}", filePath.toStdString());
    emit sigReadProgress(0);

    QFuture<void> future = QtConcurrent::run([this, filePath]() {
        QFile file(filePath);
        if (!file.exists()) {
            LoggerManager::getInstance()->log("debug", Level::ERROR, "文件不存在: {}", filePath.toStdString());
            QMutexLocker locker(&m_mutex);
            m_state = ReadFailed;
            QMetaObject::invokeMethod(this, [this]() {
                emit sigReadFinished(false, QStringLiteral("文件不存在"));
            }, Qt::QueuedConnection);
            return;
        }

        if (!file.open(QIODevice::ReadOnly)) {
            LoggerManager::getInstance()->log("debug", Level::ERROR, "文件打开失败: {}", filePath.toStdString());
            QMutexLocker locker(&m_mutex);
            m_state = ReadFailed;
            QMetaObject::invokeMethod(this, [this]() {
                emit sigReadFinished(false, QStringLiteral("文件打开失败"));
            }, Qt::QueuedConnection);
            return;
        }

        qint64 totalSize = file.size();
        LoggerManager::getInstance()->log("debug", Level::INFO, "文件大小: {} 字节", totalSize);
        if (totalSize == 0) {
            LoggerManager::getInstance()->log("debug", Level::ERROR, "文件为空: {}", filePath.toStdString());
            file.close();
            QMutexLocker locker(&m_mutex);
            m_state = ReadFailed;
            QMetaObject::invokeMethod(this, [this]() {
                emit sigReadFinished(false, QStringLiteral("文件为空"));
            }, Qt::QueuedConnection);
            return;
        }

        const qint64 chunkSize = 4096;
        qint64 bytesRead = 0;
        QByteArray buffer;
        buffer.reserve(static_cast<int>(totalSize));

        while (!file.atEnd()) {
            QByteArray chunk = file.read(chunkSize);
            if (chunk.isEmpty()) {
                break;
            }
            buffer.append(chunk);
            bytesRead += chunk.size();

            int percent = static_cast<int>((bytesRead * 100) / totalSize);
            QMetaObject::invokeMethod(this, [this, percent]() {
                emit sigReadProgress(percent);
            }, Qt::QueuedConnection);
        }

        file.close();
        LoggerManager::getInstance()->log("debug", Level::INFO, "文件读取完毕, 共读取 {} 字节", bytesRead);

        {
            QMutexLocker locker(&m_mutex);
            m_rawData = buffer;
        }

        splitData();

        {
            QMutexLocker locker(&m_mutex);
            m_state = ReadComplete;
            LoggerManager::getInstance()->log("debug", Level::INFO, "状态变更为 ReadComplete, 分包数: {}", m_packets.size());
        }

        QMetaObject::invokeMethod(this, [this]() {
            emit sigReadProgress(100);
            emit sigReadFinished(true, QString());
        }, Qt::QueuedConnection);
    });

    Q_UNUSED(future)
}

BinFileReader::ReadState BinFileReader::readState() const
{
    QMutexLocker locker(&m_mutex);
    return m_state;
}

void BinFileReader::clean()
{
    QMutexLocker locker(&m_mutex);
    if (m_state == NotRead) {
        LoggerManager::getInstance()->log("debug", Level::DEBUG, "clean 已处于 NotRead 状态，跳过清理");
        return;
    }
    LoggerManager::getInstance()->log("debug", Level::INFO, "clean 清理读取器, 释放原始数据 {} 字节, 分包数 {}", m_rawData.size(), m_packets.size());
    m_state = NotRead;
    m_rawData.clear();
    m_rawData.squeeze();
    m_packets.clear();
    m_packets.squeeze();
}

int BinFileReader::packetCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_packets.size();
}

QByteArray BinFileReader::packetAt(int index) const
{
    if (index < 0 || index >= m_packets.size()) {
        LoggerManager::getInstance()->log("debug", Level::WARN, "packetAt 索引越界: index={}, packetCount={}", index, m_packets.size());
        return QByteArray();
    }
    LoggerManager::getInstance()->log("debug", Level::DEBUG, "packetAt index={}, 帧大小={} 字节", index, m_packets.at(index).size());
    return m_packets.at(index);
}

const QByteArray& BinFileReader::getAllBytes() const
{
    QMutexLocker locker(&m_mutex);
    if (m_rawData.isEmpty()) {
        LoggerManager::getInstance()->log("debug", Level::DEBUG, "getAllBytes 原始数据为空");
        static QByteArray emptyData;
        return emptyData;
    }
    LoggerManager::getInstance()->log("debug", Level::DEBUG, "getAllBytes 返回原始数据引用 {} 字节", m_rawData.size());
    return m_rawData;
}

bool BinFileReader::isLastPacket(int index) const
{
    if (index < 0 || index >= m_packets.size()) {
        LoggerManager::getInstance()->log("debug", Level::WARN, "isLastPacket 索引无效: index={}, packetCount={}", index, m_packets.size());
        return false;
    }
    bool isLast = (index == m_packets.size() - 1);
    LoggerManager::getInstance()->log("debug", Level::DEBUG, "isLastPacket index={}, packetCount={}, isLast={}", index, m_packets.size(), isLast);
    return isLast;
}

void BinFileReader::splitData()
{
    QMutexLocker locker(&m_mutex);
    m_packets.clear();

    if (m_rawData.isEmpty()) {
        LoggerManager::getInstance()->log("debug", Level::WARN, "splitData 原始数据为空, 跳过分包");
        return;
    }

    int offset = 0;
    int totalSize = m_rawData.size();
    LoggerManager::getInstance()->log("debug", Level::INFO, "splitData 开始分包, 数据总大小: {} 字节, 自定义规则数: {}, 默认帧大小: {}", totalSize, m_packetSizes.size(), m_defaultPacketSize);

    if (m_packetSizes.isEmpty()) {
        while (offset < totalSize) {
            int len = qMin(m_defaultPacketSize, totalSize - offset);
            m_packets.append(m_rawData.mid(offset, len));
            offset += len;
        }
    } else {
        for (int i = 0; offset < totalSize; ++i) {
            int size;
            if (i < m_packetSizes.size()) {
                size = m_packetSizes.at(i);
            } else {
                size = m_packetSizes.last();
            }

            if (size <= 0) {
                size = 256;
            }

            int len = qMin(size, totalSize - offset);
            m_packets.append(m_rawData.mid(offset, len));
            offset += len;
        }
    }

    LoggerManager::getInstance()->log("debug", Level::INFO, "splitData 分包完成, 总帧数: {}", m_packets.size());
}
