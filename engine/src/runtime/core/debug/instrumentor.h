// do@Redlive

#pragma once

#include <chrono>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

#if defined(DODOE_TRACY_ENABLED) && DODOE_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#endif

namespace dodoe {

    class Instrumentor {
    public:
        Instrumentor(const Instrumentor&) = delete;
        Instrumentor(Instrumentor&&) = delete;

        static Instrumentor& Self() {
            static Instrumentor instance;
            return instance;
        }

        static std::int64_t nowMicroseconds() {
            static const auto process_start = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - process_start).count();
        }

        void beginSession(std::string_view name, std::string_view file_path = "DodoeProfile.json") {
            std::lock_guard lock(m_mutex);
            endSessionLocked();

            m_output_stream.open(std::string(file_path), std::ios::out | std::ios::trunc);
            if (!m_output_stream.is_open()) {
                return;
            }

            m_session_name.assign(name);
            m_has_written_event = false;
            writeHeaderLocked();
            writeThreadNameLocked("MainThread");
            m_output_stream.flush();
        }

        void endSession() {
            std::lock_guard lock(m_mutex);
            endSessionLocked();
        }

        void writeProfile(std::string_view name, std::string_view category,
                          std::int64_t start_time_us, std::int64_t duration_us) {
            std::lock_guard lock(m_mutex);
            if (!m_output_stream.is_open()) {
                return;
            }

            writeEventPrefixLocked();
            m_output_stream << "{\"cat\":\"" << escape(nameOr(category, "function"))
                            << "\",\"dur\":" << duration_us
                            << ",\"name\":\"" << escape(name)
                            << "\",\"ph\":\"X\",\"pid\":0,\"tid\":" << threadId()
                            << ",\"ts\":" << start_time_us << '}';
            if (category != "frame" && category != "task") {
                m_output_stream.flush();
            }
        }

        void writeInstant(std::string_view name, std::string_view category) {
            std::lock_guard lock(m_mutex);
            if (!m_output_stream.is_open()) {
                return;
            }

            writeEventPrefixLocked();
            m_output_stream << "{\"cat\":\"" << escape(nameOr(category, "function"))
                            << "\",\"name\":\"" << escape(name)
                            << "\",\"ph\":\"i\",\"pid\":0,\"s\":\"t\",\"tid\":"
                            << threadId() << ",\"ts\":" << nowMicroseconds() << '}';
            m_output_stream.flush();
        }

        void writeThreadName(std::string_view name) {
            std::lock_guard lock(m_mutex);
            if (!m_output_stream.is_open()) {
                return;
            }
            writeThreadNameLocked(name);
            m_output_stream.flush();
        }

    private:
        Instrumentor() = default;

        ~Instrumentor() {
            endSession();
        }

        static std::string_view nameOr(std::string_view value, std::string_view fallback) {
            return value.empty() ? fallback : value;
        }

        static std::uint64_t threadId() {
            return static_cast<std::uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }

        static std::string escape(std::string_view value) {
            std::string escaped;
            escaped.reserve(value.size());
            for (const char ch : value) {
                switch (ch) {
                case '\\': escaped += "\\\\"; break;
                case '\"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20) {
                        std::ostringstream stream;
                        stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                               << static_cast<int>(static_cast<unsigned char>(ch));
                        escaped += stream.str();
                    } else {
                        escaped += ch;
                    }
                    break;
                }
            }
            return escaped;
        }

        void endSessionLocked() {
            if (!m_output_stream.is_open()) {
                return;
            }
            m_output_stream << "]}";
            m_output_stream.close();
            m_session_name.clear();
            m_has_written_event = false;
        }

        void writeHeaderLocked() {
            m_output_stream << "{\"displayTimeUnit\":\"ms\",\"otherData\":{\"session\":\""
                            << escape(m_session_name) << "\"},\"traceEvents\":[";
        }

        void writeEventPrefixLocked() {
            if (m_has_written_event) {
                m_output_stream << ',';
            }
            m_has_written_event = true;
        }

        void writeThreadNameLocked(std::string_view name) {
            writeEventPrefixLocked();
            m_output_stream << "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":0,\"tid\":"
                            << threadId() << ",\"args\":{\"name\":\"" << escape(name) << "\"}}";
        }

        std::mutex m_mutex{};
        std::ofstream m_output_stream{};
        std::string m_session_name{};
        bool m_has_written_event{false};
    };

    class InstrumentationTimer {
    public:
        InstrumentationTimer(std::string_view name, std::string_view category)
            : m_name(name), m_category(category), m_start_time_us(Instrumentor::nowMicroseconds()) {
        }

        ~InstrumentationTimer() {
            stop();
        }

        InstrumentationTimer(const InstrumentationTimer&) = delete;
        InstrumentationTimer& operator=(const InstrumentationTimer&) = delete;

        void stop() {
            if (m_stopped) {
                return;
            }
            const std::int64_t end_time_us = Instrumentor::nowMicroseconds();
            Instrumentor::Self().writeProfile(m_name, m_category, m_start_time_us, end_time_us - m_start_time_us);
            m_stopped = true;
        }

    private:
        std::string m_name;
        std::string m_category;
        std::int64_t m_start_time_us{0};
        bool m_stopped{false};
    };

}

