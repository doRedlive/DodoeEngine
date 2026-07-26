// do@Redlive

#pragma once

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <string>
#include <thread>
#include <mutex>
#include <sstream>

namespace dodoe {

    using FloatingPointMicroseconds = std::chrono::duration<double, std::micro>;

    struct ProfileResult {
        String name;
        FloatingPointMicroseconds start;
        std::chrono::microseconds elapsed_time;
        std::thread::id thread_id;
    };

    struct InstrumentationSession {
        String name;
    };

    class Instrumentor {
        std::mutex m_mutex;
        InstrumentationSession* m_cur_session;
        std::ofstream m_output_stream;
    public:
        Instrumentor(const Instrumentor&) = delete;
        Instrumentor(Instrumentor&&) = delete;

        static Instrumentor& Self() {
            static Instrumentor instance;
            return instance;
        }

        void beginSession(const String& name, const String& file_path = "result.json") {
            std::lock_guard lock(m_mutex);
            if (m_cur_session) {
                std::cerr << "Instrumentor::beginSession error\n";
                internalEndSession();
            }

            m_output_stream.open(file_path);
            if (m_output_stream.is_open()) {
                m_cur_session = new InstrumentationSession({name});
                writeHeader();
            }
            else {
                std::cerr << "Instrumentor could not open results file " << file_path << std::endl;
            }
        }

        void endSession() {
            std::lock_guard lock(m_mutex);
            internalEndSession();
        }

        void writeProfile(const ProfileResult& result) {
            std::stringstream json;
			json << std::setprecision(3) << std::fixed;
			json << ",{";
			json << "\"cat\":\"function\",";
			json << "\"dur\":" << (result.elapsed_time.count()) << ',';
			json << "\"name\":\"" << result.name << "\",";
			json << "\"ph\":\"X\",";
			json << "\"pid\":0,";
			json << "\"tid\":" << result.thread_id << ",";
			json << "\"ts\":" << result.start.count();
			json << "}";

            std::lock_guard lock(m_mutex);
            if (m_cur_session) {
                m_output_stream << json.str();
                m_output_stream.flush();
            }
        }
    private:
        Instrumentor() : m_cur_session(nullptr) { }
        ~Instrumentor() { endSession(); }

        void internalEndSession() {
            if (m_cur_session) {
                writeFooter();
                m_output_stream.close();
                delete m_cur_session;
                m_cur_session = nullptr;
            }
        }

        void writeHeader() {
            m_output_stream << "{\"otherData\": {}, \"traceEvents\":[{}";
            m_output_stream.flush();
        }

        void writeFooter() {
            m_output_stream << "]}";
            m_output_stream.flush();
        }
        
    };

	class InstrumentationTimer {
	public:
		InstrumentationTimer(const char* name)
			: name_(name), stopped_(false) {
			start_timepoint_ = std::chrono::steady_clock::now();
		}

		~InstrumentationTimer() {
			if (!stopped_)
				stop();
		}

		void stop() {
			auto end_timepoint = std::chrono::steady_clock::now();
			auto high_res_start = FloatingPointMicroseconds{ start_timepoint_.time_since_epoch() };
			auto elapsed_time = std::chrono::time_point_cast<std::chrono::microseconds>(end_timepoint).time_since_epoch() - std::chrono::time_point_cast<std::chrono::microseconds>(start_timepoint_).time_since_epoch();

			Instrumentor::Self().writeProfile({ name_, high_res_start, elapsed_time, std::this_thread::get_id() });

			stopped_ = true;
		}

	private:
		const char* name_;
		std::chrono::time_point<std::chrono::steady_clock> start_timepoint_;
		bool stopped_;
	};

	namespace InstrumentorUtils {

		template <size_t N>
		struct ChangeResult {
			char Data[N];
		};

		template <size_t N, size_t K>
		consteval auto CleanupOutputString(const char(&expr)[N], const char(&remove)[K]) {
			ChangeResult<N> result = {};

			size_t src_index = 0;
			size_t dst_index = 0;
			while (src_index < N) {
				size_t match_index = 0;
				while (match_index < K - 1 && src_index + match_index < N - 1 && expr[src_index + match_index] == remove[match_index])
					match_index++;
				if (match_index == K - 1)
					src_index += match_index;
				result.Data[dst_index++] = expr[src_index] == '"' ? '\'' : expr[src_index];
				src_index++;
			}
			return result;
		}
	} // InstrumentorUtils
} // dodoe

#define DO_PROFILE 0
#if DO_PROFILE
	// Resolve which function signature macro will be used. Note that this only
	// is resolved when the (pre)compiler starts, so the syntax highlighting
	// could mark the wrong one in your editor!
	#if defined(__GNUC__) || (defined(__MWERKS__) && (__MWERKS__ >= 0x3000)) || (defined(__ICC) && (__ICC >= 600)) || defined(__ghs__)
		#define DO_FUNC_SIG __PRETTY_FUNCTION__
	#elif defined(__DMC__) && (__DMC__ >= 0x810)
		#define DO_FUNC_SIG __PRETTY_FUNCTION__
	#elif (defined(__FUNCSIG__) || (_MSC_VER))
		#define DO_FUNC_SIG __FUNCSIG__
	#elif (defined(__INTEL_COMPILER) && (__INTEL_COMPILER >= 600)) || (defined(__IBMCPP__) && (__IBMCPP__ >= 500))
		#define DO_FUNC_SIG __FUNCTION__
	#elif defined(__BORLANDC__) && (__BORLANDC__ >= 0x550)
		#define DO_FUNC_SIG __FUNC__
	#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
		#define DO_FUNC_SIG __func__
	#elif defined(__cplusplus) && (__cplusplus >= 201103)
		#define DO_FUNC_SIG __func__
	#else
		#define DO_FUNC_SIG "DO_FUNC_SIG unknown!"
	#endif

	#define DO_PROFILE_BEGIN_SESSION(name, filepath) ::dodoe::Instrumentor::Self().beginSession(name, filepath)
	#define DO_PROFILE_END_SESSION() ::dodoe::Instrumentor::Self().endSession()
	#define DO_PROFILE_SCOPE_LINE2(name, line) constexpr auto fixedName##line = ::dodoe::InstrumentorUtils::CleanupOutputString(name, "__cdecl ");\
										   ::dodoe::InstrumentationTimer timer##line(fixedName##line.Data)
	#define DO_PROFILE_SCOPE_LINE(name, line) DO_PROFILE_SCOPE_LINE2(name, line)
	#define DO_PROFILE_SCOPE(name) DO_PROFILE_SCOPE_LINE(name, __LINE__)
	#define DO_PROFILE_FUNCTION() DO_PROFILE_SCOPE(DO_FUNC_SIG)
#else
	#define DO_PROFILE_BEGIN_SESSION(name, filepath)
	#define DO_PROFILE_END_SESSION()
	#define DO_PROFILE_SCOPE(name)
	#define DO_PROFILE_FUNCTION()
#endif