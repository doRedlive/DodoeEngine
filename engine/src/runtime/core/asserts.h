//
// Created by GreenMuffin on 2026/3/5.
//

#ifndef DODOE_ASSERT_H
#define DODOE_ASSERT_H

#include "core.h"

#include <filesystem>

#ifdef DO_ENABLE_ASSERTS

    #define DO_INTERNAL_ASSERT_IMPL(type, check, msg, ...) \
        {                                                  \
            if (!(check))                                  \
            {                                              \
                type##Error(msg, __VA_ARGS__);             \
                DoDebugBreak();                            \
            }                                              \
        }
    #define DO_INTERNAL_ASSERT_WITH_MSG(type, check, ...) \
        DO_INTERNAL_ASSERT_IMPL(type, check, "Assertion failed: {0}", __VA_ARGS__)
    #define DO_INTERNAL_ASSERT_NO_MSG(type, check) \
        DO_INTERNAL_ASSERT_IMPL(type, check, "Assertion '{0}' failed at {1}:{2}", DO_STRINGIFY_MACRO(check), std::filesystem::path(__FILE__).filename().string(), __LINE__)

    #define DO_INTERNAL_ASSERT_GET_MACRO_NAME(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, macro, ...) macro
    #define DO_INTERNAL_ASSERT_GET_MACRO(...) DO_EXPAND_MACRO( DO_INTERNAL_ASSERT_GET_MACRO_NAME(__VA_ARGS__, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_WITH_MSG, DO_INTERNAL_ASSERT_NO_MSG) )

    #define DO_ASSERT(...) DO_EXPAND_MACRO( DO_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(Do, __VA_ARGS__) )
    #define InAssert(...) DO_EXPAND_MACRO( DO_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(Log, __VA_ARGS__))

#else

    #define DO_ASSERT(...)
    #define InAssert(...)

#endif//DO_ENABLE_ASSERTS

#endif//DODOE_ASSERT_H