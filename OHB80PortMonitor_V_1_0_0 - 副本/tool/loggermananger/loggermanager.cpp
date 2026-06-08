#include "loggermanager.h"

#include <ctime>
#include <cstring>
#include <algorithm>

// 跨平台递归删除目录（替代 std::filesystem::remove_all，兼容 MinGW 8.1）
static bool remove_directory_recursive(const std::string& path)
{
#ifdef _WIN32
    std::string search = path;
    for (auto& c : search) { if (c == '/') c = '\\'; }
    if (search.back() != '\\') search += '\\';
    std::string pattern = search + '*';

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return false;

    do {
        std::string name = ffd.cFileName;
        if (name == "." || name == "..") continue;
        std::string full = search + name;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            remove_directory_recursive(full);
        } else {
            DeleteFileA(full.c_str());
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
    return RemoveDirectoryA(path.c_str()) != 0;
#else
    DIR* dp = opendir(path.c_str());
    if (!dp) return false;
    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string full = path + "/" + name;
        if (entry->d_type == DT_DIR) {
            remove_directory_recursive(full);
        } else {
            unlink(full.c_str());
        }
    }
    closedir(dp);
    return rmdir(path.c_str()) == 0;
#endif
}

LoggerManager::LoggerManager()
    : root_dir_("./logs"),
      max_file_size_(10 * 1024 * 1024),      // 默认10MB
      max_files_(5),                          // 默认保留5个文件
      retention_days_(7),                     // 默认7天
      trace_filename_("trace.log"),           // 默认trace.log
      auto_trace_enabled_(false),
      warn_error_split_enabled_(false),
      is_shutdown_(false),
      cleanup_running_(true)
{
    // 初始化 spdlog 全局异步线程池
    // 队列容量 131072 条，2个 I/O 线程，满足1000并发连接压力
    spdlog::init_thread_pool(131072, 4);
    thread_pool_ = spdlog::thread_pool();

    // 启动日志清理线程
    cleanup_thread_ = std::thread(&LoggerManager::cleanup_thread_func, this);
}

LoggerManager::~LoggerManager()
{
    if (!is_shutdown_.load()) {
        shutdown();
    }
}

// 6.2.2 设置根目录
void LoggerManager::set_root_dir(const std::string& root_dir)
{
    std::unique_lock<std::shared_mutex> lock(logger_mtx_);
    root_dir_ = root_dir;
    ensure_directory(root_dir_);
}

// 6.2.3 设置日志回滚参数
void LoggerManager::set_rotation(size_t max_file_size, size_t max_files)
{
    std::unique_lock<std::shared_mutex> lock(logger_mtx_);
    max_file_size_ = max_file_size;
    max_files_ = max_files;
}

// 6.2.4 设置日志保留天数
void LoggerManager::set_retention_days(int days)
{
    retention_days_ = days;
}

// 6.2.5 设置trace日志文件名
void LoggerManager::set_trace_filename(const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(logger_mtx_);
    trace_filename_ = normalize_filename(name);
}

// 6.2.6 启用自动trace日志
void LoggerManager::enable_auto_trace(bool enabled)
{
    auto_trace_enabled_.store(enabled, std::memory_order_relaxed);
}

// 6.2.7 启用自动分离warn和error日志
void LoggerManager::enable_warn_error_split(bool enabled)
{
    warn_error_split_enabled_.store(enabled, std::memory_order_relaxed);
}

// 6.2.8 清理日志系统
void LoggerManager::shutdown()
{
    if (is_shutdown_.exchange(true)) return;

    // 停止清理线程
    cleanup_running_.store(false);
    cleanup_cv_.notify_all();
    if (cleanup_thread_.joinable()) {
        cleanup_thread_.join();
    }

    // 刷新并释放所有 logger
    {
        std::unique_lock<std::shared_mutex> lock(logger_mtx_);
        for (auto& pair : loggers_) {
            if (pair.second) {
                pair.second->flush();
            }
        }
        loggers_.clear();
    }

    // 关闭 spdlog 全局资源
    spdlog::shutdown();
}

