#ifndef __NETWORK_LOGGER_HPP__
#define __NETWORK_LOGGER_HPP__

// Aether 日志系统 - 合并版
// 原文件：util.hpp, level.hpp, message.hpp, formatter.hpp, sink.hpp, logger.hpp, log.h

#include <string>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <sys/syscall.h>
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <queue>
#include <cassert>
#include <memory>
#include <tuple>
#include <sstream>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace logsystem {

// ===== util.hpp =====
namespace util {
    class date {
    public:
        static size_t now() { return (size_t)time(nullptr); }
        static size_t nowMs() {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch());
            return ms.count();
        }
        static std::pair<size_t, size_t> nowTime() {
            auto tp = std::chrono::system_clock::now();
            auto sec = std::chrono::system_clock::to_time_t(tp);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
            return {(size_t)sec, (size_t)ms.count()};
        }
    };

    class file {
    public:
        static std::string path(const std::string &filename) {
            if (filename.empty()) return ".";
            size_t pos = filename.find_last_of("/\\");
            if (pos == std::string::npos) return ".";
            return filename.substr(0, pos + 1);
        }
        static void create_directory(const std::string &path) {
            if (path.empty()) return;
            if (std::filesystem::exists(path)) return;
            try {
                std::filesystem::create_directories(path);
            } catch (const std::filesystem::filesystem_error &e) {
                std::cerr << e.what() << '\n';
            }
        }
    };

    class thread {
    public:
        static pid_t getTid() {
            static thread_local pid_t tid = 0;
            if (tid == 0) {
                tid = static_cast<pid_t>(syscall(SYS_gettid));
            }
            return tid;
        }
    };
}

// ===== level.hpp =====
class LogLevel {
public:
    enum class value {
        TRACE = 0,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        OFF
    };
    static const char *toString(LogLevel::value l) {
        switch (l) {
#define TOSTRING(name) #name
            case LogLevel::value::TRACE: return TOSTRING(TRACE);
            case LogLevel::value::DEBUG: return TOSTRING(DEBUG);
            case LogLevel::value::INFO:  return TOSTRING(INFO);
            case LogLevel::value::WARN:  return TOSTRING(WARN);
            case LogLevel::value::ERROR: return TOSTRING(ERROR);
            case LogLevel::value::FATAL: return TOSTRING(FATAL);
            case LogLevel::value::OFF:   return TOSTRING(OFF);
#undef TOSTRING
            default: return "UNKNOWN";
        }
    }
    static int toInt(LogLevel::value l) {
        return static_cast<int>(l);
    }
};

// ===== message.hpp =====
struct LogMsg {
    std::string _name;
    std::string _file;
    std::string _payload;
    LogLevel::value _level;
    size_t _line;
    size_t _ctime;
    size_t _ctime_ms;
    pid_t _pid;
    pid_t _tid;
    bool _flush_marker;

    LogMsg()
        : _level(LogLevel::value::TRACE)
        , _line(0)
        , _ctime(0)
        , _ctime_ms(0)
        , _pid(0)
        , _tid(0)
        , _flush_marker(false)
    {}

    LogMsg(const std::string &name, const std::string &file, size_t line,
           std::string payload, LogLevel::value level)
        : _name(name)
        , _file(file)
        , _payload(std::move(payload))
        , _level(level)
        , _line(line)
        , _ctime(0)
        , _ctime_ms(0)
        , _pid(getpid())
        , _tid(util::thread::getTid())
        , _flush_marker(false)
    {
        auto t = util::date::nowTime();
        _ctime = t.first;
        _ctime_ms = t.second;
    }

    static LogMsg flushMarker() {
        LogMsg msg("", "", 0, "", LogLevel::value::OFF);
        msg._flush_marker = true;
        return msg;
    }

    bool isFlushMarker() const { return _flush_marker; }
};

// ===== formatter.hpp =====
class FormatItem {
public:
    using ptr = std::shared_ptr<FormatItem>;
    virtual ~FormatItem() {}
    virtual void format(std::ostream &os, const LogMsg &msg) = 0;
};

class MsgFormatItem : public FormatItem {
public:
    MsgFormatItem(const std::string &) {}
    virtual void format(std::ostream &os, const LogMsg &msg) { os << msg._payload; }
};

class LevelFormatItem : public FormatItem {
public:
    LevelFormatItem(const std::string &) {}
    virtual void format(std::ostream &os, const LogMsg &msg) { os << LogLevel::toString(msg._level); }
};

