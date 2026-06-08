#ifndef FIRMWAREUPDATECONFIGSETTINGWIDGET_H
#define FIRMWAREUPDATECONFIGSETTINGWIDGET_H

#include "../settingwidget/settingwidget.h"
#include "tasks/set_firmware_config_task.h"
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <functional>

class FirmwareUpdateConfigSettingWidget : public SettingWidget
{
    Q_OBJECT

public:
    explicit FirmwareUpdateConfigSettingWidget(QWidget *parent = nullptr);
    ~FirmwareUpdateConfigSettingWidget();
    
    // 获取配置的 bin 文件路径
    QString binFilePath() const;

signals:
    // bin 文件路径变更时发出，由外部（DebugPage）连接到 FirmwareUpdateSettingWidget
    void binFilePathChanged(const QString &filePath);

private slots:
    void onLoadBinFileBtnClicked();              // 加载 bin 文件按钮点击
    void onPrepareTimeoutSetBtnClicked();        // 设置准备指令超时按钮点击
    void onWaitingTimeSetBtnClicked();           // 设置等待设备就绪时间按钮点击
    void onSendIntervalSetBtnClicked();          // 设置发送间隔按钮点击
    void onTransferTimeoutSetBtnClicked();       // 设置传输响应超时按钮点击
    void onPostTransferWaitTimeSetBtnClicked();  // 设置数据传输后等待时间按钮点击

private:
    void initUI();
    
    // 初始化各个设置项
    void initLoadBinFileItem();              // bin 文件加载项
    void initPrepareTimeoutItem();          // 准备指令超时项
    void initWaitingTimeItem();             // 等待设备就绪项
    void initSendIntervalItem();            // 发送间隔项
    void initTransferTimeoutItem();         // 传输响应超时项
    void initPostTransferWaitTimeItem();     // 数据传输后等待时间项
    
    // 通用任务提交方法
    void submitConfigTask(SettingItemWidget *item,
                         std::function<void(SetFirmwareConfigTask *)> configSetter,
                         const QString &paramName, 
                         int value,
                         std::function<bool(int)> saveConfigCallback = nullptr);

private:
    // 控件指针
    QLineEdit *m_binFileLineEdit;           // bin 文件路径编辑框
    QSpinBox *m_prepareTimeoutSpinBox;      // 准备指令超时设置框
    QSpinBox *m_waitingTimeSpinBox;         // 等待设备就绪时间设置框
    QSpinBox *m_sendIntervalSpinBox;        // 发送间隔设置框
    QSpinBox *m_transferTimeoutSpinBox;     // 传输响应超时设置框
    QSpinBox *m_postTransferWaitTimeSpinBox; // 数据传输后等待时间设置框

    // SettingItemWidget 指针（用于显示状态）
    SettingItemWidget *m_prepareTimeoutItem;
    SettingItemWidget *m_waitingTimeItem;
    SettingItemWidget *m_sendIntervalItem;
    SettingItemWidget *m_transferTimeoutItem;
    SettingItemWidget *m_postTransferWaitTimeItem;
};

#endif // FIRMWAREUPDATECONFIGSETTINGWIDGET_H