#if defined(DODOE_TRACY_ENABLED) && DODOE_TRACY_ENABLED
    #define DODOE_PROFILE_CONCAT_INNER(left, right) left##right
    #define DODOE_PROFILE_CONCAT(left, right) DODOE_PROFILE_CONCAT_INNER(left, right)
    #if defined(_MSC_VER)
        #define DODOE_PROFILE_FUNCTION_NAME __FUNCSIG__
    #else
        #define DODOE_PROFILE_FUNCTION_NAME __PRETTY_FUNCTION__
    #endif
    namespace dodoe {
        inline void TracySetThreadName(std::string_view name) {
            const std::string copy{name};
            tracy::SetThreadName(copy.c_str());
        }

        inline void TracyWriteMessage(std::string_view name, std::string_view category) {
            if (!name.empty()) {
                tracy::Profiler::LogString(
                    tracy::MessageSourceType::User,
                    tracy::MessageSeverity::Info,
                    0,
                    TRACY_CALLSTACK,
                    name.size(),
                    name.data());
            }
            (void)category;
        }

        class TracyScope {
        public:
            TracyScope(std::string_view name, std::string_view category,
                       uint32_t line, const char* file, const char* function)
                : m_zone(line, file, std::strlen(file), function, std::strlen(function),
                          nullptr, 0, TRACY_CALLSTACK, true) {
                m_zone.Name(name.data(), name.size());
                m_zone.Text(category.data(), category.size());
            }

            TracyScope(const TracyScope&) = delete;
            TracyScope& operator=(const TracyScope&) = delete;

        private:
            tracy::ScopedZone m_zone;
        };
    }
    #define DODOE_TRACY_SCOPE_IMPL(name, category, id) \
        ::dodoe::TracyScope DODOE_PROFILE_CONCAT(dodoe_tracy_scope_, id)((name), (category), __LINE__, TracyFile, TracyFunction)
    #define DO_PROFILE_BEGIN_SESSION(name, file_path)
    #define DO_PROFILE_END_SESSION()
    #define DO_PROFILE_THREAD_NAME(name) ::dodoe::TracySetThreadName((name))
    #define DO_PROFILE_MARK(name, category) ::dodoe::TracyWriteMessage((name), (category))
    #define DO_PROFILE_SCOPE_CATEGORY(name, category) DODOE_TRACY_SCOPE_IMPL((name), (category), __COUNTER__)
    #define DO_PROFILE_SCOPE(name) DO_PROFILE_SCOPE_CATEGORY((name), "function")
    #define DO_PROFILE_FUNCTION() DO_PROFILE_SCOPE(DODOE_PROFILE_FUNCTION_NAME)
    #define DO_PROFILE_FRAME() FrameMark
#elif defined(DODOE_PROFILE_ENABLED) && DODOE_PROFILE_ENABLED
    #define DODOE_PROFILE_CONCAT_INNER(left, right) left##right
    #define DODOE_PROFILE_CONCAT(left, right) DODOE_PROFILE_CONCAT_INNER(left, right)
    #if defined(_MSC_VER)
        #define DODOE_PROFILE_FUNCTION_NAME __FUNCSIG__
    #else
        #define DODOE_PROFILE_FUNCTION_NAME __PRETTY_FUNCTION__
    #endif
    #define DO_PROFILE_BEGIN_SESSION(name, file_path) ::dodoe::Instrumentor::Self().beginSession((name), (file_path))
    #define DO_PROFILE_END_SESSION() ::dodoe::Instrumentor::Self().endSession()
    #define DO_PROFILE_THREAD_NAME(name) ::dodoe::Instrumentor::Self().writeThreadName((name))
    #define DO_PROFILE_MARK(name, category) ::dodoe::Instrumentor::Self().writeInstant((name), (category))
    #define DO_PROFILE_SCOPE_CATEGORY(name, category) ::dodoe::InstrumentationTimer DODOE_PROFILE_CONCAT(dodoe_profile_timer_, __COUNTER__)((name), (category))
    #define DO_PROFILE_SCOPE(name) DO_PROFILE_SCOPE_CATEGORY((name), "function")
    #define DO_PROFILE_FUNCTION() DO_PROFILE_SCOPE(DODOE_PROFILE_FUNCTION_NAME)
    #define DO_PROFILE_FRAME()
#else
    #define DO_PROFILE_BEGIN_SESSION(name, file_path)
    #define DO_PROFILE_END_SESSION()
    #define DO_PROFILE_THREAD_NAME(name)
    #define DO_PROFILE_MARK(name, category)
    #define DO_PROFILE_SCOPE_CATEGORY(name, category)
    #define DO_PROFILE_SCOPE(name)
    #define DO_PROFILE_FUNCTION()
    #define DO_PROFILE_FRAME()
#endif