class NameFormatItem : public FormatItem {
public:
    NameFormatItem(const std::string &) {}
    virtual void format(std::ostream &os, const LogMsg &msg) { os << msg._name; }
};

class ThreadFormatItem : public FormatItem {
public:
    ThreadFormatItem(const std::string &) {}
    virtual void format(std::ostream &os, const LogMsg &msg) { os << msg._tid; }
};

class PidFormatItem : public FormatItem {
public:
    PidFormatItem(const std::string &) {}
    virtual void format(std::ostream &os, const LogMsg &msg) { os << msg._pid; }
};

class TimeFormatItem : public FormatItem {
private:
    std::string _format;
public:
    TimeFormatItem(const std::string &format = "%H:%M:%S") : _format(format) {
        if (format.empty()) _format = "%H:%M:%S";
    }
    virtual void format(std::ostream &os, const LogMsg &msg) {
        time_t t = msg._ctime;
        struct tm lt;
        localtime_r(&t, &lt);
        std::string fmt = _format;
        char ms_buf[8];
        snprintf(ms_buf, sizeof(ms_buf), "%03zu", msg._ctime_ms);
        size_t pos = 0;
        while ((pos = fmt.find("%i", pos)) != std::string::npos) {
            fmt.replace(pos, 2, ms_buf);
            pos += strlen(ms_buf);
        }
        char tmp[128];
        strftime(tmp, 127, fmt.c_str(), &lt);
        os << tmp;
    }
};

class CFileFormatItem : public FormatItem {
public:
    CFileFormatItem(const std::string &) {}
    virtual void format(std::ostream &os, const LogMsg &msg) { os << msg._file; }
};

class CLineFormatItem : public FormatItem {
public:
    CLineFormatItem(const std::string &) {}
    virtual void format(std::ostream &os, const LogMsg &msg) { os << msg._line; }
};

class LiteralFormatItem : public FormatItem {
private:
    std::string _str;
public:
    LiteralFormatItem(const std::string &str = "") : _str(str) {}
    virtual void format(std::ostream &os, const LogMsg &) { os << _str; }
};

class Formatter {
public:
    using ptr = std::shared_ptr<Formatter>;
    using ItemFactory = std::function<FormatItem::ptr(const std::string&)>;
    /*
        格式化字符说明：
            %d  日期（支持子格式 {%Y-%m-%d %H:%M:%S}，%i 表示毫秒）
            %P  进程ID
            %t  线程ID
            %p  日志等级
            %c  日志器名称
            %f  文件名
            %l  行号
            %m  日志消息
            %n  换行
            %T  缩进(Tab)
    */
    static std::mutex& registryMutex() {
        static std::mutex m;
        return m;
    }
    static void registerItem(const std::string &key, ItemFactory factory) {
        std::lock_guard<std::mutex> lock(registryMutex());
        _registry[key] = factory;
    }

    static void registerBuiltinItems() {
        registerItem("m", [](const std::string&) { return std::make_shared<MsgFormatItem>(""); });
        registerItem("p", [](const std::string&) { return std::make_shared<LevelFormatItem>(""); });
        registerItem("c", [](const std::string&) { return std::make_shared<NameFormatItem>(""); });
        registerItem("t", [](const std::string&) { return std::make_shared<ThreadFormatItem>(""); });
        registerItem("P", [](const std::string&) { return std::make_shared<PidFormatItem>(""); });
        registerItem("n", [](const std::string&) { return std::make_shared<LiteralFormatItem>("\n"); });
        registerItem("T", [](const std::string&) { return std::make_shared<LiteralFormatItem>("\t"); });
        registerItem("d", [](const std::string &subfmt) { return std::make_shared<TimeFormatItem>(subfmt); });
        registerItem("f", [](const std::string&) { return std::make_shared<CFileFormatItem>(""); });
        registerItem("l", [](const std::string&) { return std::make_shared<CLineFormatItem>(""); });
    }

    Formatter(const std::string &pattern = "[%d{%Y-%m-%d %H:%M:%S.%i}][%P][%t][%p][%c][%f:%l] %m%n")
        : _pattern(pattern) {
        static std::once_flag once;
        std::call_once(once, registerBuiltinItems);
        assert(parsePattern());
    }

    const std::string &pattern() const { return _pattern; }

