// do@Redlive

#pragma once

#include "dopch.h"

// fmt provides std::format-compatible formatting WITHOUT pulling in windows.h
#include <spdlog/fmt/bundled/format.h>

#include <memory>

namespace spdlog {
    class logger;
}

namespace dodoe {

    enum class LogLevel {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Critical,
    };

    struct LogMessage {
        std::string content;
        std::string payload;
        std::string logger_name;
        LogLevel level{ LogLevel::Trace };
        uint32_t repeat_count{1};
        uint64_t sequence{0};
    };

    class Log {
    public:
        static void Initialize();

        static void SetLoggerLevel(const std::shared_ptr<spdlog::logger>& logger, LogLevel level);

        static std::vector<LogMessage> GetCoreLogs();
        static std::vector<LogMessage> GetClientLogs();
        static void ClearClientLogs();
        static void ClearCoreLogs();

        static bool ShouldCoreLog(LogLevel level) { return level >= m_core_level; }
        static bool ShouldClientLog(LogLevel level) { return level >= m_client_level; }

        static void CoreLog(LogLevel level, std::string_view message);
        static void ClientLog(LogLevel level, std::string_view message);

    private:
        static std::shared_ptr<spdlog::logger> m_core_logger;
        static std::shared_ptr<spdlog::logger> m_client_logger;
        static LogLevel m_core_level;
        static LogLevel m_client_level;
    };

} // dodoe

#define DO_TRACE(...) \
    do { \
        if (dodoe::Log::ShouldCoreLog(dodoe::LogLevel::Trace)) \
            dodoe::Log::CoreLog(dodoe::LogLevel::Trace, fmt::format(__VA_ARGS__)); \
    } while(0)
#define DO_DEBUG(...) \
    do { \
        if (dodoe::Log::ShouldCoreLog(dodoe::LogLevel::Debug)) \
            dodoe::Log::CoreLog(dodoe::LogLevel::Debug, fmt::format(__VA_ARGS__)); \
    } while(0)
#define DO_INFO(...) \
    do { \
        if (dodoe::Log::ShouldCoreLog(dodoe::LogLevel::Info)) \
            dodoe::Log::CoreLog(dodoe::LogLevel::Info, fmt::format(__VA_ARGS__)); \
    } while(0)
#define DO_WARN(...) \
    do { \
        if (dodoe::Log::ShouldCoreLog(dodoe::LogLevel::Warn)) \
            dodoe::Log::CoreLog(dodoe::LogLevel::Warn, fmt::format(__VA_ARGS__)); \
    } while(0)
#define DO_ERROR(...) \
    do { \
        if (dodoe::Log::ShouldCoreLog(dodoe::LogLevel::Error)) \
            dodoe::Log::CoreLog(dodoe::LogLevel::Error, fmt::format(__VA_ARGS__)); \
    } while(0)
#define DO_CRITICAL(...) \
    do { \
        if (dodoe::Log::ShouldCoreLog(dodoe::LogLevel::Critical)) \
            dodoe::Log::CoreLog(dodoe::LogLevel::Critical, fmt::format(__VA_ARGS__)); \
    } while(0)

#define LOG_TRACE(...) \
    do { \
        if (dodoe::Log::ShouldClientLog(dodoe::LogLevel::Trace)) \
            dodoe::Log::ClientLog(dodoe::LogLevel::Trace, fmt::format(__VA_ARGS__)); \
    } while(0)
#define LOG_DEBUG(...) \
    do { \
        if (dodoe::Log::ShouldClientLog(dodoe::LogLevel::Debug)) \
            dodoe::Log::ClientLog(dodoe::LogLevel::Debug, fmt::format(__VA_ARGS__)); \
    } while(0)
#define LOG_INFO(...) \
    do { \
        if (dodoe::Log::ShouldClientLog(dodoe::LogLevel::Info)) \
            dodoe::Log::ClientLog(dodoe::LogLevel::Info, fmt::format(__VA_ARGS__)); \
    } while(0)
#define LOG_WARN(...) \
    do { \
        if (dodoe::Log::ShouldClientLog(dodoe::LogLevel::Warn)) \
            dodoe::Log::ClientLog(dodoe::LogLevel::Warn, fmt::format(__VA_ARGS__)); \
    } while(0)
#define LOG_ERROR(...) \
    do { \
        if (dodoe::Log::ShouldClientLog(dodoe::LogLevel::Error)) \
            dodoe::Log::ClientLog(dodoe::LogLevel::Error, fmt::format(__VA_ARGS__)); \
    } while(0)
#define LOG_CRITICAL(...) \
    do { \
        if (dodoe::Log::ShouldClientLog(dodoe::LogLevel::Critical)) \
            dodoe::Log::ClientLog(dodoe::LogLevel::Critical, fmt::format(__VA_ARGS__)); \
    } while(0)
