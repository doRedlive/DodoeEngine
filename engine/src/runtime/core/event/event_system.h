//
// Created by GreenMuffin on 2025/11/24.
//

#ifndef DODOE_EVENT_SYSTEM_H
#define DODOE_EVENT_SYSTEM_H

#include "dopch.h"

#include "runtime/core/event/event.h"

#include "entt/entt.hpp"

namespace dodoe {
    class EventSystem {
    public:
        EventSystem() = delete;
        ~EventSystem() = delete;

        static void initialize();
        static void Poll();
        static void shutdown();

        template<typename T, auto Method, typename Instance>
        static void Subscribe(Instance* instance) {
            if (!initialized_()) {
                return;
            }
            dispatcher_().sink<T>().template connect<Method>(instance);
        }

        template<typename T, auto Method, typename Instance>
        static void Unsubscribe(Instance* instance) {
            static_assert(Method != nullptr, "Method must be a valid member function pointer");
            if (!initialized_()) {
                return;
            }
            dispatcher_().sink<T>().template disconnect<Method>(instance);
        }

        template<typename T, auto Method, typename Instance>
        static void unsubscribe_all(Instance* instance) {
            if (!initialized_()) {
                return;
            }
            dispatcher_().sink<T>().disconnect<Method>(instance);
        }

        template<typename T, typename ...Args>
        static void Publish(Args&&... args) {
            if (!initialized_()) {
                return;
            }
            dispatcher_().trigger<T>(std::forward<Args>(args)...);
        }

        template<typename T, typename ...Args>
        static void enqueueEvent(Args&&... args) {
            if (!initialized_()) {
                return;
            }
            dispatcher_().enqueue<T>(std::forward<Args>(args)...);
        }

        static void Handle();

    private:
        static bool initialized_();
        static entt::dispatcher& dispatcher_();
    };
} // dodoe

#endif //DODOE_EVENT_SYSTEM_H
