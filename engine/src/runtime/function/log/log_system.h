// do@Redlive

#pragma once

#include "dopch.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

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

        static Ref<spdlog::logger> GetCoreLogger()   { return m_core_logger; }
        static Ref<spdlog::logger> GetClientLogger() { return m_client_logger; }

        static void SetLoggerLevel(const Ref<spdlog::logger>& logger, LogLevel level);

        static std::vector<LogMessage> GetCoreLogs();
        static std::vector<LogMessage> GetClientLogs();
        static void ClearClientLogs();
        static void ClearCoreLogs();

    private:
        static Ref<spdlog::logger> m_core_logger;
        static Ref<spdlog::logger> m_client_logger;
    };


#define DO_TRACE(...)       dodoe::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define DO_DEBUG(...)       dodoe::Log::GetCoreLogger()->debug(__VA_ARGS__)
#define DO_INFO(...)        dodoe::Log::GetCoreLogger()->info(__VA_ARGS__)
#define DO_WARN(...)        dodoe::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define DO_ERROR(...)       dodoe::Log::GetCoreLogger()->error(__VA_ARGS__)
#define DO_CRITICAL(...)    dodoe::Log::GetCoreLogger()->critical(__VA_ARGS__)

#define LOG_TRACE(...)      dodoe::Log::GetClientLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)      dodoe::Log::GetClientLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)       dodoe::Log::GetClientLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)       dodoe::Log::GetClientLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)      dodoe::Log::GetClientLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...)   dodoe::Log::GetClientLogger()->critical(__VA_ARGS__)
    
} // dodoe
