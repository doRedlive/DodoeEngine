// do@Redlive

#pragma once

#include "runtime/platform/platform_detection.h"

#ifdef DO_DEBUG
    #if defined(DO_PLATFORM_WINDOWS)
        #include <intrin.h>
        #define DoDebugBreak() __debugbreak()
    #elif defined(DO_PLATFORM_LINUX)
        #include <signal.h>
        #define DoDebugBreak() raise(SIGTRAP)
    #elif defined(DO_PLATFORM_MACOS)
        #define DoDebugBreak() __builtin_trap()
    #endif

    #define DO_ENABLE_ASSERTS
#else
    #define DoDebugBreak();
#endif//DO_DEBUG

#define DO_EXPAND_MACRO(x) x
#define DO_STRINGIFY_MACRO(x) #x
