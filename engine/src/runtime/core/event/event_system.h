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
        void initialize();
        void update();
        void shutdown();

        template<typename T, auto Method, typename Instance>
        void subscribe_event(Instance* instance) {
            event_dispatcher_->sink<T>().template connect<Method>(instance);
        }

        template<typename T, auto Method, typename Instance>
        void unsubscribe_event(Instance* instance) {
            static_assert(Method != nullptr, "Method must be a valid member function pointer");
            assert(event_dispatcher_ && "EventSystem not initialized");
            event_dispatcher_->sink<T>().template disconnect<Method>(instance);
        }

        template<typename T, auto Method, typename Instance>
        void unsubscribe_all(Instance* instance) {
            event_dispatcher_->sink<T>().disconnect<Method>(instance);
        }

        template<typename T, typename ...Args>
        void publish_event(Args&&... args) {
            assert(event_dispatcher_ && "EventSystem not initialized");
            event_dispatcher_->trigger<T>(std::forward<Args>(args)...);
        }

        template<typename T, typename ...Args>
        void enqueue_event(Args&&... args) {
            assert(event_dispatcher_ && "EventSystem not initialized");
            event_dispatcher_->enqueue<T>(std::forward<Args>(args)...);
        }

        void handle_events() const;

    private:
        Scope<entt::dispatcher> event_dispatcher_ {nullptr};
    };
} // dodoe

#endif //DODOE_EVENT_SYSTEM_H
