#include "setofohbinfo.h"
#include <algorithm>

SetOfOHBInfo::SetOfOHBInfo()
    : m_size(4)
    , m_uiId(0)
{
    initializeFoups();
    updateSetId();
}

SetOfOHBInfo::SetOfOHBInfo(int size, const QVector<FoupOfOHBInfo>& foups)
    : m_size(size)
    , m_uiId(0)
    , m_foups(foups)
{
    // 确保提供的foups数量不超过size
    if (m_foups.size() > m_size) {
        m_foups = m_foups.mid(0, m_size);
    }
    // 如果提供的foups数量小于size，用默认对象填充
    while (m_foups.size() < m_size) {
        m_foups.append(FoupOfOHBInfo());
    }
    updateSetId();
}

SetOfOHBInfo::SetOfOHBInfo(const SetOfOHBInfo &other)
    : m_size(other.m_size)
    , m_setId(other.m_setId)
    , m_uiId(other.m_uiId)
    , m_foups(other.m_foups)
{
}

SetOfOHBInfo &SetOfOHBInfo::operator=(const SetOfOHBInfo &other)
{
    if (this != &other)
    {
        m_size = other.m_size;
        m_setId = other.m_setId;
        m_uiId = other.m_uiId;
        m_foups = other.m_foups;
    }
    return *this;
}

const FoupOfOHBInfo& SetOfOHBInfo::getFoupByQRCode(const QString& qrCode) const
{
    for (int i = 0; i < m_foups.size(); i++)
    {
        if (m_foups[i].qrCode() == qrCode)
            return m_foups[i];
    }
    // 返回一个默认的空对象
    static FoupOfOHBInfo emptyFoup;
    return emptyFoup;
}

FoupOfOHBInfo& SetOfOHBInfo::getFoupByQRCode(const QString& qrCode)
{
    for (int i = 0; i < m_foups.size(); i++)
    {
        if (m_foups[i].qrCode() == qrCode)
            return m_foups[i];
    }
    // 返回一个默认的空对象
    static FoupOfOHBInfo emptyFoup;
    return emptyFoup;
}

bool SetOfOHBInfo::setFoupByQRCode(const QString& qrCode, const FoupOfOHBInfo& foupInfo)
{
    for (int i = 0; i < m_foups.size(); i++)
    {
        if (m_foups[i].qrCode() == qrCode || m_foups[i].qrCode().isEmpty())
        {
            m_foups[i] = foupInfo;
            updateSetId(); // 更新Set ID
            return true;
        }
    }
    return false;
}

void SetOfOHBInfo::setSize(int size)
{
    if (size <= 0) {
        return;
    }
    
    m_size = size;
    
    // 如果新的size小于当前foups数量，截断
    if (m_foups.size() > m_size) {
        m_foups = m_foups.mid(0, m_size);
    }
    // 如果新的size大于当前foups数量，用默认对象填充
    while (m_foups.size() < m_size) {
        m_foups.append(FoupOfOHBInfo());
    }
    
    updateSetId(); // 更新Set ID
}

void SetOfOHBInfo::initializeFoups()
{
    m_foups.clear();
    m_foups.reserve(m_size);
    
    for (int i = 0; i < m_size; ++i)
    {
        m_foups.append(FoupOfOHBInfo());
    }
}

void SetOfOHBInfo::updateSetId()
{
    if (m_foups.isEmpty()) {
        m_setId = "";
        return;
    }
    
    // 查找第一个有效的QR码
    QString firstQR;
    for (const auto& foup : m_foups) {
        if (!foup.qrCode().isEmpty()) {
            firstQR = foup.qrCode();
            break;
        }
    }
    
    // 查找最后一个有效的QR码
    QString lastQR;
    for (int i = m_foups.size() - 1; i >= 0; --i) {
        if (!m_foups[i].qrCode().isEmpty()) {
            lastQR = m_foups[i].qrCode();
            break;
        }
    }
    
    // 设置Set ID格式：第一个QR码 ~ 最后一个QR码
    if (!firstQR.isEmpty() && !lastQR.isEmpty()) {
        m_setId = firstQR + "~" + lastQR;
    } else if (!firstQR.isEmpty()) {
        m_setId = firstQR;
    } else {
        m_setId = "";
    }
}

QString SetOfOHBInfo::getInletPressureRange() const
{
    if (m_foups.isEmpty()) {
        return "0.0~0.0Mpa";
    }
    
    auto minIt = std::min_element(m_foups.begin(), m_foups.end(), 
        [](const FoupOfOHBInfo& a, const FoupOfOHBInfo& b) {
            return a.inletPressure() < b.inletPressure();
        });
    
    auto maxIt = std::max_element(m_foups.begin(), m_foups.end(), 
        [](const FoupOfOHBInfo& a, const FoupOfOHBInfo& b) {
            return a.inletPressure() < b.inletPressure();
        });
    
    double minPressure = minIt->inletPressure();
    double maxPressure = maxIt->inletPressure();
    
    return QString("%1~%2Mpa").arg(minPressure, 0, 'f', 2).arg(maxPressure, 0, 'f', 2);
}

QString SetOfOHBInfo::getInletFlowAverage() const
{
    if (m_foups.isEmpty()) {
        return "0.0L/Min";
    }
    
    double totalFlow = 0.0;
    for (const auto& foup : m_foups) {
        totalFlow += foup.inletFlow();
    }
    
    double averageFlow = totalFlow / m_foups.size();
    return QString("%1L/Min").arg(averageFlow, 0, 'f', 2);
}

QString SetOfOHBInfo::getRHRange() const
{
    if (m_foups.isEmpty()) {
        return "0.0~0.0%";
    }
    
    auto minIt = std::min_element(m_foups.begin(), m_foups.end(), 
        [](const FoupOfOHBInfo& a, const FoupOfOHBInfo& b) {
            return a.RH() < b.RH();
        });
    
    auto maxIt = std::max_element(m_foups.begin(), m_foups.end(), 
        [](const FoupOfOHBInfo& a, const FoupOfOHBInfo& b) {
            return a.RH() < b.RH();
        });
    
    double minRH = minIt->RH();
    double maxRH = maxIt->RH();
    
    return QString("%1~%2%").arg(minRH, 0, 'f', 2).arg(maxRH, 0, 'f', 2);
}

void SetOfOHBInfo::setFoups(const QVector<FoupOfOHBInfo>& foups)
{
    m_foups = foups;
    m_size = m_foups.size();
    updateSetId();
}
