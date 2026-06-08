#include "vefcmonitorinfo.h"
#include <algorithm>
#include <numeric>

VEFCMonitorInfo::VEFCMonitorInfo()
    : m_dailyFirstData(), m_dailyLastData(), m_dailyLastDataRecorded(false)
{
}

void VEFCMonitorInfo::addVEFCData(const VEFCData& data)
{
    m_dataQueue.enqueue(data);
}

void VEFCMonitorInfo::resetAll()
{
    m_dailyFirstData = VEFCData();
    m_dailyLastData = VEFCData();
    m_dailyLastDataRecorded = false;
    m_dataQueue.clear();
}

VEFCData VEFCMonitorInfo::getMinimumData() const
{
    if (m_dataQueue.isEmpty()) {
        return VEFCData();
    }

    VEFCData minData = m_dataQueue.first();
    for (const auto& data : m_dataQueue) {
        if (data.gasPressure < minData.gasPressure) {
            minData.gasPressure = data.gasPressure;
        }
        if (data.actualFlow < minData.actualFlow) {
            minData.actualFlow = data.actualFlow;
        }
        if (data.sensorPressure < minData.sensorPressure) {
            minData.sensorPressure = data.sensorPressure;
        }
        if (data.sensorTemperature < minData.sensorTemperature) {
            minData.sensorTemperature = data.sensorTemperature;
        }
    }
    return minData;
}

VEFCData VEFCMonitorInfo::getMaximumData() const
{
    if (m_dataQueue.isEmpty()) {
        return VEFCData();
    }

    VEFCData maxData = m_dataQueue.first();
    for (const auto& data : m_dataQueue) {
        if (data.gasPressure > maxData.gasPressure) {
            maxData.gasPressure = data.gasPressure;
        }
        if (data.actualFlow > maxData.actualFlow) {
            maxData.actualFlow = data.actualFlow;
        }
        if (data.sensorPressure > maxData.sensorPressure) {
            maxData.sensorPressure = data.sensorPressure;
        }
        if (data.sensorTemperature > maxData.sensorTemperature) {
            maxData.sensorTemperature = data.sensorTemperature;
        }
    }
    return maxData;
}

VEFCData VEFCMonitorInfo::getAverageData() const
{
    if (m_dataQueue.isEmpty()) {
        return VEFCData();
    }

    VEFCData avgData;
    double gasPressureSum = 0.0;
    double actualFlowSum = 0.0;
    double sensorPressureSum = 0.0;
    double sensorTemperatureSum = 0.0;

    for (const auto& data : m_dataQueue) {
        gasPressureSum += data.gasPressure;
        actualFlowSum += data.actualFlow;
        sensorPressureSum += data.sensorPressure;
        sensorTemperatureSum += data.sensorTemperature;
    }

    int count = m_dataQueue.size();
    avgData.gasPressure = gasPressureSum / count;
    avgData.actualFlow = actualFlowSum / count;
    avgData.sensorPressure = sensorPressureSum / count;
    avgData.sensorTemperature = sensorTemperatureSum / count;

    return avgData;
}

VEFCData VEFCMonitorInfo::getMedianData() const
{
    if (m_dataQueue.isEmpty()) {
        return VEFCData();
    }

    QVector<double> gasPressures;
    QVector<double> actualFlows;
    QVector<double> sensorPressures;
    QVector<double> sensorTemperatures;

    for (const auto& data : m_dataQueue) {
        gasPressures.append(data.gasPressure);
        actualFlows.append(data.actualFlow);
        sensorPressures.append(data.sensorPressure);
        sensorTemperatures.append(data.sensorTemperature);
    }

    std::sort(gasPressures.begin(), gasPressures.end());
    std::sort(actualFlows.begin(), actualFlows.end());
    std::sort(sensorPressures.begin(), sensorPressures.end());
    std::sort(sensorTemperatures.begin(), sensorTemperatures.end());

    VEFCData medianData;
    int size = gasPressures.size();
    if (size % 2 == 0) {
        medianData.gasPressure = (gasPressures[size / 2 - 1] + gasPressures[size / 2]) / 2.0;
        medianData.actualFlow = (actualFlows[size / 2 - 1] + actualFlows[size / 2]) / 2.0;
        medianData.sensorPressure = (sensorPressures[size / 2 - 1] + sensorPressures[size / 2]) / 2.0;
        medianData.sensorTemperature = (sensorTemperatures[size / 2 - 1] + sensorTemperatures[size / 2]) / 2.0;
    } else {
        medianData.gasPressure = gasPressures[size / 2];
        medianData.actualFlow = actualFlows[size / 2];
        medianData.sensorPressure = sensorPressures[size / 2];
        medianData.sensorTemperature = sensorTemperatures[size / 2];
    }

    return medianData;
}

