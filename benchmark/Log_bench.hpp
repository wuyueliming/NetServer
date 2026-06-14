#pragma once

#include <iostream>
#include <cstdio>
#include <string>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <ctime>
#include <memory>
#include <unistd.h>
#include <mutex>

namespace NetServer
{
    namespace LogModule
    {

        const std::string gsep = "\r\n";

        class LogStrategy
        {
        public:
            virtual ~LogStrategy() = default;
            virtual void SyncLog(const std::string &message) = 0;
        };

        class ConsoleLogStrategy : public LogStrategy
        {
        public:
            ConsoleLogStrategy()
            {
            }
            void SyncLog(const std::string &message) override
            {
                std::lock_guard lockguard(_mutex);
                std::cout << message << gsep;
            }
            ~ConsoleLogStrategy()
            {
            }

        private:
            std::mutex _mutex;
        };

        const std::string defaultpath = "./log/";
        const std::string defaultfile = "my.log";
        class FileLogStrategy : public LogStrategy
        {
        public:
            FileLogStrategy(const std::string &path = defaultpath, const std::string &file = defaultfile)
                : _path(path),
                  _file(file)
            {
                std::lock_guard lockguard(_mutex);
                if (std::filesystem::exists(_path))
                {
                    return;
                }
                try
                {
                    std::filesystem::create_directories(_path);
                }
                catch (const std::filesystem::filesystem_error &e)
                {
                    std::cerr << e.what() << '\n';
                }
            }
            void SyncLog(const std::string &message) override
            {
                std::lock_guard lockguard(_mutex);

                std::string filename = _path + (_path.back() == '/' ? "" : "/") + _file;
                std::ofstream out(filename, std::ios::app);
                if (!out.is_open())
                {
                    return;
                }
                out << message << gsep;
                out.close();
            }
            ~FileLogStrategy()
            {
            }

        private:
            std::string _path;
            std::string _file;
            std::mutex _mutex;
        };

        class NullLogStrategy : public LogStrategy
        {
        public:
            void SyncLog(const std::string &message) override {}
        };

        enum LogLevel
        {
            DEBUG,
            INFO,
            WARNING,
            ERROR,
            FATAL
        };
        inline std::string Level2Str(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::DEBUG:
                return "DEBUG";
            case LogLevel::INFO:
                return "INFO";
            case LogLevel::WARNING:
                return "WARNING";
            case LogLevel::ERROR:
                return "ERROR";
            case LogLevel::FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
            }
        }
        inline std::string GetTimeStamp()
        {
            time_t curr = time(nullptr);
            struct tm curr_tm;
            localtime_r(&curr, &curr_tm);
            char timebuffer[128];
            snprintf(timebuffer, sizeof(timebuffer), "%4d-%02d-%02d %02d:%02d:%02d",
                     curr_tm.tm_year + 1900,
                     curr_tm.tm_mon + 1,
                     curr_tm.tm_mday,
                     curr_tm.tm_hour,
                     curr_tm.tm_min,
                     curr_tm.tm_sec);
            return timebuffer;
        }

        class Logger
        {
        public:
            Logger()
            {
                EnableConsoleLogStrategy();
            }
            void EnableFileLogStrategy()
            {
                _fflush_strategy = std::make_unique<FileLogStrategy>();
            }
            void EnableConsoleLogStrategy()
            {
                _fflush_strategy = std::make_unique<ConsoleLogStrategy>();
            }
            void EnableNullLogStrategy()
            {
                _fflush_strategy = std::make_unique<NullLogStrategy>();
            }

            class LogMessage
            {
            public:
                LogMessage(LogLevel &level, std::string &src_name, int line_number, Logger &logger)
                    : _curr_time(GetTimeStamp()),
                      _level(level),
                      _pid(getpid()),
                      _src_name(src_name),
                      _line_number(line_number),
                      _logger(logger)
                {
                    std::stringstream ss;
                    ss << "[" << _curr_time << "] "
                       << "[" << Level2Str(_level) << "] "
                       << "[" << _pid << "] "
                       << "[" << _src_name << "] "
                       << "[" << _line_number << "] "
                       << "- ";
                    _loginfo = ss.str();
                }
                template <typename T>
                LogMessage &operator<<(const T &info)
                {
                    std::stringstream ss;
                    ss << info;
                    _loginfo += ss.str();
                    return *this;
                }

                ~LogMessage()
                {
                    if (_logger._fflush_strategy)
                    {
                        _logger._fflush_strategy->SyncLog(_loginfo);
                    }
                }

            private:
                std::string _curr_time;
                LogLevel _level;
                pid_t _pid;
                std::string _src_name;
                int _line_number;
                std::string _loginfo;
                Logger &_logger;
            };

            LogMessage operator()(LogLevel level, std::string name, int line)
            {
                return LogMessage(level, name, line, *this);
            }
            ~Logger()
            {
            }

        private:
            std::unique_ptr<LogStrategy> _fflush_strategy;
        };

        inline Logger logger;

        #define LOG(level) logger(level, __FILE__, __LINE__)
        #define Enable_Console_Log_Strategy() logger.EnableConsoleLogStrategy()
        #define Enable_File_Log_Strategy() logger.EnableFileLogStrategy()
        #define Enable_Null_Log_Strategy() logger.EnableNullLogStrategy()
    }

}
