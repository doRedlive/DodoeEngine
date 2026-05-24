// do@Redlive

#include "log_system.h"

#include <atomic>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/base_sink.h"

namespace dodoe {

    namespace {

        std::atomic_uint64_t s_log_sequence{0};

    }

    const auto kLogLevelUmap = std::unordered_map<spdlog::level::level_enum, LogLevel>{
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
        std::vector<LogMessage> getLogs() {
            std::lock_guard<Mutex> lock(this->mutex_);
            return m_log_msg_buffer;
        }

        void clearLogs() {
            std::lock_guard<Mutex> lock(this->mutex_);
            m_log_msg_buffer.clear();
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override {
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

            const auto sequence = ++s_log_sequence;

            LogMessage log_msg;
            log_msg.content = std::string(formatted.begin(), formatted.end());
            log_msg.payload = std::string(msg.payload.begin(), msg.payload.end());
            log_msg.logger_name = std::string(msg.logger_name.begin(), msg.logger_name.end());
            log_msg.level = kLogLevelUmap.at(msg.level);
            log_msg.sequence = sequence;

            if (!m_log_msg_buffer.empty()) {
                auto& last_log = m_log_msg_buffer.back();
                if (last_log.level == log_msg.level
                    && last_log.payload == log_msg.payload
                    && last_log.logger_name == log_msg.logger_name) {
                    last_log.content = std::move(log_msg.content);
                    last_log.sequence = log_msg.sequence;
                    ++last_log.repeat_count;
                    return;
                }
            }

            m_log_msg_buffer.push_back(std::move(log_msg));
        }

        void flush_() override {}

    private:
        std::vector<LogMessage> m_log_msg_buffer{};
    };

    using MemSinkMt = MemorySink<std::mutex>;

    Ref<spdlog::logger> Log::m_core_logger   = spdlog::stdout_color_mt("Engine");
    Ref<spdlog::logger> Log::m_client_logger = spdlog::stdout_color_mt("Client");
    Ref<MemSinkMt> s_editor_console_sink = create_ref<MemSinkMt>();
    Ref<MemSinkMt> s_engine_console_sink = create_ref<MemSinkMt>();

    void Log::Initialize() {
        m_core_logger->set_pattern("%^[%T] %n: %v%$");
        m_client_logger->set_pattern("%^[%T] [%l] %n: %v%$");

        SetLoggerLevel(m_core_logger, LogLevel::Trace);
        SetLoggerLevel(m_client_logger, LogLevel::Debug);

        const auto console_sink = create_ref<spdlog::sinks::stdout_color_sink_mt>();
        const auto core_file_sink = create_ref<spdlog::sinks::basic_file_sink_mt>("engine.log", true);
        const auto client_file_sink = create_ref<spdlog::sinks::basic_file_sink_mt>("client.log", true);

        console_sink->set_pattern("%^[%T] %n: %v%$");
        core_file_sink->set_pattern("[%T] [%l] %n: %v");
        client_file_sink->set_pattern("[%T] [%l] %n: %v");
        s_editor_console_sink->set_pattern("[%T] %n: %v");
        s_engine_console_sink->set_pattern("[%T] [%l] %n: %v");

        m_core_logger->sinks().clear();
        m_client_logger->sinks().clear();
        m_core_logger->sinks().push_back(console_sink);
        m_core_logger->sinks().push_back(core_file_sink);
        m_core_logger->sinks().push_back(s_engine_console_sink);
        m_client_logger->sinks().push_back(console_sink);
        m_client_logger->sinks().push_back(client_file_sink);
        m_client_logger->sinks().push_back(s_editor_console_sink);
    }

    void Log::SetLoggerLevel(const Ref<spdlog::logger>& logger, const LogLevel level) {
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

    std::vector<LogMessage> Log::GetCoreLogs() {
        return s_engine_console_sink->getLogs();
    }

    std::vector<LogMessage> Log::GetClientLogs() {
        return s_editor_console_sink->getLogs();
    }

    void Log::ClearCoreLogs() {
        s_engine_console_sink->clearLogs();
    }

    void Log::ClearClientLogs() {
        s_editor_console_sink->clearLogs();
    }

} // dodoe
