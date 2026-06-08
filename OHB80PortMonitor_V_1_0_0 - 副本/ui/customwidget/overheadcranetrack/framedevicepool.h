#ifndef FRAMEDEVICEPOOL_H
#define FRAMEDEVICEPOOL_H

#include <QObject>
#include <QMap>
#include <QSharedPointer>
#include <QWidget>
#include "framedevice.h"

class CraneMapWidget;

/**
 * @brief FrameDevice 控件池，负责控件的复用管理
 * 
 * 功能：
 * 1. 为不同 MapLevel 维护独立的控件集合
 * 2. 提供控件获取/归还接口
 * 3. 视图切换时显示/隐藏对应层级的控件
 * 4. 避免频繁创建销毁控件，提升性能
 */
class FrameDevicePool : public QObject
{
    Q_OBJECT

public:
    // MapLevel 枚举（与 CraneMapWidget::MapLevel 对应）
    enum MapLevel {
        SetLevel = 0,
        FoupLevel = 1,
        BayLevel = 2
    };

    explicit FrameDevicePool(QWidget* parent = nullptr);
    ~FrameDevicePool();

    /**
     * @brief 获取或创建一个 FrameDevice 控件
     * @param level 地图级别
     * @param deviceKey 设备键（唯一标识）
     * @param deviceType 设备类型
     * @return FrameDevice 智能指针
     */
    QSharedPointer<FrameDevice> acquire(MapLevel level, int deviceKey, FrameDevice::DeviceType deviceType);

    /**
     * @brief 切换到指定 MapLevel，显示该层级的控件，隐藏其他层级
     * @param level 目标地图级别
     */
    void switchToLevel(MapLevel level);

    /**
     * @brief 清空指定层级的所有控件
     * @param level 地图级别
     */
    void clearLevel(MapLevel level);

    /**
     * @brief 清空所有层级的控件
     */
    void clearAll();

    /**
     * @brief 获取指定层级的控件映射表
     * @param level 地图级别
     * @return 控件映射表的引用
     */
    QMap<int, QSharedPointer<FrameDevice>>& getDeviceMap(MapLevel level);

    /**
     * @brief 获取当前活动层级的控件数量
     * @param level 地图级别
     * @return 控件数量
     */
    int getDeviceCount(MapLevel level) const;

private:
    QWidget* m_parentWidget;  // 父控件（用于创建 FrameDevice）
    
    // 为每个 MapLevel 维护独立的控件映射表
    // key: MapLevel, value: QMap<deviceKey, FrameDevice>
    QMap<MapLevel, QMap<int, QSharedPointer<FrameDevice>>> m_deviceMapByLevel;
    
    // 当前活动的 MapLevel
    MapLevel m_currentLevel;
};

#endif // FRAMEDEVICEPOOL_H