// 手动刷新指定文件的 logger（立即写入磁盘）
void LoggerManager::flush(const std::string& file_name)
{
    if (is_shutdown_.load(std::memory_order_relaxed)) return;

    // 获取当前日期和规范化文件名
    std::string date_str = get_current_date();
    std::string normalized = normalize_filename(file_name);
    std::string full_path = build_path(date_str, normalized);

    // 读锁查找 logger
    std::shared_lock<std::shared_mutex> rlock(logger_mtx_);
    auto it = loggers_.find(full_path);
    if (it != loggers_.end() && it->second) {
        it->second->flush();
    }
}

// 获取当前日期字符串 yyyy-MM-dd
std::string LoggerManager::get_current_date()
{
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_val;
#ifdef _WIN32
    localtime_s(&tm_val, &now_t);
#else
    localtime_r(&now_t, &tm_val);
#endif
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday);
    return std::string(buf);
}

// 规范化文件名：自动补 .log 后缀
std::string LoggerManager::normalize_filename(const std::string& name)
{
    if (name.empty()) return "default.log";
    const std::string ext = ".log";
    if (name.size() >= ext.size() &&
        name.compare(name.size() - ext.size(), ext.size(), ext) == 0) {
        return name;
    }
    return name + ext;
}

// 在 .log 前插入后缀，如 debug.log -> debug_warn.log
std::string LoggerManager::insert_suffix(const std::string& normalized, const std::string& suffix)
{
    const std::string ext = ".log";
    if (normalized.size() >= ext.size() &&
        normalized.compare(normalized.size() - ext.size(), ext.size(), ext) == 0) {
        return normalized.substr(0, normalized.size() - ext.size()) + suffix + ext;
    }
    return normalized + suffix;
}

// 构建完整路径：root_dir/date/filename
std::string LoggerManager::build_path(const std::string& date_str, const std::string& filename)
{
    std::string path = root_dir_;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
#ifdef _WIN32
        path += '\\';
#else
        path += '/';
#endif
    }
    path += date_str;
#ifdef _WIN32
    path += '\\';
#else
    path += '/';
#endif
    path += filename;
    return path;
}

// 确保目录存在（跨平台递归创建）
void LoggerManager::ensure_directory(const std::string& dir)
{
    if (dir.empty()) return;
    if (created_dirs_.count(dir)) return;

#ifdef _WIN32
    // Windows: 逐层创建目录
    std::string path = dir;
    // 统一分隔符
    for (auto& c : path) {
        if (c == '/') c = '\\';
    }
    size_t pos = 0;
    // 跳过盘符 (如 C:\)
    if (path.size() >= 2 && path[1] == ':') {
        pos = 2;
        if (pos < path.size() && path[pos] == '\\') pos++;
    }
    while (pos < path.size()) {
        size_t next = path.find('\\', pos);
        if (next == std::string::npos) next = path.size();
        std::string sub = path.substr(0, next);
        if (!sub.empty()) {
            _mkdir(sub.c_str());
        }
        pos = next + 1;
    }
    _mkdir(path.c_str());
#else
    // Linux/Mac: 逐层创建
    std::string path = dir;
    size_t pos = 1;
    while (pos < path.size()) {
        size_t next = path.find('/', pos);
        if (next == std::string::npos) next = path.size();
        std::string sub = path.substr(0, next);
        mkdir(sub.c_str(), 0755);
        pos = next + 1;
    }
    mkdir(path.c_str(), 0755);
#endif

    created_dirs_.insert(dir);
}

