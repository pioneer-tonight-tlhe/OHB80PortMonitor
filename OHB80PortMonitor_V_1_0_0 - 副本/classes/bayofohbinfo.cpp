#include "bayofohbinfo.h"

BayOfOHBInfo::BayOfOHBInfo()
    : m_size(4)
{
    initializeSets();
    updateBayId();
}

BayOfOHBInfo::BayOfOHBInfo(int size, const QVector<SetOfOHBInfo>& sets)
    : m_size(size)
    , m_sets(sets)
{
    // 确保提供的sets数量不超过size
    if (m_sets.size() > m_size) {
        m_sets = m_sets.mid(0, m_size);
    }
    // 如果提供的sets数量小于size，用默认对象填充
    while (m_sets.size() < m_size) {
        m_sets.append(SetOfOHBInfo());
    }
    updateBayId();
}

BayOfOHBInfo::BayOfOHBInfo(const BayOfOHBInfo &other)
    : m_size(other.m_size)
    , m_bayId(other.m_bayId)
    , m_sets(other.m_sets)
{
}

BayOfOHBInfo &BayOfOHBInfo::operator=(const BayOfOHBInfo &other)
{
    if (this != &other)
    {
        m_size = other.m_size;
        m_bayId = other.m_bayId;
        m_sets = other.m_sets;
    }
    return *this;
}

const SetOfOHBInfo& BayOfOHBInfo::getSetById(const QString& setId) const
{
    for (int i = 0; i < m_sets.size(); i++)
    {
        if (m_sets[i].getSetId() == setId)
            return m_sets[i];
    }
    // 返回一个默认的空对象
    static SetOfOHBInfo emptySet;
    return emptySet;
}

SetOfOHBInfo& BayOfOHBInfo::getSetById(const QString& setId)
{
    for (int i = 0; i < m_sets.size(); i++)
    {
        if (m_sets[i].getSetId() == setId)
            return m_sets[i];
    }
    // 返回一个默认的空对象
    static SetOfOHBInfo emptySet;
    return emptySet;
}

bool BayOfOHBInfo::setSetById(const QString& setId, const SetOfOHBInfo& setInfo)
{
    for (int i = 0; i < m_sets.size(); i++)
    {
        if (m_sets[i].getSetId() == setId || m_sets[i].getSetId().isEmpty())
        {
            m_sets[i] = setInfo;
            updateBayId(); // 更新Bay ID
            return true;
        }
    }
    return false;
}

void BayOfOHBInfo::setSize(int size)
{
    if (size <= 0) {
        return;
    }
    
    m_size = size;
    
    // 如果新的size小于当前sets数量，截断
    if (m_sets.size() > m_size) {
        m_sets = m_sets.mid(0, m_size);
    }
    // 如果新的size大于当前sets数量，用默认对象填充
    while (m_sets.size() < m_size) {
        m_sets.append(SetOfOHBInfo());
    }
    
    updateBayId(); // 更新Bay ID
}

void BayOfOHBInfo::initializeSets()
{
    m_sets.clear();
    m_sets.reserve(m_size);
    
    for (int i = 0; i < m_size; ++i)
    {
        m_sets.append(SetOfOHBInfo());
    }
}

void BayOfOHBInfo::updateBayId()
{
    if (m_sets.isEmpty()) {
        m_bayId = "";
        return;
    }
    
    // 查找第一个有效的Set ID
    QString firstSetId;
    for (const auto& set : m_sets) {
        if (!set.getSetId().isEmpty()) {
            firstSetId = set.getSetId();
            break;
        }
    }
    
    // 查找最后一个有效的Set ID
    QString lastSetId;
    for (int i = m_sets.size() - 1; i >= 0; --i) {
        if (!m_sets[i].getSetId().isEmpty()) {
            lastSetId = m_sets[i].getSetId();
            break;
        }
    }
    
    // 设置Bay ID格式：第一个Set ID ~ 最后一个Set ID
    if (!firstSetId.isEmpty() && !lastSetId.isEmpty()) {
        m_bayId = firstSetId + " ~ " + lastSetId;
    } else if (!firstSetId.isEmpty()) {
        m_bayId = firstSetId;
    } else {
        m_bayId = "";
    }
}
