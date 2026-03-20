//
// Created by GreenMuffin on 2026/3/5.
//

#ifndef DODOE_PLATFORM_DETECTION_H
#define DODOE_PLATFORM_DETECTION_H

#ifdef _WIN32
    #ifdef _WIN64
        #define DO_PLATFORM_WINDOWS
    #else
        #error "x86 builds are not supported!"
    #endif
#elif defined(__APPLE__) || defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE == 1
        #define DO_PLATFORM_IOS
        #error "ios simulator is not supportd!"
    #elif TARGET_OS_MAC == 1
        #define DO_PLATFORM_MACOS
    #else
        #error "unknown apple platform!"
    #endif
#elif defined(__ANDROID__)
    #define DO_PLATFORM_ANDROID
    #error "android is not supported!"
#elif defined(__linux__)
    #define DO_PLATFORM_LINUX
#else
    #error "unknown platform"
#endif//Platform detection


#endif//DODOE_PLATFORM_DETECTION_H