    std::string format(const LogMsg &msg) {
        thread_local std::ostringstream ss;
        ss.str("");
        ss.clear();
        for (auto &it : _items) { it->format(ss, msg); }
        return ss.str();
    }

private:
    FormatItem::ptr createItem(const std::string &fc, const std::string &subfmt) {
        std::lock_guard<std::mutex> lock(registryMutex());
        auto it = _registry.find(fc);
        if (it != _registry.end()) { return it->second(subfmt); }
        return FormatItem::ptr();
    }

    bool parsePattern() {
        std::vector<std::tuple<std::string, std::string, int>> arry;
        std::string format_key, format_val, string_row;
        bool sub_format_error = false;
        size_t pos = 0;

        while (pos < _pattern.size()) {
            if (_pattern[pos] != '%') { string_row.append(1, _pattern[pos++]); continue; }
            if (pos + 1 < _pattern.size() && _pattern[pos + 1] == '%') {
                string_row.append(1, '%');
                pos += 2;
                continue;
            }
            if (!string_row.empty()) {
                arry.push_back(std::make_tuple(string_row, "", 0));
                string_row.clear();
            }
            pos += 1;
            if (pos < _pattern.size() && isalpha(_pattern[pos])) {
                format_key = _pattern[pos];
            } else {
                std::cout << &_pattern[pos - 1] << " 位置附近格式错误！\n";
                return false;
            }
            pos += 1;
            if (pos < _pattern.size() && _pattern[pos] == '{') {
                sub_format_error = true;
                pos += 1;
                while (pos < _pattern.size()) {
                    if (_pattern[pos] == '}') {
                        sub_format_error = false;
                        pos += 1;
                        break;
                    }
                    format_val.append(1, _pattern[pos++]);
                }
            }
            arry.push_back(std::make_tuple(format_key, format_val, 1));
            format_key.clear();
            format_val.clear();
        }

        if (sub_format_error) { std::cout << "{} 对应出错\n"; return false; }
        if (!string_row.empty()) arry.push_back(std::make_tuple(string_row, "", 0));

        for (auto &it : arry) {
            if (std::get<2>(it) == 0) {
                _items.push_back(std::make_shared<LiteralFormatItem>(std::get<0>(it)));
            } else {
                FormatItem::ptr fi = createItem(std::get<0>(it), std::get<1>(it));
                if (fi.get() == nullptr) {
                    std::cout << "没有对应的格式化字符: %" << std::get<0>(it) << std::endl;
                    return false;
                }
                _items.push_back(fi);
            }
        }
        return true;
    }

private:
    std::string _pattern;
    std::vector<FormatItem::ptr> _items;
    static std::unordered_map<std::string, ItemFactory> _registry;
};

inline std::unordered_map<std::string, Formatter::ItemFactory> Formatter::_registry;

// ===== sink.hpp =====
class LogSink {
public:
    using ptr = std::shared_ptr<LogSink>;
    LogSink() : _level(LogLevel::value::TRACE) {}
    virtual ~LogSink() {}

    void log(const std::string &msg) {
        std::lock_guard<std::mutex> lock(_mutex);
        logImpl(msg);
    }

    void flush() {
        std::lock_guard<std::mutex> lock(_mutex);
        flushImpl();
    }

    void setLevel(LogLevel::value level) { _level.store(level, std::memory_order_relaxed); }
    LogLevel::value level() const { return _level.load(std::memory_order_relaxed); }
    bool shouldLog(LogLevel::value level) const { return level >= _level.load(std::memory_order_relaxed); }

protected:
    virtual void logImpl(const std::string &msg) = 0;
    virtual void flushImpl() {}
    std::mutex _mutex;
    std::atomic<LogLevel::value> _level;
};

class StdoutSink : public LogSink {
public:
    using ptr = std::shared_ptr<StdoutSink>;
protected:
    void logImpl(const std::string &msg) override { std::cout << msg; std::cout.flush(); }
    void flushImpl() override { std::cout.flush(); }
};

class FileSink : public LogSink {
public:
    using ptr = std::shared_ptr<FileSink>;
    FileSink(const std::string &filename) : _filename(filename) {
        util::file::create_directory(util::file::path(filename));
        _ofs.open(_filename, std::ios::binary | std::ios::app);
        assert(_ofs.is_open());
    }
    ~FileSink() {
        if (_ofs.is_open()) { _ofs.flush(); _ofs.close(); }
    }
protected:
    void logImpl(const std::string &msg) override { _ofs << msg; }
    void flushImpl() override { if (_ofs.is_open()) _ofs.flush(); }
private:
    std::string _filename;
    std::ofstream _ofs;
};

