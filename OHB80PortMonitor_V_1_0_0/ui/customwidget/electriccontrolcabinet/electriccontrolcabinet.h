#ifndef ELECTRICCONTROLCABINET_H
#define ELECTRICCONTROLCABINET_H

#include <QWidget>

namespace Ui {
class electriccontrolcabinet;
}

class electriccontrolcabinet : public QWidget
{
    Q_OBJECT

public:
    explicit electriccontrolcabinet(QWidget *parent = nullptr);
    ~electriccontrolcabinet();

private:
    Ui::electriccontrolcabinet *ui;
};

#endif // ELECTRICCONTROLCABINET_H
