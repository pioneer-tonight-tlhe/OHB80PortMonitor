#ifndef LOGGERMANAGER_H
#define LOGGERMANAGER_H

#include <string>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <atomic>
#include <thread>
#include <chrono>
#include <sstream>
#include <vector>
#include <condition_variable>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#endif

// windows.h 定义了 ERROR 宏，必须在定义枚举前取消
#ifdef ERROR
#undef ERROR
#endif

// 日志级别枚举
enum class Level {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    CRITICAL
};

class LoggerManager
{
private:
    LoggerManager();

public:
    ~LoggerManager();

    // 禁止拷贝和赋值
    LoggerManager(const LoggerManager&) = delete;
    LoggerManager& operator=(const LoggerManager&) = delete;

    // 6.2.1 单例模式
    static LoggerManager& instance() {
        static LoggerManager inst;
        return inst;
    }

    // 6.2.2 设置根目录
    void set_root_dir(const std::string& root_dir);

    // 6.2.3 设置日志回滚参数（默认单文件10MB，保留5个文件）
    void set_rotation(size_t max_file_size = 10 * 1024 * 1024, size_t max_files = 5);

    // 6.2.4 设置日志保留天数（默认7天）
    void set_retention_days(int days);

    // 6.2.5 设置trace日志文件名（默认为trace.log）
    void set_trace_filename(const std::string& name);

    // 6.2.6 启用自动trace日志（启用后，所有的日志都会汇总到该日志中）
    void enable_auto_trace(bool enabled);

    // 6.2.7 启用自动分离warn和error日志
    void enable_warn_error_split(bool enabled);

    // 6.2.8 清理日志系统
    void shutdown();

    // 手动刷新指定文件的 logger（立即写入磁盘）
    void flush(const std::string& file_name);

private:
    // 日志目标结构体（用于批量 logger 查找）
    struct LogTarget {
        std::string path;
        std::string filename;
    };

public:
    // 6.1.1 核心日志方法
    // log(文件名, 日志级别, 日志内容格式, 写入的参数列表)
    // 示例: log("debug", Level::INFO, "hello word {} {}!", "simon", 12);
    template<typename... Args>
    void log(const std::string& file_name, Level level,
             const std::string& fmt, Args&&... args)
    {
        if (is_shutdown_.load(std::memory_order_relaxed)) return;

        // 格式化消息内容
        std::string content = format_message(fmt, std::forward<Args>(args)...);

        // 构建完整日志行: [2026-03-11 15:11:35] [INFO] 内容
        std::string timestamp = get_timestamp();
        const char* level_str = level_to_string(level);
        std::string message = std::string("[") + timestamp + "] [" + level_str + "] " + content;

        // 映射到 spdlog 级别（用于 flush_on 策略）
        spdlog::level::level_enum spd_level = to_spdlog_level(level);

        // 获取当前日期目录
        std::string date_str = get_current_date();

        // 规范化文件名
        std::string normalized = normalize_filename(file_name);

        // 收集需要写入的所有目标 logger（减少锁获取次数）
        std::vector<LogTarget> targets;
        targets.reserve(3);

        // 主日志
        targets.push_back({build_path(date_str, normalized), normalized});

        // 6.2.6 自动trace：汇总到trace日志
        bool trace_on = auto_trace_enabled_.load(std::memory_order_relaxed);
        std::string trace_fn;
        if (trace_on) {
            trace_fn = trace_filename_;
            std::string trace_path = build_path(date_str, trace_fn);
            // 避免重复写入（如果主日志就是trace文件）
            if (trace_path != targets[0].path) {
                targets.push_back({trace_path, trace_fn});
            }
        }

        // 6.2.7 自动分离warn/error
        if (warn_error_split_enabled_.load(std::memory_order_relaxed)) {
            if (level == Level::WARN) {
                std::string wn = insert_suffix(normalized, "_warn");
                targets.push_back({build_path(date_str, wn), wn});
            } else if (level == Level::ERROR) {
                std::string en = insert_suffix(normalized, "_error");
                targets.push_back({build_path(date_str, en), en});
            }
        }

        // 批量获取 loggers 并写入
        auto loggers = get_loggers_batch(targets, date_str);
        for (auto& lg : loggers) {
            if (lg) {
                lg->log(spd_level, message);
            }
        }
    }

private:
    // spdlog级别映射
    static spdlog::level::level_enum to_spdlog_level(Level level) {
        switch (level) {
            case Level::TRACE:    return spdlog::level::trace;
            case Level::DEBUG:    return spdlog::level::debug;
            case Level::INFO:     return spdlog::level::info;
            case Level::WARN:     return spdlog::level::warn;
            case Level::ERROR:    return spdlog::level::err;
            case Level::CRITICAL: return spdlog::level::critical;
            default:              return spdlog::level::info;
        }
    }

