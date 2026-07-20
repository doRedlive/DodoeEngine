// do@Redlive

#pragma once

#include "dopch.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <thread>

namespace dodoe {

    template <typename T, Size_t Capacity>
    class SpscQueue {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

        static constexpr Size_t kMask = Capacity - 1;

        std::array<T, Capacity> m_buffer{};
        alignas(64) std::atomic<Size_t> m_head{0};
        alignas(64) std::atomic<Size_t> m_tail{0};
        std::atomic<Bool> m_closed{false};
        std::mutex m_block_mutex{};
        std::condition_variable m_block_cv{};

        Size_t size() const {
            return m_tail.load(std::memory_order_acquire) - m_head.load(std::memory_order_acquire);
        }

        Bool isFull() const {
            return size() >= Capacity;
        }

        Bool isEmpty() const {
            return m_head.load(std::memory_order_acquire) == m_tail.load(std::memory_order_acquire);
        }

    public:
        SpscQueue() = default;

        void push(T&& item) {
            while (isFull()) {
                if (m_closed.load(std::memory_order_relaxed)) {
                    return;
                }
                std::unique_lock<std::mutex> lock(m_block_mutex);
                m_block_cv.wait_for(lock, std::chrono::microseconds(100), [this] {
                    return !isFull() || m_closed.load(std::memory_order_relaxed);
                });
            }
            Size_t tail = m_tail.load(std::memory_order_relaxed);
            m_buffer[tail & kMask] = std::move(item);
            m_tail.store(tail + 1, std::memory_order_release);
        }

        bool pop(T& item, const std::function<bool()>& should_exit) {
            while (isEmpty()) {
                if (m_closed.load(std::memory_order_relaxed)) {
                    return false;
                }
                if (should_exit()) {
                    return false;
                }
                std::unique_lock<std::mutex> lock(m_block_mutex);
                m_block_cv.wait_for(lock, std::chrono::microseconds(100), [this, &should_exit] {
                    return !isEmpty() || m_closed.load(std::memory_order_relaxed) || should_exit();
                });
            }
            Size_t head = m_head.load(std::memory_order_relaxed);
            item = std::move(m_buffer[head & kMask]);
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        bool tryPush(const T& item) {
            if (m_closed.load(std::memory_order_relaxed)) {
                return false;
            }
            Size_t tail = m_tail.load(std::memory_order_relaxed);
            if (tail - m_head.load(std::memory_order_acquire) >= Capacity) {
                return false;
            }
            m_buffer[tail & kMask] = item;
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        bool tryPush(T&& item) {
            if (m_closed.load(std::memory_order_relaxed)) {
                return false;
            }
            Size_t tail = m_tail.load(std::memory_order_relaxed);
            if (tail - m_head.load(std::memory_order_acquire) >= Capacity) {
                return false;
            }
            m_buffer[tail & kMask] = std::move(item);
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        bool tryPop(T& item) {
            Size_t head = m_head.load(std::memory_order_relaxed);
            if (head == m_tail.load(std::memory_order_acquire)) {
                return false;
            }
            item = std::move(m_buffer[head & kMask]);
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        void close() {
            m_closed.store(true, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(m_block_mutex);
            }
            m_block_cv.notify_all();
        }
    };

} // dodoe
