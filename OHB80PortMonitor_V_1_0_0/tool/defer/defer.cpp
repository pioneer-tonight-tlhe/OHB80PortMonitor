#include "defer.h"

namespace Tool {

Defer::Defer(std::function<void()> callback)
    : m_callback(std::move(callback))
    , m_cancelled(false)
{
}

Defer::~Defer()
{
    if (!m_cancelled && m_callback) {
        m_callback();
    }
}

Defer::Defer(Defer&& other) noexcept
    : m_callback(std::move(other.m_callback))
    , m_cancelled(other.m_cancelled)
{
    other.m_cancelled = true;
}

Defer& Defer::operator=(Defer&& other) noexcept
{
    if (this != &other) {
        // 先执行当前对象的回调（如果未取消）
        if (!m_cancelled && m_callback) {
            m_callback();
        }
        
        m_callback = std::move(other.m_callback);
        m_cancelled = other.m_cancelled;
        other.m_cancelled = true;
    }
    return *this;
}

void Defer::cancel()
{
    m_cancelled = true;
}

void Defer::invoke()
{
    if (!m_cancelled && m_callback) {
        m_callback();
        m_cancelled = true;
    }
}

} // namespace Tool
