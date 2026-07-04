// do@Redlive

#pragma once

#include "dopch.h"

#include <array>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace dodoe {

    template <typename T, Size_t Capacity>
    class SpscQueue {
        std::array<T, Capacity> m_buffer;
        Size_t m_head{0};
        Size_t m_tail{0};
        Size_t m_count{0};
        Bool m_closed{false};
        std::mutex m_mutex;
        std::condition_variable m_not_empty;
        std::condition_variable m_not_full;

    public:
        SpscQueue() = default;

        void push(T&& item) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_not_full.wait(lock, [this] { return m_count < Capacity || m_closed; });
            if (m_closed) return;
            m_buffer[m_tail] = std::move(item);
            m_tail = (m_tail + 1) % Capacity;
            ++m_count;
            m_not_empty.notify_one();
        }

        bool pop(T& item, const std::function<bool()>& should_exit) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_not_empty.wait(lock, [this, &should_exit] { return m_count > 0 || m_closed || should_exit(); });
            if (m_closed && m_count == 0) return false;
            if (m_count == 0 && should_exit()) return false;
            if (m_count == 0) return false;
            item = std::move(m_buffer[m_head]);
            m_head = (m_head + 1) % Capacity;
            --m_count;
            m_not_full.notify_one();
            return true;
        }

        bool tryPop(T& item) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_count == 0) return false;
            item = std::move(m_buffer[m_head]);
            m_head = (m_head + 1) % Capacity;
            --m_count;
            m_not_full.notify_one();
            return true;
        }

        void close() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_closed = true;
            m_not_empty.notify_all();
            m_not_full.notify_all();
        }
    };

} // dodoe
