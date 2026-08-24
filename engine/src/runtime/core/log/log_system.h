// do@Redlive

#pragma once

#include "dopch.h"

#include <spdlog/spdlog.h>

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
        String content;
        String payload;
        String logger_name;
        LogLevel level{ LogLevel::Trace };
        uint32_t repeat_count{1};
        uint64_t sequence{0};
    };

    inline StringView GetModuleName(StringView source_file) {
        if (source_file.empty()) {
            return "Unknown";
        }
        const Size_t separator = source_file.find_last_of("\\/");
        const StringView file_name = separator == StringView::npos
            ? source_file
            : source_file.substr(separator + 1);
        const Size_t extension = file_name.find_last_of('.');
        return extension == StringView::npos ? file_name : file_name.substr(0, extension);
    }

    class DODOE_API Log {
    public:
        static void Initialize();

        static void SetLoggerLevel(const Ref<spdlog::logger>& logger, LogLevel level);

        static DynamicArray<LogMessage> GetCoreLogs();
        static DynamicArray<LogMessage> GetClientLogs();
        static void ClearClientLogs();
        static void ClearCoreLogs();

        static bool ShouldCoreLog(LogLevel level) { return level >= m_core_level; }
        static bool ShouldClientLog(LogLevel level) { return level >= m_client_level; }

        template <typename... Args>
        static void CoreLog(LogLevel level, StringView source_file, fmt::format_string<Args...> fmt, Args&&... args) {
            if (!ShouldCoreLog(level)) return;
            m_core_logger->log(static_cast<spdlog::level::level_enum>(level), "[{}] {}", GetModuleName(source_file), fmt::format(fmt, std::forward<Args>(args)...));
        }

        template <typename... Args>
        static void ClientLog(LogLevel level, StringView source_file, fmt::format_string<Args...> fmt, Args&&... args) {
            if (!ShouldClientLog(level)) return;
            m_client_logger->log(static_cast<spdlog::level::level_enum>(level), "[{}] {}", GetModuleName(source_file), fmt::format(fmt, std::forward<Args>(args)...));
        }

    private:
        static Ref<spdlog::logger> m_core_logger;
        static Ref<spdlog::logger> m_client_logger;
        static LogLevel m_core_level;
        static LogLevel m_client_level;
    };

} // dodoe

#define DO_CORE_LOG(level, ...) dodoe::Log::CoreLog(dodoe::LogLevel::level, __FILE__, __VA_ARGS__)
#define DO_TRACE(...) DO_CORE_LOG(Trace, __VA_ARGS__)
#define DO_DEBUG(...) DO_CORE_LOG(Debug, __VA_ARGS__)
#define DO_INFO(...) DO_CORE_LOG(Info, __VA_ARGS__)
#define DO_WARN(...) DO_CORE_LOG(Warn, __VA_ARGS__)
#define DO_ERROR(...) DO_CORE_LOG(Error, __VA_ARGS__)
#define DO_CRITICAL(...) DO_CORE_LOG(Critical, __VA_ARGS__)

#define DO_CLIENT_LOG(level, ...) dodoe::Log::ClientLog(dodoe::LogLevel::level, __FILE__, __VA_ARGS__)
#define LOG_TRACE(...) DO_CLIENT_LOG(Trace, __VA_ARGS__)
#define LOG_DEBUG(...) DO_CLIENT_LOG(Debug, __VA_ARGS__)
#define LOG_INFO(...) DO_CLIENT_LOG(Info, __VA_ARGS__)
#define LOG_WARN(...) DO_CLIENT_LOG(Warn, __VA_ARGS__)
#define LOG_ERROR(...) DO_CLIENT_LOG(Error, __VA_ARGS__)
#define LOG_CRITICAL(...) DO_CLIENT_LOG(Critical, __VA_ARGS__)
