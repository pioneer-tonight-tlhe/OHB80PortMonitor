#ifndef DEFER_H
#define DEFER_H

#include <functional>

namespace Tool {

/**
 * @brief Defer 类 - RAII 模式，在作用域结束时自动执行回调函数
 * 
 * 使用场景：当函数需要执行 return 语句时，确保某些收尾操作一定会执行
 * 
 * 示例：
 *   void func() {
 *       Defer defer([]{
 *           cleanup();  // 函数退出时自动执行
 *       });
 *       
 *       if (condition) {
 *           return;  // cleanup() 会被自动调用
 *       }
 *       
 *       // 正常返回时，cleanup() 也会被调用
 *   }
 */
class Defer
{
public:
    /**
     * @brief 构造函数，接收回调函数
     * @param callback 收尾回调函数
     */
    explicit Defer(std::function<void()> callback);

    /**
     * @brief 析构函数，自动执行回调函数
     */
    ~Defer();

    /**
     * @brief 禁用拷贝构造
     */
    Defer(const Defer&) = delete;

    /**
     * @brief 禁用拷贝赋值
     */
    Defer& operator=(const Defer&) = delete;

    /**
     * @brief 启用移动构造
     */
    Defer(Defer&& other) noexcept;

    /**
     * @brief 启用移动赋值
     */
    Defer& operator=(Defer&& other) noexcept;

    /**
     * @brief 取消执行回调函数
     */
    void cancel();

    /**
     * @brief 立即执行回调函数并取消自动执行
     */
    void invoke();

private:
    std::function<void()> m_callback;
    bool m_cancelled;
};

} // namespace Tool

#endif // DEFER_H
