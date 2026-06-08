#include "chartpage.h"
#include "ui_chartpage.h"

ChartPage::ChartPage(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ChartPage)
{
    ui->setupUi(this);
}

ChartPage::~ChartPage()
{
    delete ui;
}