// 获取或创建 spdlog async logger（带 rotating_file_sink）
std::shared_ptr<spdlog::logger> LoggerManager::get_or_create_logger(
    const std::string& full_path,
    const std::string& date_str,
    const std::string& /*filename*/)
{
    // 快速路径：读锁查缓存（多线程并发读取，零争用）
    {
        std::shared_lock<std::shared_mutex> rlock(logger_mtx_);
        if (cached_date_ == date_str) {
            auto it = loggers_.find(full_path);
            if (it != loggers_.end()) {
                return it->second;
            }
        }
    }

    // 慢速路径：写锁创建新 logger
    std::unique_lock<std::shared_mutex> wlock(logger_mtx_);

    // 检测日期变化
    check_date_change(date_str);

    // Double-check：可能其他线程已创建
    auto it = loggers_.find(full_path);
    if (it != loggers_.end()) {
        return it->second;
    }

    // 提取目录部分并确保存在
    std::string dir = full_path;
#ifdef _WIN32
    size_t last_sep = dir.find_last_of("\\/");
#else
    size_t last_sep = dir.find_last_of('/');
#endif
    if (last_sep != std::string::npos) {
        ensure_directory(dir.substr(0, last_sep));
    }

    // 创建 rotating_file_sink（线程安全版）
    auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        full_path, max_file_size_, max_files_);

    // 仅输出原始消息（时间戳和级别已在 log() 中自行格式化）
    sink->set_pattern("%v");

    // 创建唯一 logger 名称（路径即名称，确保唯一）
    std::string logger_name = full_path;

    // 从 spdlog 注册表中移除可能存在的同名 logger（防止冲突）
    spdlog::drop(logger_name);

    // 创建异步 logger，使用全局线程池，满队列时阻塞（保证不丢数据）
    auto logger = std::make_shared<spdlog::async_logger>(
        logger_name,
        sink,
        thread_pool_,
        spdlog::async_overflow_policy::block);

    logger->set_level(spdlog::level::trace);
    logger->flush_on(spdlog::level::warn);

    // 注册到 spdlog 全局注册表
    spdlog::register_logger(logger);

    // 缓存
    loggers_[full_path] = logger;

    return logger;
}

// 批量获取 loggers（减少锁获取次数，优化1000并发场景）
std::vector<std::shared_ptr<spdlog::logger>> LoggerManager::get_loggers_batch(
    const std::vector<LogTarget>& targets,
    const std::string& date_str)
{
    std::vector<std::shared_ptr<spdlog::logger>> result;
    result.reserve(targets.size());
    std::vector<size_t> missing_indices;

    // 第一阶段：读锁批量查缓存
    {
        std::shared_lock<std::shared_mutex> rlock(logger_mtx_);
        bool date_ok = (cached_date_ == date_str);
        for (size_t i = 0; i < targets.size(); ++i) {
            if (date_ok) {
                auto it = loggers_.find(targets[i].path);
                if (it != loggers_.end()) {
                    result.push_back(it->second);
                    continue;
                }
            }
            result.push_back(nullptr);
            missing_indices.push_back(i);
        }
    }

    // 全部命中缓存，直接返回
    if (missing_indices.empty()) {
        return result;
    }

    // 第二阶段：写锁创建缺失的 loggers
    {
        std::unique_lock<std::shared_mutex> wlock(logger_mtx_);
        check_date_change(date_str);

        for (size_t idx : missing_indices) {
            // Double-check
            auto it = loggers_.find(targets[idx].path);
            if (it != loggers_.end()) {
                result[idx] = it->second;
                continue;
            }

            // 提取目录并确保存在
            std::string dir = targets[idx].path;
#ifdef _WIN32
            size_t last_sep = dir.find_last_of("\\/");
#else
            size_t last_sep = dir.find_last_of('/');
#endif
            if (last_sep != std::string::npos) {
                ensure_directory(dir.substr(0, last_sep));
            }

            auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                targets[idx].path, max_file_size_, max_files_);
            sink->set_pattern("%v");

            std::string logger_name = targets[idx].path;
            spdlog::drop(logger_name);

            auto logger = std::make_shared<spdlog::async_logger>(
                logger_name, sink, thread_pool_,
                spdlog::async_overflow_policy::block);
            logger->set_level(spdlog::level::trace);
            logger->flush_on(spdlog::level::warn);
            spdlog::register_logger(logger);

            loggers_[targets[idx].path] = logger;
            result[idx] = logger;
        }
    }

    return result;
}

