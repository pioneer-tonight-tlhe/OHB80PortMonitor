#ifndef FIRMWAREUPDATESETTINGWIDGET_H
#define FIRMWAREUPDATESETTINGWIDGET_H

#include "../settingwidget/settingwidget.h"

class FirmwareUpdateWidget;

// ====================================================================
// FirmwareUpdateSettingWidget —— 固件升级界面容器
//   用 SettingWidget 封装 FirmwareUpdateWidget，提供统一的标题栏
// ====================================================================
class FirmwareUpdateSettingWidget : public SettingWidget
{
    Q_OBJECT
public:
    explicit FirmwareUpdateSettingWidget(QWidget *parent = nullptr);
    ~FirmwareUpdateSettingWidget();

    // 对外暴露内部的 FirmwareUpdateWidget（只读指针）
    FirmwareUpdateWidget *firmwareUpdateWidget() const;

public slots:
    // 设置固件 bin 文件路径（由外部配置控件触发）
    void setFirmwareFilePath(const QString &filePath);

private:
    FirmwareUpdateWidget *m_firmwareUpdateWidget;
};

#endif // FIRMWAREUPDATESETTINGWIDGET_H
