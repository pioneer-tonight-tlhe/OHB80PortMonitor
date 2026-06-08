#ifndef ALARMPAGE_H
#define ALARMPAGE_H

#include <QWidget>

namespace Ui {
class AlarmPage;
}

class AlarmPage : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmPage(QWidget *parent = nullptr);
    ~AlarmPage();

private:
    Ui::AlarmPage *ui;
};

#endif // ALARMPAGE_H
