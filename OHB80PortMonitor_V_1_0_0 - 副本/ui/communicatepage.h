#ifndef COMMUNICATEPAGE_H
#define COMMUNICATEPAGE_H

#include <QWidget>
#include "modbustcpmastermanager/modbuscommand/modbuscommand.h"
#include "usermanager.h"

namespace Ui {
class CommunicatePage;
}

class CommunicatePage : public QWidget
{
    Q_OBJECT

public:
    explicit CommunicatePage(QWidget *parent = nullptr);
    ~CommunicatePage();

    void initCommLoggerWidget();

private slots:
    // 处理通讯完成信号，写入日志表
    void onCommunicationCompleted(ModbusCommand cmd, QString masterId, QString description);

    // 处理用户权限变化，更新 live log 列可见性
    void onPermissionChanged(UserPermission permission);

private:
    Ui::CommunicatePage *ui;
};

#endif // COMMUNICATEPAGE_H
