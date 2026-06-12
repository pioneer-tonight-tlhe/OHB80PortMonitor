#include "uidemo6.h"
#include "ui_uidemo6.h"
#include "quiwidget.h"
#include "homepage.h"
#include "alarmpage.h"
#include "app.h"
#include "usermanager.h"
#include "app/shareddata.h"
#include "scheduler/tasks/operation_dispatch_task/operation_dispatch_task.h"
#include "scheduler/tasks/alarm_dispatch_task/alarm_dispatch_task.h"
#include "ui/customwidget/scrollingtiplabel/scrollingtiplabel.h"
#include "ui/customwidget/operationlogwidget/operationlogwidget.h"

UIDemo6::UIDemo6(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UIDemo6)
{
    ui->setupUi(this);
    this->initForm();
    QUIWidget::setFormInCenter(this);
    
    // 初始化双击标题栏最大化功能，默认启用
    this->doubleClickMaximize = true;

    // 连接 OperationDispatchTask / AlarmDispatchTask 到 scrollingTipLabel
    // App::initialize()（含 initScheduler）在 UIDemo6 创建前已完成，任务均已就绪
    this->connectTipLabelTask();
}

UIDemo6::~UIDemo6()
{
    if (m_operationLogWindow) {
        m_operationLogWindow->close();
        delete m_operationLogWindow;
        m_operationLogWindow = nullptr;
    }
    delete ui;
}

void UIDemo6::initForm()
{
    this->max = false;
    this->location = this->geometry();
    this->setProperty("form", true);
    this->setProperty("canMove", true);
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::WindowMinMaxButtonsHint);

    // 设置uidemo6主界面的名称
    ui->labTitle->setText(App::getDisplayName());
    this->setWindowTitle(App::getDisplayName());

    QStringList qss;
    qss.append("QLabel#label{color:#F0F0F0;font:50pt;}");
    qss.append("QWidget#widgetLeft>QAbstractButton{background:none;border-radius:0px;}");
    qss.append("QWidget#widgetMenu>QAbstractButton{border:0px solid #FF0000;border-radius:0px;padding:0px;margin:0px;}");
    this->setStyleSheet(qss.join(""));

    //添加自定义属性,用于切换ico用
    ui->btnHome->setProperty("icoName", "homepage");
    ui->btnSetting->setProperty("icoName", "configpage");
    ui->btnChart->setProperty("icoName", "chartpage");
    ui->btnCommunicate->setProperty("icoName", "communicatepage");
    ui->btnAlarm->setProperty("icoName", "alarmpage");
    ui->btnDebug->setProperty("icoName", "debugpage");

    // ui->btnDebug->hide();
    ui->btnChart->hide();
    ui->btnMenu_Max->hide();

    QList<QToolButton *> btns = ui->widgetLeft->findChildren<QToolButton *>();
    foreach (QToolButton *btn, btns) {
        btn->setMaximumHeight(90);
        btn->setCheckable(true);
        connect(btn, SIGNAL(clicked(bool)), this, SLOT(buttonClick()));
    }

    ui->btnHome->click();

    // 为 widgetTitle 安装事件过滤器以处理双击事件
    ui->widgetTitle->installEventFilter(this);

    // 老 RunningLoggerWidget 已废弃，运行日志由 RunningLoggerTask + OperationLogDBCon 接管；
    // 老 AlarmLoggerWidget 也已废弃，警报生命周期由 AlarmDispatchTask 与 AlarmLogDBCon 接管。
    // UI 端需要实时显示请订阅 OperationLogDBCon::recordInserted / AlarmLogDBCon::recordInserted。

    // 注册控件权限
    registerWidgetPermissions();

    // 启动时全屏显示
    this->showFullScreen();
}

bool UIDemo6::eventFilter(QObject *obj, QEvent *event)
{
    if (doubleClickMaximize && obj == ui->widgetTitle && event->type() == QEvent::MouseButtonDblClick) {
        // 双击标题栏时触发最大化/还原
        emit requestMaximizeOrRestore();
        return true;
    }
    return QDialog::eventFilter(obj, event);
}

void UIDemo6::buttonClick()
{
    QToolButton *b = (QToolButton *)sender();

    QList<QToolButton *> btns = ui->widgetLeft->findChildren<QToolButton *>();
    foreach (QToolButton *btn, btns)
    {
        QString icoName = btn->property("icoName").toString();
        if (btn != b) {
            btn->setChecked(false);
            btn->setIcon(QIcon(QString(":/image/%1.png").arg(icoName)));
        } else {
            btn->setChecked(true);
            btn->setIcon(QIcon(QString(":/image/%1_focus.png").arg(icoName)));
            if (icoName == "homepage")
                ui->stackedWidget->setCurrentIndex(0);
            else if (icoName == "configpage")
                ui->stackedWidget->setCurrentIndex(1);
            else if (icoName == "chartpage")
                ui->stackedWidget->setCurrentIndex(2);
            else if (icoName == "communicatepage")
                ui->stackedWidget->setCurrentIndex(3);
            else if (icoName == "alarmpage")
                ui->stackedWidget->setCurrentIndex(4);
            else if (icoName == "debugpage")
                ui->stackedWidget->setCurrentIndex(5);
        }
    }
}