class RollSink : public LogSink {
public:
    using ptr = std::shared_ptr<RollSink>;
    RollSink(const std::string &basename, size_t max_fsize)
        : _basename(basename), _max_fsize(max_fsize), _cur_fsize(0), _file_index(0) {
        util::file::create_directory(util::file::path(basename));
    }
    ~RollSink() {
        if (_ofs.is_open()) { _ofs.flush(); _ofs.close(); }
    }
protected:
    void logImpl(const std::string &msg) override {
        initLogFile();
        _ofs << msg;
        _cur_fsize += msg.size();
    }
    void flushImpl() override { if (_ofs.is_open()) _ofs.flush(); }
private:
    void initLogFile() {
        if (!_ofs.is_open() || _cur_fsize >= _max_fsize) {
            if (_ofs.is_open()) { _ofs.flush(); _ofs.close(); }
            std::string name = createFilename();
            _ofs.open(name, std::ios::binary | std::ios::app);
            assert(_ofs.is_open());
            _ofs.seekp(0, std::ios::end);
            _cur_fsize = _ofs.tellp();
        }
    }
    std::string createFilename() {
        time_t t = time(nullptr);
        struct tm lt;
        localtime_r(&t, &lt);
        std::stringstream ss;
        ss << _basename
           << lt.tm_year + 1900
           << std::setw(2) << std::setfill('0') << lt.tm_mon + 1
           << std::setw(2) << std::setfill('0') << lt.tm_mday
           << std::setw(2) << std::setfill('0') << lt.tm_hour
           << std::setw(2) << std::setfill('0') << lt.tm_min
           << std::setw(2) << std::setfill('0') << lt.tm_sec
           << "-" << getpid()
           << "-" << ++_file_index
           << ".log";
        return ss.str();
    }
    std::string _basename;
    std::ofstream _ofs;
    size_t _max_fsize;
    size_t _cur_fsize;
    size_t _file_index;
};

class SinkFactory {
public:
    using SinkCreator = std::function<LogSink::ptr(const std::string&)>;
    static std::mutex& creatorsMutex() {
        static std::mutex m;
        return m;
    }
    static void registerSink(const std::string &type, SinkCreator creator) {
        std::lock_guard<std::mutex> lock(creatorsMutex());
        _creators[type] = creator;
    }
    static LogSink::ptr create(const std::string &desc) {
        static std::once_flag once;
        std::call_once(once, registerBuiltinSinks);
        size_t pos = desc.find(':');
        std::string type = (pos == std::string::npos) ? desc : desc.substr(0, pos);
        std::string param = (pos == std::string::npos) ? "" : desc.substr(pos + 1);
        SinkCreator creator;
        {
            std::lock_guard<std::mutex> lock(creatorsMutex());
            auto it = _creators.find(type);
            if (it != _creators.end()) creator = it->second;
        }
        if (creator) return creator(param);
        return LogSink::ptr();
    }
    static void registerBuiltinSinks() {
        registerSink("stdout", [](const std::string&) { return std::make_shared<StdoutSink>(); });
        registerSink("file", [](const std::string &filename) { return std::make_shared<FileSink>(filename); });
        registerSink("roll", [](const std::string &param) -> LogSink::ptr {
            size_t pos = param.find(':');
            if (pos == std::string::npos) return LogSink::ptr();
            std::string basename = param.substr(0, pos);
            size_t size = std::stoul(param.substr(pos + 1));
            return std::make_shared<RollSink>(basename, size);
        });
    }
private:
    static std::unordered_map<std::string, SinkCreator> _creators;
};

inline std::unordered_map<std::string, SinkFactory::SinkCreator> SinkFactory::_creators;

// ===== logger.hpp =====
class LoggerManager;

class BackendWorker {
public:
    using Callback = std::function<void(const std::vector<LogMsg>&)>;
    BackendWorker(Callback cb) : _callback(std::move(cb)) {}
    ~BackendWorker() { stop(); }

    void start() {
        _running.store(true, std::memory_order_release);
        _thread = std::thread(&BackendWorker::workerLoop, this);
        std::unique_lock<std::mutex> lock(_start_mtx);
        _start_cv.wait(lock, [this] { return _started.load(std::memory_order_acquire); });
    }

