//
// Created by GreenMuffin on 2025/10/7.
//

#include "log_system.h"

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/base_sink.h"

namespace dodoe {

    const auto log_level_map = std::unordered_map<spdlog::level::level_enum, LogLevel>{
        {spdlog::level::trace, LogLevel::Trace},
        {spdlog::level::debug, LogLevel::Debug},
        {spdlog::level::info, LogLevel::Info},
        {spdlog::level::warn, LogLevel::Warn},
        {spdlog::level::err, LogLevel::Error},
        {spdlog::level::critical, LogLevel::Critical},
    };

    template<typename Mutex>
    class MemorySink : public spdlog::sinks::base_sink<Mutex> {
    public:
        [[nodiscard]] 
        const std::vector<LogMessage>& get_all_logs() {
            std::lock_guard<Mutex> lock(this->mutex_);
            return log_msg_buffer_;
        }

        void clear_logs() {
            std::lock_guard<Mutex> lock(this->mutex_);
            log_msg_buffer_.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

            LogMessage log_msg;
            log_msg.content = std::string(formatted.begin(), formatted.end());
            log_msg.level = log_level_map.at(msg.level);
            log_msg_buffer_.push_back(log_msg);
        }

        void flush_() override {}

    private:
        std::vector<LogMessage> log_msg_buffer_{};
    };

    using memory_sink_mt = MemorySink<std::mutex>;

    Ref<spdlog::logger> Log::core_logger_   = spdlog::stdout_color_mt("Dodoe");
    Ref<spdlog::logger> Log::client_logger_ = spdlog::stdout_color_mt("Client");
    Ref<memory_sink_mt> s_editor_console_sink = create_ref<memory_sink_mt>();

    void Log::initialize() {
        core_logger_->set_pattern("%^[%T] %n: %v%$");
        client_logger_->set_pattern("%^[%T] [%l] %n: %v%$");

        set_logger_level(core_logger_, LogLevel::Trace);
        set_logger_level(client_logger_, LogLevel::Debug);

        const auto console_sink = create_ref<spdlog::sinks::stdout_color_sink_mt>();
        const auto core_file_sink = create_ref<spdlog::sinks::basic_file_sink_mt>("dodoe.log", true);
        const auto client_file_sink = create_ref<spdlog::sinks::basic_file_sink_mt>("client.log", true);

        console_sink->set_pattern("%^[%T] %n: %v%$");
        core_file_sink->set_pattern("[%T] [%l] %n: %v");
        client_file_sink->set_pattern("[%T] [%l] %n: %v");
        s_editor_console_sink->set_pattern("[%T] %n: %v");

        core_logger_->sinks().clear();
        client_logger_->sinks().clear();
        core_logger_->sinks().push_back(console_sink);
        core_logger_->sinks().push_back(core_file_sink);
        client_logger_->sinks().push_back(console_sink);
        client_logger_->sinks().push_back(client_file_sink);
        client_logger_->sinks().push_back(s_editor_console_sink);
    }

    void Log::set_logger_level(const Ref<spdlog::logger>& logger, const LogLevel level) {
        switch (level) {
            case LogLevel::Trace:
                logger->set_level(spdlog::level::trace);
                break;
            case LogLevel::Debug:
                logger->set_level(spdlog::level::debug);
                break;
            case LogLevel::Info:
                logger->set_level(spdlog::level::info);
                break;
            case LogLevel::Warn:
                logger->set_level(spdlog::level::warn);
                break;
            case LogLevel::Error:
                logger->set_level(spdlog::level::err);
                break;
            case LogLevel::Critical:
                logger->set_level(spdlog::level::critical);
                break;
            default: ;
        }
    }

    const std::vector<LogMessage>& Log::get_all_client_logs() {
        return s_editor_console_sink->get_all_logs();
    }
}
