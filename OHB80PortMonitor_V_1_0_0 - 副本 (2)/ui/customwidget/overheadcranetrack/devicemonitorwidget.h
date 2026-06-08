#ifndef DEVICEMONITORWIDGET_H
#define DEVICEMONITORWIDGET_H

#include "tablewidgetmanager.h"
#include "framedevice.h"
#include "app/app.h"

#include <QWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QSharedPointer>

class CraneMapWidget;

namespace Graph {

class DeviceMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceMonitorWidget(QWidget* parent = nullptr);
    ~DeviceMonitorWidget();

    // 连接 CraneMapWidget 的设备选中信号
    void bindCraneMapWidget(CraneMapWidget* widget);

    // 设置定时刷新间隔（毫秒）
    void setRefreshInterval(int ms);

    // 启动/停止定时刷新
    void startRefresh();
    void stopRefresh();

private slots:
    // 处理设备选中事件
    void onDeviceSelected(FrameDevice::DeviceType deviceType,
                          QSharedPointer<SetOfOHBInfo> setInfo,
                          QSharedPointer<FoupOfOHBInfo> foupInfo);

    // 定时刷新表格数据
    void refreshData();

private:
    // 初始化默认的三个表格
    void initTables();

    // 根据 m_monitoredFoupInfos 刷新 FoupMonitor 和 FoupPurgeTimeMonitor
    void refreshFoupMonitor();

    // 根据 m_monitoredFoupInfos 刷新 SetMonitor
    void refreshSetMonitor();

    TableWidgetManager m_tableManager;
    QVBoxLayout* m_layout;
    QTimer* m_refreshTimer;

    // 当前选中的设备类型
    FrameDevice::DeviceType m_selectedDeviceType;

    // 直接存储全局变量中 FoupOfOHBInfo 的指针，避免每次刷新都查找
    QVector<FoupOfOHBInfo*> m_monitoredFoupInfos;

};

}

#endif // DEVICEMONITORWIDGET_H
