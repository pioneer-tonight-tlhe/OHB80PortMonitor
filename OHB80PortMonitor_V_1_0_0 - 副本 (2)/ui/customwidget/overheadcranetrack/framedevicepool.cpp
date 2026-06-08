#include "framedevicepool.h"
#include <QDebug>
#include <QWidget>

FrameDevicePool::FrameDevicePool(QWidget* parent)
    : QObject(parent)
    , m_parentWidget(parent)
    , m_currentLevel(SetLevel)
{
}

FrameDevicePool::~FrameDevicePool()
{
    clearAll();
}

QSharedPointer<FrameDevice> FrameDevicePool::acquire(MapLevel level, int deviceKey, FrameDevice::DeviceType deviceType)
{
    // 获取该层级的控件映射表
    QMap<int, QSharedPointer<FrameDevice>>& deviceMap = m_deviceMapByLevel[level];
    
    // 检查是否已存在该控件
    if (deviceMap.contains(deviceKey)) {
        QSharedPointer<FrameDevice> device = deviceMap[deviceKey];
        // 控件已存在，复用前重置尺寸（避免累积缩放效果）
        // 1. 清除固定尺寸限制
        device->setMinimumSize(0, 0);
        device->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        // 2. 重新初始化UI
        device->initializeUI(deviceType);
        // 3. 调整到合适尺寸
        device->adjustSize();
        return device;
    }
    
    // 控件不存在，创建新控件
    QSharedPointer<FrameDevice> newDevice(new FrameDevice(m_parentWidget));
    newDevice->initializeUI(deviceType);
    
    // 根据当前活动层级决定是否显示
    if (level == m_currentLevel) {
        newDevice->show();
    } else {
        newDevice->hide();
    }
    
    // 存入映射表
    deviceMap[deviceKey] = newDevice;
    
    qDebug() << "[FrameDevicePool] Created new device: level=" << level 
             << "key=" << deviceKey << "type=" << (int)deviceType;
    
    return newDevice;
}

void FrameDevicePool::switchToLevel(MapLevel level)
{
    if (level == m_currentLevel) {
        return;
    }
    
    qDebug() << "[FrameDevicePool] Switching from level" << m_currentLevel << "to" << level;
    
    // 隐藏当前层级的所有控件
    if (m_deviceMapByLevel.contains(m_currentLevel)) {
        QMap<int, QSharedPointer<FrameDevice>>& currentDeviceMap = m_deviceMapByLevel[m_currentLevel];
        for (auto it = currentDeviceMap.begin(); it != currentDeviceMap.end(); ++it) {
            if (it.value()) {
                it.value()->hide();
            }
        }
    }
    
    // 显示目标层级的所有控件
    if (m_deviceMapByLevel.contains(level)) {
        QMap<int, QSharedPointer<FrameDevice>>& targetDeviceMap = m_deviceMapByLevel[level];
        for (auto it = targetDeviceMap.begin(); it != targetDeviceMap.end(); ++it) {
            if (it.value()) {
                it.value()->show();
            }
        }
    }
    
    m_currentLevel = level;
}

void FrameDevicePool::clearLevel(MapLevel level)
{
    if (!m_deviceMapByLevel.contains(level)) {
        return;
    }
    
    QMap<int, QSharedPointer<FrameDevice>>& deviceMap = m_deviceMapByLevel[level];
    
    qDebug() << "[FrameDevicePool] Clearing level" << level << "with" << deviceMap.size() << "devices";
    
    // 隐藏并删除所有控件
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        if (it.value()) {
            it.value()->hide();
        }
    }
    
    deviceMap.clear();
    m_deviceMapByLevel.remove(level);
}

void FrameDevicePool::clearAll()
{
    qDebug() << "[FrameDevicePool] Clearing all levels";
    
    for (auto levelIt = m_deviceMapByLevel.begin(); levelIt != m_deviceMapByLevel.end(); ++levelIt) {
        QMap<int, QSharedPointer<FrameDevice>>& deviceMap = levelIt.value();
        for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
            if (it.value()) {
                it.value()->hide();
            }
        }
        deviceMap.clear();
    }
    
    m_deviceMapByLevel.clear();
}

QMap<int, QSharedPointer<FrameDevice>>& FrameDevicePool::getDeviceMap(MapLevel level)
{
    return m_deviceMapByLevel[level];
}

int FrameDevicePool::getDeviceCount(MapLevel level) const
{
    if (!m_deviceMapByLevel.contains(level)) {
        return 0;
    }
    return m_deviceMapByLevel[level].size();
}
