//
// Created by GreenMuffin on 2026/2/3.
//

#ifndef DODOE_SINGLETON_H
#define DODOE_SINGLETON_H

#include "dopch.h"

namespace dodoe {

    template <typename T>
    class Singleton {
    public:
        static T& instance() {
            if (!is_initialized_) {
                DoError("The singleton {} not initialized!", typeid(T).name());
                std::abort();       // TODO: FIXME;
            }
            return *instance_;
        }

        virtual ~Singleton() = default;
        Singleton(const Singleton&) = delete;
        Singleton& operator=(const Singleton&) = delete;

        template <typename... Args>
        static void initialize(Args&&... args) {
            if (is_initialized_) return;
            is_initialized_ = true;
            instance_ = Scope<T>(new T(std::forward<Args>(args)...));
        }

        static void shutdown() {
            is_initialized_ = false;
            instance_.reset();
        }

    protected:
        inline static Scope<T> instance_;
        inline static bool is_initialized_{false};

        Singleton() = default;
    };
} // dodoe

#endif // !DODOE_SINGLETON_H