    void push(LogMsg msg) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_running.load(std::memory_order_acquire)) return;
            _write_queue.push_back(std::move(msg));
        }
        _cv.notify_one();
    }

    void flush() {
        auto request = std::make_shared<FlushRequest>();
        { std::lock_guard<std::mutex> lock(_mutex);
          _write_queue.push_back(LogMsg::flushMarker());
          _flush_requests.push(request); }
        _cv.notify_one();
        std::unique_lock<std::mutex> flk(request->mtx);
        request->cv.wait(flk, [&] { return request->done; });
    }

    void stop() {
        { std::lock_guard<std::mutex> lock(_mutex);
          if (!_running.load(std::memory_order_relaxed)) return;
          _running.store(false, std::memory_order_release); }
        _cv.notify_one();
        if (_thread.joinable()) _thread.join();
    }

private:
    struct FlushRequest {
        std::mutex mtx;
        std::condition_variable cv;
        bool done = false;
    };

    void workerLoop() {
        { std::lock_guard<std::mutex> lock(_start_mtx);
          _started.store(true, std::memory_order_release); }
        _start_cv.notify_one();
        std::vector<LogMsg> read_queue;
        while (_running.load(std::memory_order_acquire) || !_write_queue.empty()) {
            { std::unique_lock<std::mutex> lock(_mutex);
              _cv.wait_for(lock, std::chrono::seconds(3), [this] {
                  return !_write_queue.empty() || !_running.load(std::memory_order_acquire);
              });
              std::swap(_write_queue, read_queue); }
            if (!read_queue.empty()) {
                _callback(read_queue);
                for (auto &msg : read_queue) {
                    if (msg.isFlushMarker()) {
                        std::lock_guard<std::mutex> lock(_mutex);
                        if (!_flush_requests.empty()) {
                            auto request = _flush_requests.front();
                            _flush_requests.pop();
                            request->done = true;
                            request->cv.notify_one();
                        }
                    }
                }
                read_queue.clear();
            }
        }
        { std::lock_guard<std::mutex> lock(_mutex); std::swap(_write_queue, read_queue); }
        if (!read_queue.empty()) { _callback(read_queue); }
        // 处理剩余的 flush 请求，避免 flush() 永久阻塞
        {
            std::lock_guard<std::mutex> lock(_mutex);
            while (!_flush_requests.empty()) {
                auto request = _flush_requests.front();
                _flush_requests.pop();
                request->done = true;
                request->cv.notify_one();
            }
        }
    }

    std::atomic<bool> _running{false};
    std::atomic<bool> _started{false};
    std::mutex _mutex, _start_mtx;
    std::condition_variable _cv, _start_cv;
    std::vector<LogMsg> _write_queue;
    std::thread _thread;
    Callback _callback;
    std::queue<std::shared_ptr<FlushRequest>> _flush_requests;
};

class LogFilter {
public:
    using ptr = std::shared_ptr<LogFilter>;
    virtual ~LogFilter() {}
    virtual bool filter(const LogMsg &msg) = 0;
};

class LevelFilter : public LogFilter {
public:
    LevelFilter(LogLevel::value level) : _level(level) {}
    bool filter(const LogMsg &msg) override { return msg._level >= _level; }
private:
    LogLevel::value _level;
};

class ModuleFilter : public LogFilter {
public:
    ModuleFilter(const std::string &module) : _module(module) {}
    bool filter(const LogMsg &msg) override { return msg._name == _module; }
private:
    std::string _module;
};

class Logger {
public:
    using ptr = std::shared_ptr<Logger>;

    Logger(const std::string &name, Formatter::ptr formatter,
           std::vector<LogSink::ptr> &sinks, LogLevel::value level = LogLevel::value::DEBUG)
        : _name(name), _level(level), _flush_level(LogLevel::value::ERROR)
        , _formatter(formatter), _sinks(sinks.begin(), sinks.end()) {}

    ~Logger() { if (_backend) _backend->stop(); }

    std::string loggerName() const { return _name; }
    LogLevel::value loggerLevel() const { return _level; }
    void setLevel(LogLevel::value level) { _level = level; }
    void setFlushLevel(LogLevel::value level) { _flush_level = level; }
    void addFilter(LogFilter::ptr filter) {
        std::lock_guard<std::mutex> lock(_filter_mutex);
        _filters.push_back(filter);
    }
    void flushSinks() { for (auto &it : _sinks) it->flush(); }

