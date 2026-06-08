#ifndef SETOFOHBINFO_H
#define SETOFOHBINFO_H

#include <QVector>
#include "foupofohbinfo.h"

class SetOfOHBInfo
{
public:
    // 默认构造函数
    SetOfOHBInfo();
    
    // 带参数构造函数（size和FoupOfOHBInfo对象QVector）
    SetOfOHBInfo(int size, const QVector<FoupOfOHBInfo>& foups);
    
    // 拷贝构造函数
    SetOfOHBInfo(const SetOfOHBInfo &other);
    
    // 赋值运算符
    SetOfOHBInfo &operator=(const SetOfOHBInfo &other);
    
    // 析构函数
    ~SetOfOHBInfo() = default;
    
    // 根据qrcode获取FoupOfOHBInfo对象方法
    const FoupOfOHBInfo& getFoupByQRCode(const QString& qrCode) const;
    FoupOfOHBInfo& getFoupByQRCode(const QString& qrCode);
    
    // 根据qrcode设置FoupOfOHBInfo对象方法
    bool setFoupByQRCode(const QString& qrCode, const FoupOfOHBInfo& foupInfo);
    
    // 设置整个Foup队列
    void setFoups(const QVector<FoupOfOHBInfo>& foups);
    
    // 获取和设置size
    int getSize() const { return m_size; }
    void setSize(int size);
    
    // 获取Foup列表
    const QVector<FoupOfOHBInfo>& getFoups() const { return m_foups; }
    QVector<FoupOfOHBInfo>& getFoups() { return m_foups; }
    
    // 获取Set ID
    const QString& getSetId() const { return m_setId; }
    
    // 获取进气压力范围（格式：第一个压力~最后一个压力 Mpa）
    QString getInletPressureRange() const;
    
    // 获取进气流量平均值（格式：平均值 L/Min）
    QString getInletFlowAverage() const;
    
    // 获取湿度范围（格式：第一个湿度~最后一个湿度 %）
    QString getRHRange() const;
    
    // 获取和设置 uiId
    int getUiId() const { return m_uiId; }
    void setUiId(int uiId) { m_uiId = uiId; }

private:
    int m_size;                           // 管理的FoupOfOHBInfo对象数量
    QString m_setId;                      // Set ID，默认为第一个到最后一个QR码的范围
    int m_uiId;                           // UI ID，用于界面显示
    QVector<FoupOfOHBInfo> m_foups;       // 管理FoupOfOHBInfo对象的队列
    
    // 初始化Foup列表
    void initializeFoups();
    
    // 更新Set ID
    void updateSetId();
};

#endif // SETOFOHBINFO_H