void UIDemo6::on_btnMenu_Min_clicked()
{
    emit requestMinimize();
}

void UIDemo6::on_btnMenu_Max_clicked()
{
    emit requestMaximizeOrRestore();
}

void UIDemo6::on_btnMenu_Close_clicked()
{
    emit requestClose();
}

void UIDemo6::onScrollingTipLabelClicked()
{
    if (!m_operationLogWindow) {
        m_operationLogWindow = new OperationLogWidget(nullptr);
        m_operationLogWindow->setWindowFlags(Qt::Window);
        m_operationLogWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        m_operationLogWindow->setWindowTitle("Operation Logger");
    }
    m_operationLogWindow->show();
    m_operationLogWindow->raise();
    m_operationLogWindow->activateWindow();
}

void UIDemo6::setDoubleClickMaximize(bool enabled)
{
    this->doubleClickMaximize = enabled;
}

bool UIDemo6::getDoubleClickMaximize() const
{
    return this->doubleClickMaximize;
}

void UIDemo6::connectTipLabelTask()
{
    // 连接 OperationDispatchTask 的操作日志插入信号
    OperationDispatchTask* operationTask = SharedData::getOperationDispatchTask();
    if (operationTask) {
        // 补播：UIDemo6 在 App::initialize() 之后才创建，启动早期由 NetworkStatusTask
        // 等任务派发的运行日志会先于 connect 发出 —— 取出最近一条直接喂给公告栏，
        // 避免初始打开软件时公告栏显示为空。
        const QList<OperationRecord> backlog = operationTask->recentRecords();
        if (!backlog.isEmpty()) {
            ui->scrollingTipLabel->submitOperationLog(backlog.last());
        }

        connect(operationTask, &OperationDispatchTask::operationLogInserted,
                this, [this](const OperationRecord& rec) {
                    ui->scrollingTipLabel->submitOperationLog(rec);
                }, Qt::QueuedConnection);
    }

    // 连接 AlarmDispatchTask 的警报日志插入信号
    AlarmDispatchTask* alarmTask = SharedData::getAlarmDispatchTask();
    if (alarmTask) {
        // 补播：UIDemo6 在 AlarmDispatchTask::start() 之后才创建，启动早期
        // loadActiveFromDb() 从数据库恢复的未解决警报已经 emit alarmPublished 完成，
        // 这里把当前活跃警报快照直接喂给公告栏，避免恢复的警报在公告栏消失。
        const QList<AlarmInfo> activeBacklog = alarmTask->activeAlarms();
        for (const AlarmInfo& info : activeBacklog) {
            if (info.record.isResolved == static_cast<int>(AlarmResolvedStatus::NoNeed)) continue;
            ui->scrollingTipLabel->submitAlarmLog(info.record);
        }

        connect(alarmTask, &AlarmDispatchTask::alarmPublished,
                this, [this](const AlarmInfo& info) {
                    // NoNeed 类型（如 SH85 自检告警）不需要在滚动公告栏显示
                    if (info.record.isResolved == static_cast<int>(AlarmResolvedStatus::NoNeed)) return;
                    ui->scrollingTipLabel->submitAlarmLog(info.record);
                }, Qt::QueuedConnection);

        // 连接 DB 写入完成后的解决信号，使用 (qrCode, alarmType) 复合 key 移除
        connect(alarmTask, &AlarmDispatchTask::alarmResolvePersisted,
                this, [this](const AlarmRecord& record) {
                    ui->scrollingTipLabel->submitAlarmResolved(record);
                }, Qt::QueuedConnection);
    }

    // 点击公告栏打开运行日志窗口
    connect(ui->scrollingTipLabel, &ScrollingTipLabel::clicked,
            this, &UIDemo6::onScrollingTipLabelClicked);

    qDebug() << "[UIDemo6] OperationDispatchTask and AlarmDispatchTask signals connected to scrollingTipLabel";
}

void UIDemo6::registerWidgetPermissions()
{
    UserManager* mgr = UserManager::instance();
    if (!mgr) return;

    // Root (4): btnDebug
    mgr->registerWidget(ui->btnDebug, UserPermission::Engineer);

    // Debug (2): btnCommunicate
    mgr->registerWidget(ui->btnDebug, UserPermission::Debug);

    // Normal (1): btnSetting, btnHome, btnAlarm ,btnCommunicate(普通用户可查看)
    mgr->registerWidget(ui->btnSetting, UserPermission::Normal);
    mgr->registerWidget(ui->btnCommunicate, UserPermission::Normal);
    // 其他按钮默认所有用户可见，可省略注册
    // mgr->registerWidget(ui->btnHome, UserPermission::Normal);
    // mgr->registerWidget(ui->btnAlarm, UserPermission::Normal);
    // mgr->registerWidget(ui->btnChart, UserPermission::Normal);
}