VEFCData VEFCMonitorInfo::getStandardDeviationData() const
{
    if (m_dataQueue.isEmpty()) {
        return VEFCData();
    }

    QVector<double> gasPressures = extractFieldValues(0);
    QVector<double> actualFlows = extractFieldValues(1);
    QVector<double> sensorPressures = extractFieldValues(2);
    QVector<double> sensorTemperatures = extractFieldValues(3);

    VEFCData stdDevData;
    stdDevData.gasPressure = calculateStandardDeviation(gasPressures);
    stdDevData.actualFlow = calculateStandardDeviation(actualFlows);
    stdDevData.sensorPressure = calculateStandardDeviation(sensorPressures);
    stdDevData.sensorTemperature = calculateStandardDeviation(sensorTemperatures);

    return stdDevData;
}

QVector<VEFCData> VEFCMonitorInfo::getHourlyAverageData() const
{
    QVector<VEFCData> hourlyData(24, VEFCData());
    QVector<int> hourlyCount(24, 0);

    for (const auto& data : m_dataQueue) {
        int hour = data.timestamp.time().hour();
        if (hour >= 0 && hour < 24) {
            hourlyData[hour].gasPressure += data.gasPressure;
            hourlyData[hour].actualFlow += data.actualFlow;
            hourlyData[hour].sensorPressure += data.sensorPressure;
            hourlyData[hour].sensorTemperature += data.sensorTemperature;
            hourlyCount[hour]++;
        }
    }

    for (int i = 0; i < 24; ++i) {
        if (hourlyCount[i] > 0) {
            hourlyData[i].gasPressure /= hourlyCount[i];
            hourlyData[i].actualFlow /= hourlyCount[i];
            hourlyData[i].sensorPressure /= hourlyCount[i];
            hourlyData[i].sensorTemperature /= hourlyCount[i];
            hourlyData[i].timestamp = QDateTime(QDate::currentDate(), QTime(i, 0, 0));
        }
    }

    return hourlyData;
}

QVector<VEFCData> VEFCMonitorInfo::getWindowAverageData(int windowMinutes) const
{
    if (m_dataQueue.isEmpty() || windowMinutes <= 0) {
        return QVector<VEFCData>();
    }

    QVector<VEFCData> windowData;
    QVector<int> windowCount;

    for (const auto& data : m_dataQueue) {
        int windowIndex = data.timestamp.time().hour() * 60 + data.timestamp.time().minute();
        windowIndex /= windowMinutes;

        if (windowIndex >= windowData.size()) {
            windowData.resize(windowIndex + 1, VEFCData());
            windowCount.resize(windowIndex + 1, 0);
        }

        windowData[windowIndex].gasPressure += data.gasPressure;
        windowData[windowIndex].actualFlow += data.actualFlow;
        windowData[windowIndex].sensorPressure += data.sensorPressure;
        windowData[windowIndex].sensorTemperature += data.sensorTemperature;
        windowCount[windowIndex]++;
    }

    for (int i = 0; i < windowData.size(); ++i) {
        if (windowCount[i] > 0) {
            windowData[i].gasPressure /= windowCount[i];
            windowData[i].actualFlow /= windowCount[i];
            windowData[i].sensorPressure /= windowCount[i];
            windowData[i].sensorTemperature /= windowCount[i];
        }
    }

    return windowData;
}

double VEFCMonitorInfo::calculateStandardDeviation(const QVector<double>& values) const
{
    if (values.isEmpty()) {
        return 0.0;
    }

    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double variance = 0.0;

    for (double value : values) {
        variance += (value - mean) * (value - mean);
    }

    variance /= values.size();
    return std::sqrt(variance);
}

QVector<double> VEFCMonitorInfo::extractFieldValues(int fieldIndex) const
{
    QVector<double> values;

    for (const auto& data : m_dataQueue) {
        switch (fieldIndex) {
        case 0:
            values.append(data.gasPressure);
            break;
        case 1:
            values.append(data.actualFlow);
            break;
        case 2:
            values.append(data.sensorPressure);
            break;
        case 3:
            values.append(data.sensorTemperature);
            break;
        default:
            break;
        }
    }

    return values;
}