// 检测日期变化，清除旧 logger 缓存
void LoggerManager::check_date_change(const std::string& current_date)
{
    // 已在 logger_mtx_ 锁内调用
    if (cached_date_.empty()) {
        cached_date_ = current_date;
        return;
    }
    if (cached_date_ == current_date) {
        return;
    }

    // 日期已变化：刷新并释放所有旧 logger，清除目录缓存
    for (auto& pair : loggers_) {
        if (pair.second) {
            pair.second->flush();
            spdlog::drop(pair.second->name());
        }
    }
    loggers_.clear();
    created_dirs_.clear();
    // 保留 root_dir_ 在缓存中
    created_dirs_.insert(root_dir_);

    cached_date_ = current_date;
}

// 清理过期日志目录
void LoggerManager::cleanup_old_logs()
{
    if (root_dir_.empty() || retention_days_ <= 0) return;

    // 获取当前日期
    auto now = std::chrono::system_clock::now();
    std::time_t now_t = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
#ifdef _WIN32
    localtime_s(&now_tm, &now_t);
#else
    localtime_r(&now_t, &now_tm);
#endif

    // 计算截止日期的 time_t
    auto cutoff = now - std::chrono::hours(24 * retention_days_);
    std::time_t cutoff_t = std::chrono::system_clock::to_time_t(cutoff);

#ifdef _WIN32
    // Windows: 使用 FindFirstFile/FindNextFile 遍历子目录
    std::string search_path = root_dir_;
    for (auto& c : search_path) { if (c == '/') c = '\\'; }
    if (search_path.back() != '\\') search_path += '\\';
    search_path += '*';

    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(search_path.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        std::string name = ffd.cFileName;
        if (name == "." || name == "..") continue;
        // 匹配 yyyy-MM-dd 格式
        if (name.size() != 10 || name[4] != '-' || name[7] != '-') continue;

        // 解析日期
        std::tm dir_tm = {};
        int y, m, d;
        if (sscanf(name.c_str(), "%d-%d-%d", &y, &m, &d) != 3) continue;
        dir_tm.tm_year = y - 1900;
        dir_tm.tm_mon = m - 1;
        dir_tm.tm_mday = d;
        dir_tm.tm_isdst = -1;
        std::time_t dir_t = mktime(&dir_tm);

        if (dir_t < cutoff_t) {
            std::string dir_full = root_dir_;
            for (auto& c : dir_full) { if (c == '/') c = '\\'; }
            if (dir_full.back() != '\\') dir_full += '\\';
            dir_full += name;
            remove_directory_recursive(dir_full);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    // Linux/Mac
    DIR* dp = opendir(root_dir_.c_str());
    if (!dp) return;

    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        if (entry->d_type != DT_DIR) continue;
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (name.size() != 10 || name[4] != '-' || name[7] != '-') continue;

        std::tm dir_tm = {};
        int y, m, d;
        if (sscanf(name.c_str(), "%d-%d-%d", &y, &m, &d) != 3) continue;
        dir_tm.tm_year = y - 1900;
        dir_tm.tm_mon = m - 1;
        dir_tm.tm_mday = d;
        dir_tm.tm_isdst = -1;
        std::time_t dir_t = mktime(&dir_tm);

        if (dir_t < cutoff_t) {
            std::string dir_full = root_dir_ + "/" + name;
            remove_directory_recursive(dir_full);
        }
    }
    closedir(dp);
#endif
}

// 清理线程函数
void LoggerManager::cleanup_thread_func()
{
    // 启动时先执行一次清理
    cleanup_old_logs();

    while (cleanup_running_.load()) {
        // 使用条件变量等待，每24小时清理一次，但可以被立即唤醒退出
        std::unique_lock<std::mutex> lock(cleanup_mtx_);
        cleanup_cv_.wait_for(lock, std::chrono::hours(24), [this]() {
            return !cleanup_running_.load();
        });
        if (cleanup_running_.load()) {
            cleanup_old_logs();
        }
    }
}
