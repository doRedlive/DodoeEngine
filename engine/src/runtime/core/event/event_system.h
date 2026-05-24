// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/event/event.h"

#include "entt/entt.hpp"

namespace dodoe {
    class EventSystem {
    public:
        static void Initialize();
        static void Shutdown();
        static void Poll();

        template<typename T, auto Method, typename Instance>
        static void Subscribe(Instance* instance) {
            static_assert(Method != nullptr, "Method must be a valid member function pointer!");
            GetDispatcher().sink<T>().template connect<Method>(instance);
        }

        template<typename T, auto Method, typename Instance>
        static void Unsubscribe(Instance* instance) {
            static_assert(Method != nullptr, "Method must be a valid member function pointer!");
            GetDispatcher().sink<T>().template disconnect<Method>(instance);
        }

        template<typename T, typename ...Args>
        static void Publish(Args&&... args) {
            GetDispatcher().trigger<T>(std::forward<Args>(args)...);
        }

        template<typename T, typename ...Args>
        static void Enqueue(Args&&... args) {
            GetDispatcher().enqueue<T>(std::forward<Args>(args)...);
        }

        static void Handle();

    private:
        static entt::dispatcher& GetDispatcher();
    };
} // dodoe
