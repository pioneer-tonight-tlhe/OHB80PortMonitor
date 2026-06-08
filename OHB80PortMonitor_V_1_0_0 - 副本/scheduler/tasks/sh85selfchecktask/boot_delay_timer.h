#ifndef BOOT_DELAY_TIMER_H
#define BOOT_DELAY_TIMER_H

#include <QObject>

class QTimer;

// 一个独立可复用的“启动延时倒计时”控制器
// - startSeconds(s): 启动 s 秒延时，并以 1Hz 发出 countdown(remaining)
// - stop(): 停止倒计时并发出 countdown(0)
// - timeout(): 倒计时结束信号
class BootDelayTimer : public QObject {
    Q_OBJECT
public:
    explicit BootDelayTimer(QObject* parent = nullptr);

    void startSeconds(int seconds);
    void stop();
    bool isActive() const;
    int  remainingSeconds() const { return m_remaining; }

signals:
    void countdown(int remainingSeconds);
    void timeout();

private slots:
    void onTick();
    void onTimeout();

private:
    void reset(int seconds);

    QTimer* m_singleShot = nullptr; // 结束定时器
    QTimer* m_tick       = nullptr; // 1Hz 倒计时
    int     m_remaining  = 0;       // 启动延时的剩余秒数（>=0）
    bool    m_active     = false;   // 是否处于启动延时进行中
};

#endif // BOOT_DELAY_TIMER_H
