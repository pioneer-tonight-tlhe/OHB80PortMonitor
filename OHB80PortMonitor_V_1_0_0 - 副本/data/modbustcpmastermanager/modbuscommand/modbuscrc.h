#ifndef MODBUSCRC_H
#define MODBUSCRC_H

#include <QByteArray>
#include <QString>

// ============================================================
// ModbusCrc - Modbus CRC 校验工具类
//
// 所有需要 CRC 校验的模块统一通过本类进行调用
// ============================================================
class ModbusCrc
{
public:
    /**
     * @brief 计算 Modbus CRC16 校验码
     * @param data 待校验数据
     * @return CRC16 校验码（quint16）
     */
    static quint16 crc16(const QByteArray &data);

    /**
     * @brief 计算 Modbus CRC16 校验码（返回字节数组，低字节在前）
     * @param data 待校验数据
     * @return 2字节 CRC 值（低字节在前，高字节在后）
     */
    static QByteArray modbusCRC16(const QByteArray &data);

    /**
     * @brief 根据方法名称计算 CRC
     * @param method 校验方法名（如 "ModbusCRC16"）
     * @param data 待校验数据
     * @param crcBytes CRC 字节数
     * @return CRC 字节数据
     */
    static QByteArray calculate(const QString &method,
                                const QByteArray &data,
                                int crcBytes);

    /**
     * @brief 验证 CRC 是否正确
     * @param frame 完整帧数据
     * @param method 校验方法名
     * @param startOffset CRC 计算起始偏移（0-indexed）
     * @param crcBytes CRC 占用字节数
     * @return CRC 校验通过返回 true
     */
    static bool validate(const QByteArray &frame,
                         const QString &method,
                         int startOffset,
                         int crcBytes);
};

#endif // MODBUSCRC_H
