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
        static void poll_events();
        static void shutdown();

        template<typename T, auto Method, typename Instance>
        static void subscribe_event(Instance* instance) {
            dispatcher_().sink<T>().template connect<Method>(instance);
        }

        template<typename T, auto Method, typename Instance>
        static void unsubscribe_event(Instance* instance) {
            static_assert(Method != nullptr, "Method must be a valid member function pointer");
            dispatcher_().sink<T>().template disconnect<Method>(instance);
        }

        template<typename T, auto Method, typename Instance>
        static void unsubscribe_all(Instance* instance) {
            dispatcher_().sink<T>().disconnect<Method>(instance);
        }

        template<typename T, typename ...Args>
        static void publish_event(Args&&... args) {
            dispatcher_().trigger<T>(std::forward<Args>(args)...);
        }

        template<typename T, typename ...Args>
        static void enqueue_event(Args&&... args) {
            dispatcher_().enqueue<T>(std::forward<Args>(args)...);
        }

        static void handle_events();

    private:
        static entt::dispatcher& dispatcher_();
    };
} // dodoe

#endif //DODOE_EVENT_SYSTEM_H
