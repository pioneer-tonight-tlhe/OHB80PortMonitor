#ifndef CHARTPAGE_H
#define CHARTPAGE_H

#include <QWidget>

namespace Ui {
class ChartPage;
}

class ChartPage : public QWidget
{
    Q_OBJECT

public:
    explicit ChartPage(QWidget *parent = nullptr);
    ~ChartPage();

private:
    Ui::ChartPage *ui;
};

#endif // CHARTPAGE_H
