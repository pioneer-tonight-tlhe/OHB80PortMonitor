#include "alarminfo.h"
#include <QString>


QString AlarmInfo::generateAlarmId() const
{
    // 警报级别：1位数字
    QString levelStr = QString::number(record.alarmLevel);

    // 来源标识：转换为5位字符串
    // 如果是数字，前面补0；如果是字符串，取前5位或补0
    QString sourceIdStr;
    bool isNumeric;
    int sourceIdNum = record.qrCode.toInt(&isNumeric);

    if (isNumeric) {
        // 数字类型，前面补0到5位
        sourceIdStr = QString("%1").arg(sourceIdNum, 5, 10, QChar('0'));
    } else {
        // 字符串类型，取前5位，不足补0
        sourceIdStr = record.qrCode.left(5);
        while (sourceIdStr.length() < 5) {
            sourceIdStr.append('0');
        }
    }

    // 警告类型：4位数字，前面补0
    QString typeStr = QString("%1").arg(record.alarmType, 4, 10, QChar('0'));

    // 拼接为10位警报ID
    return levelStr + sourceIdStr + typeStr;
}

