//
// Created by GreenMuffin on 2025/10/7.
//

#ifndef DODOE_LOG_H
#define DODOE_LOG_H

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
        LogLevel level{ LogLevel::Trace };
    };

    class Log {
    public:
        static Ref<spdlog::logger> get_core_logger()   { return core_logger_; }
        static Ref<spdlog::logger> get_client_logger() { return client_logger_; }
        static void initialize();
        static void set_logger_level(const Ref<spdlog::logger>& logger, LogLevel level);
        static const std::vector<LogMessage>& get_all_client_logs();
        static const std::vector<LogMessage>& getCoreLogs();

    private:
        static Ref<spdlog::logger> core_logger_;
        static Ref<spdlog::logger> client_logger_;
    };


#define DoTrace(...)       dodoe::Log::get_core_logger()->trace(__VA_ARGS__)
#define DoDebug(...)       dodoe::Log::get_core_logger()->debug(__VA_ARGS__)
#define DoInfo(...)        dodoe::Log::get_core_logger()->info(__VA_ARGS__)
#define DoWarn(...)        dodoe::Log::get_core_logger()->warn(__VA_ARGS__)
#define DO_ERROR(...)       dodoe::Log::get_core_logger()->error(__VA_ARGS__)
#define DoCritical(...)    dodoe::Log::get_core_logger()->critical(__VA_ARGS__)

#define LogTrace(...)      dodoe::Log::get_client_logger()->trace(__VA_ARGS__)
#define LogDebug(...)      dodoe::Log::get_client_logger()->debug(__VA_ARGS__)
#define LOG_INFO(...)       dodoe::Log::get_client_logger()->info(__VA_ARGS__)
#define LogWarn(...)       dodoe::Log::get_client_logger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)      dodoe::Log::get_client_logger()->error(__VA_ARGS__)
#define LogCritical(...)   dodoe::Log::get_client_logger()->critical(__VA_ARGS__)
    
}


#endif //DODOE_LOG_H