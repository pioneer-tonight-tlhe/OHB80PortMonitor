#include "electriccontrolcabinet.h"
#include "ui_electriccontrolcabinet.h"

electriccontrolcabinet::electriccontrolcabinet(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::electriccontrolcabinet)
{
    ui->setupUi(this);
}

electriccontrolcabinet::~electriccontrolcabinet()
{
    delete ui;
}
