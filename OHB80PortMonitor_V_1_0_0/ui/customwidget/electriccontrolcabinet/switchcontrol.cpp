#include "switchcontrol.h"
#include "ui_switchcontrol.h"

SwitchControl::SwitchControl(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SwitchControl)
{
    ui->setupUi(this);
}

SwitchControl::~SwitchControl()
{
    delete ui;
}