    void log(LogMsg msg) {
        if (msg._level == LogLevel::value::OFF) return;
        if (!shouldLog(msg)) return;
        if (_backend) {
            LogLevel::value level = msg._level;
            _backend->push(std::move(msg));
            if (level >= _flush_level) _backend->flush();
        } else {
            std::string formatted = _formatter->format(msg);
            std::lock_guard<std::mutex> lock(_mutex);
            for (auto &it : _sinks) {
                if (it->shouldLog(msg._level)) it->log(formatted);
            }
            if (msg._level >= _flush_level) flushSinks();
        }
    }

    class LogMessage {
    public:
        LogMessage(Logger &logger, LogLevel::value level, const char *file, size_t line)
            : _logger(logger) { _msg = LogMsg(logger._name, file, line, "", level); }
        ~LogMessage() {
            _msg._payload = std::move(_payload);
            try {
                _logger.log(std::move(_msg));
            } catch (const std::exception& e) {
            } catch (...) {}
        }

        LogMessage& operator<<(const char *v) { if (v) _payload += v; else _payload += "(null)"; return *this; }
        LogMessage& operator<<(const std::string &v) { _payload += v; return *this; }
        LogMessage& operator<<(int v) { _payload += std::to_string(v); return *this; }
        LogMessage& operator<<(unsigned int v) { _payload += std::to_string(v); return *this; }
        LogMessage& operator<<(long v) { _payload += std::to_string(v); return *this; }
        LogMessage& operator<<(unsigned long v) { _payload += std::to_string(v); return *this; }
        LogMessage& operator<<(long long v) { _payload += std::to_string(v); return *this; }
        LogMessage& operator<<(unsigned long long v) { _payload += std::to_string(v); return *this; }
        LogMessage& operator<<(double v) { char buf[32]; snprintf(buf, sizeof(buf), "%g", v); _payload += buf; return *this; }
        LogMessage& operator<<(float v) { char buf[32]; snprintf(buf, sizeof(buf), "%g", v); _payload += buf; return *this; }
        LogMessage& operator<<(bool v) { _payload += v ? "true" : "false"; return *this; }
        LogMessage& operator<<(const void *v) { char buf[32]; snprintf(buf, sizeof(buf), "%p", v); _payload += buf; return *this; }

        template<typename T>
        LogMessage& operator<<(const T &v) { std::ostringstream ss; ss << v; _payload += ss.str(); return *this; }

        LogMessage(const LogMessage &) = delete;
        LogMessage &operator=(const LogMessage &) = delete;

    private:
        Logger &_logger;
        LogMsg _msg;
        std::string _payload;
    };

    LogMessage stream(LogLevel::value level, const char *file, size_t line) {
        return LogMessage(*this, level, file, line);
    }

    template<typename... Args>
    void format(LogLevel::value level, const char *file, size_t line,
                const std::string &fmt, Args&&... args) {
        std::string payload = formatString(fmt, std::forward<Args>(args)...);
        LogMsg msg(_name, file, line, payload, level);
        log(std::move(msg));
    }

public:
    class Builder {
    public:
        using ptr = std::shared_ptr<Builder>;
        Builder& name(const std::string &n) { _name = n; return *this; }
        Builder& level(LogLevel::value l) { _level = l; return *this; }
        Builder& formatter(const Formatter::ptr &f) { _formatter = f; return *this; }
        Builder& formatter(const std::string &pattern) { _formatter = std::make_shared<Formatter>(pattern); return *this; }
        Builder& sink(const LogSink::ptr &s) { _sinks.push_back(s); return *this; }
        Builder& async() { _async = true; return *this; }
        Builder& flushLevel(LogLevel::value l) { _flush_level = l; return *this; }

        Logger::ptr build() {
            if (_name.empty()) { throw std::invalid_argument("Logger name cannot be empty!"); }
            if (!_formatter) _formatter = std::make_shared<Formatter>();
            if (_sinks.empty()) _sinks.push_back(std::make_shared<StdoutSink>());

            auto logger = std::make_shared<Logger>(_name, _formatter, _sinks, _level);
            logger->setFlushLevel(_flush_level);

            if (_async) {
                // 使用 weak_ptr 防止循环引用：Logger -> BackendWorker -> lambda -> Logger
                std::weak_ptr<Logger> weak_logger = logger;
                logger->_backend = std::make_shared<BackendWorker>(
                    [weak_logger](const std::vector<LogMsg> &msgs) {
                        auto logger = weak_logger.lock();
                        if (!logger) return;
                        for (auto &msg : msgs) {
                            if (msg.isFlushMarker()) continue;
                            if (!logger->shouldLog(msg)) continue;
                            std::string formatted = logger->_formatter->format(msg);
                            for (auto &sink : logger->_sinks) {
                                if (sink->shouldLog(msg._level)) sink->log(formatted);
                            }
                        }
                        logger->flushSinks();
                    });
                logger->_backend->start();
            }
            return logger;
        }

