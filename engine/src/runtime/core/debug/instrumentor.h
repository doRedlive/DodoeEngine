// do@Redlive

#pragma once

#include <cstring>
#include <string>
#include <string_view>

#if defined(DODOE_TRACY_ENABLED) && DODOE_TRACY_ENABLED
#include <tracy/Tracy.hpp>
#endif

#if defined(DODOE_TRACY_ENABLED) && DODOE_TRACY_ENABLED

namespace dodoe {

    inline void TracySetThreadName(std::string_view name) {
        const std::string copy(name.data(), name.size());
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
        tracy::ScopedZone m_zone;
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
    };

}

    #define DODOE_PROFILE_CONCAT_INNER(left, right) left##right
    #define DODOE_PROFILE_CONCAT(left, right) DODOE_PROFILE_CONCAT_INNER(left, right)
    #if defined(_MSC_VER)
        #define DODOE_PROFILE_FUNCTION_NAME __FUNCSIG__
    #else
        #define DODOE_PROFILE_FUNCTION_NAME __PRETTY_FUNCTION__
    #endif
    #define DO_PROFILE_THREAD_NAME(name) ::dodoe::TracySetThreadName((name))
    #define DO_PROFILE_MARK(name, category) ::dodoe::TracyWriteMessage((name), (category))
    #define DO_PROFILE_SCOPE_CATEGORY(name, category) \
        ::dodoe::TracyScope DODOE_PROFILE_CONCAT(dodoe_tracy_scope_, __COUNTER__)((name), (category), __LINE__, TracyFile, TracyFunction)
    #define DO_PROFILE_SCOPE(name) DO_PROFILE_SCOPE_CATEGORY((name), "function")
    #define DO_PROFILE_FUNCTION() DO_PROFILE_SCOPE(DODOE_PROFILE_FUNCTION_NAME)
    #define DO_PROFILE_FRAME() FrameMark
#else
    #define DO_PROFILE_THREAD_NAME(name)
    #define DO_PROFILE_MARK(name, category)
    #define DO_PROFILE_SCOPE_CATEGORY(name, category)
    #define DO_PROFILE_SCOPE(name)
    #define DO_PROFILE_FUNCTION()
    #define DO_PROFILE_FRAME()
#endif
