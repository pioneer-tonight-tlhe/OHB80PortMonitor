#ifndef VEFCMONITORINFO_H
#define VEFCMONITORINFO_H

#include <QString>
#include <QDateTime>
#include <QQueue>
#include <QVector>
#include <cmath>

// VEFC 数据结构体
struct VEFCData
{
    double gasPressure = 0.0;           // 气体压力 (Kpa)
    double actualFlow = 0.0;            // 实际流量 (L/Min)
    double sensorPressure = 0.0;        // 传感器压力 (Kpa)
    double sensorTemperature = 0.0;     // 传感器温度 (℃)
    QDateTime timestamp;                // 时间 (年月日-时:分:秒.毫秒)

    VEFCData() = default;
    VEFCData(double gp, double af, double sp, double st, const QDateTime& ts = QDateTime::currentDateTime())
        : gasPressure(gp), actualFlow(af), sensorPressure(sp), sensorTemperature(st), timestamp(ts) {}
};

// VEFC 监控类
class VEFCMonitorInfo
{
public:
    VEFCMonitorInfo();
    ~VEFCMonitorInfo() = default;

    // ========== 数据记录 ==========
    // 添加 VEFC 数据到队列
    void addVEFCData(const VEFCData& data);

    // 获取今天第一条数据
    const VEFCData& getDailyFirstData() const { return m_dailyFirstData; }
    void setDailyFirstData(const VEFCData& data) { m_dailyFirstData = data; }

    // 获取每天最后一条记录
    const VEFCData& getDailyLastData() const { return m_dailyLastData; }
    void setDailyLastData(const VEFCData& data) { m_dailyLastData = data; }

    // 获取每天最后一条记录标志位
    bool isDailyLastDataRecorded() const { return m_dailyLastDataRecorded; }
    void setDailyLastDataRecorded(bool recorded) { m_dailyLastDataRecorded = recorded; }

    // 获取队列中的所有数据
    const QQueue<VEFCData>& getDataQueue() const { return m_dataQueue; }

    // ========== 重置方法 ==========
    // 重置所有数据（清空队列、重置标志位、清空开机值和最后一条记录）
    void resetAll();

    // ========== 基础统计 ==========
    // 获取今天 VEFC 最小值
    VEFCData getMinimumData() const;

    // 获取今天 VEFC 最大值
    VEFCData getMaximumData() const;

    // 获取今天 VEFC 平均值
    VEFCData getAverageData() const;

    // 获取今天 VEFC 中位数
    VEFCData getMedianData() const;

    // 获取今天 VEFC 标准差
    VEFCData getStandardDeviationData() const;

    // ========== 分时段统计 ==========
    // 获取每小时 VEFC 平均值（返回 24 个小时的数据）
    QVector<VEFCData> getHourlyAverageData() const;

    // 获取固定窗口内的 VEFC 平均值（窗口大小：分钟）
    QVector<VEFCData> getWindowAverageData(int windowMinutes) const;

    // 获取队列大小
    int getQueueSize() const { return m_dataQueue.size(); }

private:
    VEFCData m_dailyFirstData;                      // 今天第一条数据
    VEFCData m_dailyLastData;                       // 每天 23:59:59 的值
    bool m_dailyLastDataRecorded = false;           // 今天是否记录了最后一条数据

    QQueue<VEFCData> m_dataQueue;                   // 存放这一天的 VEFC 数据

    // 辅助方法
    double calculateStandardDeviation(const QVector<double>& values) const;
    QVector<double> extractFieldValues(int fieldIndex) const;
};

#endif // VEFCMONITORINFO_H