        Logger::ptr buildGlobal();

    private:
        std::string _name = "root";
        LogLevel::value _level = LogLevel::value::DEBUG;
        Formatter::ptr _formatter;
        std::vector<LogSink::ptr> _sinks;
        bool _async = false;
        LogLevel::value _flush_level = LogLevel::value::ERROR;
    };

private:
    bool shouldLog(const LogMsg &msg) const {
        if (msg._level < _level.load(std::memory_order_relaxed)) return false;
        // 使用 copy-on-write 策略：拷贝一份 _filters 遍历，避免长时间持锁
        std::vector<LogFilter::ptr> filters_copy;
        {
            std::lock_guard<std::mutex> lock(_filter_mutex);
            filters_copy = _filters;
        }
        for (auto &filter : filters_copy) { if (!filter->filter(msg)) return false; }
        return true;
    }

    template<typename... Args>
    std::string formatString(const std::string &fmt, Args&&... args) {
        std::ostringstream ss;
        size_t pos = 0;
        size_t arg_index = 0;
        while (pos < fmt.size()) {
            if (fmt[pos] == '{') {
                if (pos + 1 < fmt.size() && fmt[pos + 1] == '{') {
                    ss << '{';
                    pos += 2;
                    continue;
                }
                if (pos + 1 < fmt.size() && fmt[pos + 1] == '}') {
                    size_t i = 0;
                    bool found = false;
                    auto output_arg = [&](auto &&arg) {
                        if (!found && i == arg_index) {
                            ss << arg;
                            found = true;
                        }
                        i++;
                    };
                    (output_arg(args), ...);
                    if (!found) ss << "{}";
                    arg_index++;
                    pos += 2;
                } else {
                    ss << fmt[pos++];
                }
            } else if (fmt[pos] == '}') {
                if (pos + 1 < fmt.size() && fmt[pos + 1] == '}') {
                    ss << '}';
                    pos += 2;
                    continue;
                }
                ss << fmt[pos++];
            } else {
                ss << fmt[pos++];
            }
        }
        return ss.str();
    }

private:
    std::string _name;
    std::atomic<LogLevel::value> _level;
    std::atomic<LogLevel::value> _flush_level;
    Formatter::ptr _formatter;
    std::vector<LogSink::ptr> _sinks;
    std::vector<LogFilter::ptr> _filters;
    mutable std::mutex _mutex;  // mutable 允许 const 方法加锁（shouldLog）
    mutable std::mutex _filter_mutex;
    std::shared_ptr<BackendWorker> _backend;
};

class LoggerManager {
private:
    std::mutex _mutex;
    Logger::ptr _root_logger;
    std::unordered_map<std::string, Logger::ptr> _loggers;

    LoggerManager() {
        Logger::Builder builder;
        builder.name("root");
        _root_logger = builder.build();
        _loggers.insert(std::make_pair("root", _root_logger));
    }
    LoggerManager(const LoggerManager &) = delete;
    LoggerManager &operator=(const LoggerManager &) = delete;

public:
    static LoggerManager &getInstance() {
        static LoggerManager lm;
        return lm;
    }

    bool hasLogger(const std::string &name) {
        std::lock_guard<std::mutex> lock(_mutex);
        return _loggers.find(name) != _loggers.end();
    }

    bool addLogger(const std::string &name, Logger::ptr logger) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_loggers.find(name) != _loggers.end()) {
            return false;
        }
        _loggers.insert(std::make_pair(name, logger));
        return true;
    }

    Logger::ptr getLogger(const std::string &name) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _loggers.find(name);
        if (it == _loggers.end()) return Logger::ptr();
        return it->second;
    }

    Logger::ptr rootLogger() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _root_logger;
    }

    void flushAll() {
        std::lock_guard<std::mutex> lock(_mutex);
        for (auto &it : _loggers) it.second->flushSinks();
    }
};

