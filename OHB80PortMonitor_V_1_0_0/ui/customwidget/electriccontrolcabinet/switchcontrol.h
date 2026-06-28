#ifndef SWITCHCONTROL_H
#define SWITCHCONTROL_H

#include <QWidget>

namespace Ui {
class SwitchControl;
}

class SwitchControl : public QWidget
{
    Q_OBJECT

public:
    explicit SwitchControl(QWidget *parent = nullptr);
    ~SwitchControl();

private:
    Ui::SwitchControl *ui;
};

#endif // SWITCHCONTROL_H
