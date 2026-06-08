#ifndef BAYOFOHBINFO_H
#define BAYOFOHBINFO_H

#include <QVector>
#include "setofohbinfo.h"

class BayOfOHBInfo
{
public:
    // 默认构造函数
    BayOfOHBInfo();
    
    // 带参数构造函数（size和SetOfOHBInfo对象QVector）
    BayOfOHBInfo(int size, const QVector<SetOfOHBInfo>& sets);
    
    // 拷贝构造函数
    BayOfOHBInfo(const BayOfOHBInfo &other);
    
    // 赋值运算符
    BayOfOHBInfo &operator=(const BayOfOHBInfo &other);
    
    // 析构函数
    ~BayOfOHBInfo() = default;
    
    // 根据setId获取SetOfOHBInfo对象方法
    const SetOfOHBInfo& getSetById(const QString& setId) const;
    SetOfOHBInfo& getSetById(const QString& setId);
    
    // 根据setId设置SetOfOHBInfo对象方法
    bool setSetById(const QString& setId, const SetOfOHBInfo& setInfo);
    
    // 获取和设置size
    int getSize() const { return m_size; }
    void setSize(int size);
    
    // 获取Set列表
    const QVector<SetOfOHBInfo>& getSets() const { return m_sets; }
    QVector<SetOfOHBInfo>& getSets() { return m_sets; }
    
    // 获取Bay ID
    const QString& getBayId() const { return m_bayId; }

private:
    int m_size;                           // 管理的SetOfOHBInfo对象数量
    QString m_bayId;                      // Bay ID，格式为第一个Set ID ~ 最后一个Set ID
    QVector<SetOfOHBInfo> m_sets;         // 管理SetOfOHBInfo对象的队列
    
    // 初始化Set列表
    void initializeSets();
    
    // 更新Bay ID
    void updateBayId();
};

#endif // BAYOFOHBINFO_H
