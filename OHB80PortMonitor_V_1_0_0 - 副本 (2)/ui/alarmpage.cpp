#include "alarmpage.h"
#include "ui_alarmpage.h"

AlarmPage::AlarmPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AlarmPage)
{
    ui->setupUi(this);

    // 新 AlarmLogWidget 只需 initUi，所有警报生命周期与持久化都交给 AlarmDispatchTask
    // DeviceOffline 告警的提交/解决由 NetworkStatusTask 内部统一处理，本页无需重复订阅。
    ui->alarmLoggerWidget->initUi();
}

AlarmPage::~AlarmPage()
{
    delete ui;
}
