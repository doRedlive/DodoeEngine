// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/base.h"

#include <functional>

namespace dodoe {

    class DeferredDeletionQueue {
        struct Entry {
            std::function<void()> deleter;
            UInt64 frame_number;
        };

        DynamicArray<Entry> m_queue{};

    public:
        template <typename T>
        void enqueue(Scope<T> resource, UInt64 frame_number) {
            m_queue.push_back({
                [res = std::move(resource)]() mutable { res.reset(); },
                frame_number
            });
        }

        void enqueueFunc(std::function<void()> deleter, UInt64 frame_number) {
            m_queue.push_back({std::move(deleter), frame_number});
        }

        void processCompleted(UInt64 last_completed_frame);

        [[nodiscard]] Size_t getPendingCount() const { return m_queue.size(); }
        [[nodiscard]] Bool isEmpty() const { return m_queue.empty(); }

        void clear();
    };

} // namespace dodoe
