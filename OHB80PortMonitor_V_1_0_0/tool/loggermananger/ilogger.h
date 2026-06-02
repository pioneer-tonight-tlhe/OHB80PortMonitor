#ifndef ILOGGER_H
#define ILOGGER_H

#include <atomic>
#include <string>
#include "loggermanager.h"

/**
 * @brief 日志接口类，对 LoggerManager 的轻量封装
 *
 * 绑定一个日志文件路径，提供无需重复指定路径和级别的简化写入接口。
 * 典型用法：每个模块持有一个 ILogger 实例，各自写入独立的日志文件。
 *
 * 示例：
 *   ILogger logger("network");
 *   logger.info("连接成功，IP={}", "192.168.1.1");
 *   logger.error("连接超时");
 *   logger.flush();
 */
class ILogger
{
public:
    /**
     * @brief 构造函数，绑定日志文件名
     * @param log_file 日志文件名（无需 .log 后缀，自动补全）
     */
    explicit ILogger(const std::string& log_file = "default", bool enable = true)
        : log_file_(log_file)
        , enable_(enable)
    {}

    ILogger(const ILogger&) = delete;
    ILogger& operator=(const ILogger&) = delete;

    ILogger(ILogger&& other) noexcept
        : log_file_(std::move(other.log_file_))
        , enable_(other.enable_.load(std::memory_order_relaxed))
    {}

    ILogger& operator=(ILogger&& other) noexcept
    {
        if (this != &other) {
            log_file_ = std::move(other.log_file_);
            enable_.store(other.enable_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    /**
     * @brief 设置当前日志文件名
     * @param log_file 日志文件名（无需 .log 后缀）
     */
    void set_log_file(const std::string& log_file)
    {
        log_file_ = log_file;
    }

    /**
     * @brief 获取当前日志文件名
     */
    std::string get_log_file() const
    {
        return log_file_;
    }

    /**
     * @brief 设置当前 ILogger 是否启用写入
     * @param enable true=启用，false=禁用
     */
    void set_enable(bool enable)
    {
        enable_.store(enable, std::memory_order_relaxed);
    }

    /**
     * @brief 获取当前 ILogger 是否启用写入
     */
    bool get_enable() const
    {
        return enable_.load(std::memory_order_relaxed);
    }

    /**
     * @brief 写入 DEBUG 级别日志
     * @param fmt 格式化字符串，使用 {} 作为占位符
     * @param args 格式化参数
     */
    template<typename... Args>
    void debug(const std::string& fmt, Args&&... args)
    {
        if (!enable_.load(std::memory_order_relaxed)) return;
        LoggerManager::getInstance()->log(log_file_, Level::DEBUG, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 写入 INFO 级别日志
     * @param fmt 格式化字符串，使用 {} 作为占位符
     * @param args 格式化参数
     */
    template<typename... Args>
    void info(const std::string& fmt, Args&&... args)
    {
        if (!enable_.load(std::memory_order_relaxed)) return;
        LoggerManager::getInstance()->log(log_file_, Level::INFO, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 写入 WARN 级别日志
     * @param fmt 格式化字符串，使用 {} 作为占位符
     * @param args 格式化参数
     */
    template<typename... Args>
    void warn(const std::string& fmt, Args&&... args)
    {
        if (!enable_.load(std::memory_order_relaxed)) return;
        LoggerManager::getInstance()->log(log_file_, Level::WARN, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 写入 ERROR 级别日志
     * @param fmt 格式化字符串，使用 {} 作为占位符
     * @param args 格式化参数
     */
    template<typename... Args>
    void error(const std::string& fmt, Args&&... args)
    {
        if (!enable_.load(std::memory_order_relaxed)) return;
        LoggerManager::getInstance()->log(log_file_, Level::ERROR, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 写入 TRACE 级别日志
     * @param fmt 格式化字符串，使用 {} 作为占位符
     * @param args 格式化参数
     */
    template<typename... Args>
    void trace(const std::string& fmt, Args&&... args)
    {
        if (!enable_.load(std::memory_order_relaxed)) return;
        LoggerManager::getInstance()->log(log_file_, Level::TRACE, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 写入 CRITICAL 级别日志
     * @param fmt 格式化字符串，使用 {} 作为占位符
     * @param args 格式化参数
     */
    template<typename... Args>
    void critical(const std::string& fmt, Args&&... args)
    {
        if (!enable_.load(std::memory_order_relaxed)) return;
        LoggerManager::getInstance()->log(log_file_, Level::CRITICAL, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief 手动刷新当前日志文件，立即将缓冲区写入磁盘
     */
    void flush()
    {
        LoggerManager::getInstance()->flush(log_file_);
    }

private:
    std::string log_file_;  // 绑定的日志文件名
    std::atomic<bool> enable_;  // 当前日志实例是否启用写入
};

#endif // ILOGGER_H
