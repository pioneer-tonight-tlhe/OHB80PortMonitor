#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include "devicemonitorwidget.h"
#include <QWidget>

namespace Ui {
class HomePage;
}

class HomePage : public QWidget
{
    Q_OBJECT

public:
    explicit HomePage(QWidget *parent = nullptr);
    ~HomePage();

private:
    Ui::HomePage *ui;
    
    // 初始化 UI 样式
    void initUI();

    // 初始化设备监控器
    void initDeviceMonitorWidget();

    // 初始化天车地图控件
    void initCraneMapWidget();

    // 初始化 overheadCranesWidget（天车提示控件）
    void initOverheadCranesWidget();

private:
    // 设备监控器
    Graph::DeviceMonitorWidget* m_deviceMonitor;
};

#endif // HOMEPAGE_H
