#include "boot_delay_timer.h"

#include <QTimer>

BootDelayTimer::BootDelayTimer(QObject* parent)
    : QObject(parent)
{
    m_singleShot = new QTimer(this);
    m_singleShot->setSingleShot(true);
    connect(m_singleShot, &QTimer::timeout, this, &BootDelayTimer::onTimeout);

    m_tick = new QTimer(this);
    m_tick->setInterval(1000);
    connect(m_tick, &QTimer::timeout, this, &BootDelayTimer::onTick);
}

void BootDelayTimer::reset(int seconds)
{
    if (seconds < 0) seconds = 0;
    m_remaining = seconds;
}

void BootDelayTimer::startSeconds(int seconds)
{
    stop();
    reset(seconds);
    m_active = true;
    if (m_remaining > 0) {
        emit countdown(m_remaining);
        m_tick->start();
        m_singleShot->start(m_remaining * 1000);
    } else {
        // 立即超时
        onTimeout();
    }
}

void BootDelayTimer::stop()
{
    // 停止“到点触发”的单次定时器与 1Hz 倒计时定时器（若正在运行）
    if (m_singleShot->isActive()) m_singleShot->stop();
    if (m_tick->isActive()) m_tick->stop();
    // 若当前处于活动态：
    //  - 将状态置为非活动；
    //  - 若剩余时间不为 0，显式置零并补发 countdown(0)，确保 UI 端及时归零不残留旧值。
    if (m_active) {
        m_active = false;
        if (m_remaining != 0) {
            m_remaining = 0;
            emit countdown(0); // 主动补发 0，避免界面仍显示最后一次>0的秒数
        }
    }
}

bool BootDelayTimer::isActive() const
{
    return m_active;
}

void BootDelayTimer::onTick()
{
    if (!m_active) return;
    if (m_remaining > 0) {
        --m_remaining;
        emit countdown(m_remaining);
        if (m_remaining <= 0) {
            // 等待 singleshot 触发统一 onTimeout
            if (m_tick->isActive()) m_tick->stop();
        }
    }
}

void BootDelayTimer::onTimeout()
{
    if (!m_active) return;
    m_active = false;
    m_remaining = 0;
    emit countdown(0);
    emit timeout();
}