    // 级别转大写字符串
    static const char* level_to_string(Level level) {
        switch (level) {
            case Level::TRACE:    return "TRACE";
            case Level::DEBUG:    return "DEBUG";
            case Level::INFO:     return "INFO";
            case Level::WARN:     return "WARN";
            case Level::ERROR:    return "ERROR";
            case Level::CRITICAL: return "CRITICAL";
            default:              return "INFO";
        }
    }

    // 获取当前时间戳字符串
    static std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_val;
#ifdef _WIN32
        localtime_s(&tm_val, &now_t);
#else
        localtime_r(&now_t, &tm_val);
#endif
        char buf[32];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                 tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday,
                 tm_val.tm_hour, tm_val.tm_min, tm_val.tm_sec);
        return std::string(buf);
    }

    // 获取当前日期字符串 yyyy-MM-dd
    std::string get_current_date();

    // 规范化文件名：自动补 .log 后缀
    std::string normalize_filename(const std::string& name);

    // 在 .log 前插入后缀，如 debug.log -> debug_warn.log
    std::string insert_suffix(const std::string& normalized, const std::string& suffix);

    // 构建完整路径：root_dir/date/filename
    std::string build_path(const std::string& date_str, const std::string& filename);

    // 确保目录存在（跨平台）
    void ensure_directory(const std::string& dir);

    // 获取或创建 spdlog async logger（带 rotating_file_sink）
    std::shared_ptr<spdlog::logger> get_or_create_logger(
        const std::string& full_path,
        const std::string& date_str,
        const std::string& filename);

    // 批量获取 loggers（减少锁获取次数）
    std::vector<std::shared_ptr<spdlog::logger>> get_loggers_batch(
        const std::vector<LogTarget>& targets,
        const std::string& date_str);

    // 检测日期变化，清除旧 logger 缓存
    void check_date_change(const std::string& current_date);

    // 清理过期日志目录
    void cleanup_old_logs();

    // 清理线程函数
    void cleanup_thread_func();

    // 格式化消息：支持 {} 占位符
    template<typename... Args>
    std::string format_message(const std::string& fmt, Args&&... args) {
        std::string result = fmt;
        size_t pos = 0;
        [[maybe_unused]] auto replace_one = [&](auto&& arg) {
            size_t ph = result.find("{}", pos);
            if (ph != std::string::npos) {
                std::ostringstream oss;
                oss << arg;
                std::string val = oss.str();
                result.replace(ph, 2, val);
                pos = ph + val.length();
            }
        };
        (replace_one(std::forward<Args>(args)), ...);
        return result;
    }

private:
    std::string root_dir_;                      // 根目录
    size_t max_file_size_;                      // 单文件最大字节数
    size_t max_files_;                          // 最大保留回滚文件数
    int retention_days_;                        // 日志保留天数
    std::string trace_filename_;                // trace日志文件名

    std::atomic<bool> auto_trace_enabled_;      // 是否启用自动trace
    std::atomic<bool> warn_error_split_enabled_;// 是否启用warn/error分离
    std::atomic<bool> is_shutdown_;             // 是否已关闭

    // logger缓存：key=完整文件路径, value=spdlog::logger
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers_;
    mutable std::shared_mutex logger_mtx_;       // 读写锁保护 loggers_

    // 已创建目录缓存，避免重复 mkdir
    std::unordered_set<std::string> created_dirs_;

    // 当前日期缓存
    std::string cached_date_;

    // spdlog 全局线程池（异步）
    std::shared_ptr<spdlog::details::thread_pool> thread_pool_;

    // 清理线程
    std::thread cleanup_thread_;
    std::atomic<bool> cleanup_running_;
    std::mutex cleanup_mtx_;
    std::condition_variable cleanup_cv_;
};

#endif // LOGGERMANAGER_H