inline Logger::ptr Logger::Builder::buildGlobal() {
    auto logger = build();
    LoggerManager::getInstance().addLogger(_name, logger);
    return logger;
}

// ===== log.h =====
inline Logger::ptr getLogger(const std::string &name) {
    return LoggerManager::getInstance().getLogger(name);
}

inline Logger::ptr rootLogger() {
    return LoggerManager::getInstance().rootLogger();
}

#ifndef LOG_ACTIVE_LEVEL
#define LOG_ACTIVE_LEVEL 2
#endif

#define LOG(level) logsystem::LoggerManager::getInstance().rootLogger()->stream( \
    logsystem::LogLevel::value::level, __FILE__, __LINE__)

#define LOG_TRACE logsystem::LoggerManager::getInstance().rootLogger()->stream( \
    logsystem::LogLevel::toInt(logsystem::LogLevel::value::TRACE) >= LOG_ACTIVE_LEVEL \
    ? logsystem::LogLevel::value::TRACE : logsystem::LogLevel::value::OFF, __FILE__, __LINE__)
#define LOG_DEBUG logsystem::LoggerManager::getInstance().rootLogger()->stream( \
    logsystem::LogLevel::toInt(logsystem::LogLevel::value::DEBUG) >= LOG_ACTIVE_LEVEL \
    ? logsystem::LogLevel::value::DEBUG : logsystem::LogLevel::value::OFF, __FILE__, __LINE__)
#define LOG_INFO logsystem::LoggerManager::getInstance().rootLogger()->stream( \
    logsystem::LogLevel::toInt(logsystem::LogLevel::value::INFO) >= LOG_ACTIVE_LEVEL \
    ? logsystem::LogLevel::value::INFO : logsystem::LogLevel::value::OFF, __FILE__, __LINE__)
#define LOG_WARN logsystem::LoggerManager::getInstance().rootLogger()->stream( \
    logsystem::LogLevel::toInt(logsystem::LogLevel::value::WARN) >= LOG_ACTIVE_LEVEL \
    ? logsystem::LogLevel::value::WARN : logsystem::LogLevel::value::OFF, __FILE__, __LINE__)
#define LOG_ERROR logsystem::LoggerManager::getInstance().rootLogger()->stream( \
    logsystem::LogLevel::toInt(logsystem::LogLevel::value::ERROR) >= LOG_ACTIVE_LEVEL \
    ? logsystem::LogLevel::value::ERROR : logsystem::LogLevel::value::OFF, __FILE__, __LINE__)
#define LOG_FATAL logsystem::LoggerManager::getInstance().rootLogger()->stream( \
    logsystem::LogLevel::toInt(logsystem::LogLevel::value::FATAL) >= LOG_ACTIVE_LEVEL \
    ? logsystem::LogLevel::value::FATAL : logsystem::LogLevel::value::OFF, __FILE__, __LINE__)

inline void signalHandler(int sig) {
    const char *name = "UNKNOWN";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV"; break;
        case SIGABRT: name = "SIGABRT"; break;
        case SIGFPE:  name = "SIGFPE";  break;
        case SIGILL:  name = "SIGILL";  break;
        case SIGTERM: name = "SIGTERM"; break;
    }
    const char prefix[] = "\n=== Signal ";
    const char mid[] = " PID=";
    char pid_buf[16];
    snprintf(pid_buf, sizeof(pid_buf), "%d ===\n", getpid());
    write(STDERR_FILENO, prefix, sizeof(prefix) - 1);
    write(STDERR_FILENO, name, strlen(name));
    write(STDERR_FILENO, mid, sizeof(mid) - 1);
    write(STDERR_FILENO, pid_buf, strlen(pid_buf));
    void *array[32];
    int size = backtrace(array, 32);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    const char suffix[] = "=== End stack trace ===\n\n";
    write(STDERR_FILENO, suffix, sizeof(suffix) - 1);
    signal(sig, SIG_DFL);
    raise(sig);
}

#define INSTALL_SIGNAL_HANDLER() do { \
    signal(SIGSEGV, logsystem::signalHandler); \
    signal(SIGABRT, logsystem::signalHandler); \
    signal(SIGFPE,  logsystem::signalHandler); \
    signal(SIGILL,  logsystem::signalHandler); \
    signal(SIGTERM, logsystem::signalHandler); \
} while(0)

} // namespace logsystem

#endif