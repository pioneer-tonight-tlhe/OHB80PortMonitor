#ifndef FRAMEDEVICE_H
#define FRAMEDEVICE_H

#include <QWidget>
#include <QColor>
#include <QMap>
#include <QSharedPointer>
#include "setofohbinfo.h"
#include "foupofohbinfo.h"
#include "bayofohbinfo.h"
#include <QFont>
#include <QMouseEvent>

namespace Ui {
class FrameDevice;
}

class FrameDevice : public QWidget
{
    Q_OBJECT

public:
    // FrameDevice 类型枚举
    enum class DeviceType {
        Foup,
        Set,
        Bay
    };

    // FrameDevice 状态枚举
    enum class DeviceStatus {
        Alarm,
        FoupOut,
        FoupIn,
        PurgeTime5Min,      // 充气大于等于5分钟
        PurgeTime10Min,     // 充气大于10分钟
        PurgeTime20Min,     // 充气大于20分钟
        PurgeTime30Min,     // 充气大于30分钟
        Disable             // 禁用状态（灰色）
    };


    // 状态颜色映射表
    static const QMap<DeviceStatus, QColor> StatusColorMap;

    explicit FrameDevice(QWidget *parent = nullptr);
    ~FrameDevice();

    // 初始化UI，根据设备类型设置labType文本
    void initializeUI(DeviceType type);

    // 设置设备状态
    void setDeviceStatus(DeviceStatus status);

    // 根据设备状态获取背景颜色（仅针对 Foup 类型）
    static QColor getBackgroundColor(DeviceStatus status);

    // 根据 FoupOfOHBInfo 计算设备状态（仅针对 Foup 类型）
    static DeviceStatus calculateStatusByPurgeTime(QSharedPointer<FoupOfOHBInfo> foupInfo);

    // 设置 labIDValue 的文本
    void setLabIDValueText(const QString& text);
    
    // 设置 labInletPressureValue 的值并添加单位 Mpa
    void setLabInletPressureValue(const QString& text);
    void setLabInletPressureValue(float value);
    
    // 设置 labInletFlowValue 的值并添加单位 L/Min
    void setLabInletFlowValue(const QString& text);
    void setLabInletFlowValue(float value);
    
    // 设置 labRHValue 的值并添加单位 %
    void setLabRHValue(const QString& text);
    void setLabRHValue(float value);
    
    // 根据缩放等级调整字体大小
    void setZoomLevel(int level);
    
    // 设置和获取智能指针
    void setSetOfOHBInfo(QSharedPointer<SetOfOHBInfo> setInfo);
    void setFoupOfOHBInfo(QSharedPointer<FoupOfOHBInfo> foupInfo);
    void setBayOfOHBInfo(QSharedPointer<BayOfOHBInfo> bayInfo);
    
    QSharedPointer<SetOfOHBInfo> getSetOfOHBInfo() const { return m_setInfo; }
    QSharedPointer<FoupOfOHBInfo> getFoupOfOHBInfo() const { return m_foupInfo; }
    QSharedPointer<BayOfOHBInfo> getBayOfOHBInfo() const { return m_bayInfo; }
    
    // 更新数据显示方法
    void updateFoupInfo();  // 从绑定的 FoupOfOHBInfo 更新 UI 显示
    void updateSetInfo();   // 从绑定的 SetOfOHBInfo 更新 UI 显示
    
    // 获取设备类型
    DeviceType getDeviceType() const { return m_deviceType; }
    
    // 选中状态（显示高亮边框）
    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }

signals:
    // 双击 Set 设备时发射，携带 uiId 和第一个 Foup 的 qrCode
    void setDeviceDoubleClicked(int uiId, const QString& firstFoupQrCode);
    // 设备被单击时发射，携带设备类型和 uiId
    void deviceClicked(FrameDevice::DeviceType deviceType, int uiId);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    Ui::FrameDevice *ui;
    QString m_bgStyleSheet;   // 背景色样式
    int m_fontSize;           // 当前字体大小
    void updateStyleSheet();
    
    // 设备类型
    DeviceType m_deviceType = DeviceType::Foup;
    bool m_selected = false;
    
    // 智能指针成员变量
    QSharedPointer<SetOfOHBInfo> m_setInfo;
    QSharedPointer<FoupOfOHBInfo> m_foupInfo;
    QSharedPointer<BayOfOHBInfo> m_bayInfo;
};

#endif // FRAMEDEVICE_H
