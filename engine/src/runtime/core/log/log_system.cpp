// do@Redlive

#include "log_system.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <atomic>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/base_sink.h"

namespace dodoe {

    namespace {

        std::atomic_uint64_t s_log_sequence{0};

    }

    template<typename Mutex>
    class MemorySink : public spdlog::sinks::base_sink<Mutex> {
    public:
        [[nodiscard]]
        DynamicArray<LogMessage> getLogs() {
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
            log_msg.content = String(formatted.begin(), formatted.end());
            log_msg.payload = String(msg.payload.begin(), msg.payload.end());
            log_msg.logger_name = String(msg.logger_name.begin(), msg.logger_name.end());
            log_msg.level = static_cast<LogLevel>(msg.level);
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
        DynamicArray<LogMessage> m_log_msg_buffer{};
    };

    using MemSinkMt = MemorySink<std::mutex>;

    Ref<spdlog::logger> Log::m_core_logger   = create_ref<spdlog::logger>("Engine");
    Ref<spdlog::logger> Log::m_client_logger = create_ref<spdlog::logger>("Client");
    LogLevel Log::m_core_level   = LogLevel::Trace;
    LogLevel Log::m_client_level = LogLevel::Debug;
    Ref<MemSinkMt> s_editor_console_sink = create_ref<MemSinkMt>();
    Ref<MemSinkMt> s_engine_console_sink = create_ref<MemSinkMt>();

    void Log::Initialize() {
        m_core_logger->set_pattern("%^[%T] [%l] %n: %v%$");
        m_client_logger->set_pattern("%^[%T] [%l] %n: %v%$");

        SetLoggerLevel(m_core_logger, LogLevel::Trace);
        SetLoggerLevel(m_client_logger, LogLevel::Debug);

        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto core_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("engine.log", true);
        auto client_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("client.log", true);

        console_sink->set_pattern("%^[%T] [%l] %n: %v%$");
        core_file_sink->set_pattern("[%T] [%l] %n: %v");
        client_file_sink->set_pattern("[%T] [%l] %n: %v");
        s_editor_console_sink->set_pattern("[%T] %n: %v");
        s_engine_console_sink->set_pattern("[%T] [%l] %n: %v");

        m_core_logger->sinks().clear();
        m_client_logger->sinks().clear();
        m_core_logger->sinks().push_back(console_sink);
        m_core_logger->sinks().push_back(core_file_sink);
        m_core_logger->sinks().push_back(std::shared_ptr<MemSinkMt>(s_engine_console_sink.get(), [](MemSinkMt*) {}));
        m_client_logger->sinks().push_back(console_sink);
        m_client_logger->sinks().push_back(client_file_sink);
        m_client_logger->sinks().push_back(std::shared_ptr<MemSinkMt>(s_editor_console_sink.get(), [](MemSinkMt*) {}));
        m_core_logger->flush_on(spdlog::level::debug);
        m_client_logger->flush_on(spdlog::level::debug);
    }

    void Log::SetLoggerLevel(const Ref<spdlog::logger>& logger, const LogLevel level) {
        logger->set_level(static_cast<spdlog::level::level_enum>(level));

        if (logger.get() == m_core_logger.get()) {
            m_core_level = level;
        } else if (logger.get() == m_client_logger.get()) {
            m_client_level = level;
        }
    }

    DynamicArray<LogMessage> Log::GetCoreLogs() {
        return s_engine_console_sink->getLogs();
    }

    DynamicArray<LogMessage> Log::GetClientLogs() {
        return s_editor_console_sink->getLogs();
    }

    void Log::ClearCoreLogs() {
        s_engine_console_sink->clearLogs();
    }

    void Log::ClearClientLogs() {
        s_editor_console_sink->clearLogs();
    }

} // dodoe
