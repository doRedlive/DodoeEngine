// do@Redlive

#pragma once

#include "dopch.h"

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace dodoe {

    template <typename T, Size_t Capacity>
    class MpmcQueue {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

        static constexpr Size_t kMask = Capacity - 1;

        struct Cell {
            std::atomic<Size_t> sequence{0};
            T storage{};
        };

        std::array<Cell, Capacity> m_buffer{};
        alignas(64) std::atomic<Size_t> m_enqueue_pos{0};
        alignas(64) std::atomic<Size_t> m_dequeue_pos{0};
        std::atomic<Bool> m_closed{false};
        std::mutex m_block_mutex{};
        std::condition_variable m_block_cv{};

    public:
        MpmcQueue() {
            for (Size_t i = 0; i < Capacity; ++i) {
                m_buffer[i].sequence.store(i, std::memory_order_relaxed);
            }
        }

        void push(T&& item) {
            for (;;) {
                Size_t pos = m_enqueue_pos.load(std::memory_order_relaxed);
                Cell& cell = m_buffer[pos & kMask];
                const auto dif = static_cast<std::ptrdiff_t>(cell.sequence.load(std::memory_order_acquire))
                    - static_cast<std::ptrdiff_t>(pos);
                if (dif == 0) {
                    if (m_enqueue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        cell.storage = std::move(item);
                        cell.sequence.store(pos + 1, std::memory_order_release);
                        return;
                    }
                    continue;
                }
                if (dif < 0) {
                    if (m_closed.load(std::memory_order_relaxed)) {
                        return;
                    }
                    std::unique_lock<std::mutex> lock(m_block_mutex);
                    m_block_cv.wait_for(lock, std::chrono::microseconds(100), [this, pos] {
                        if (m_closed.load(std::memory_order_relaxed)) {
                            return true;
                        }
                        const Size_t p = m_enqueue_pos.load(std::memory_order_relaxed);
                        const auto s = static_cast<std::ptrdiff_t>(m_buffer[p & kMask].sequence.load(std::memory_order_acquire));
                        return s - static_cast<std::ptrdiff_t>(p) >= 0;
                    });
                }
            }
        }

        bool tryPop(T& item) {
            for (;;) {
                Size_t pos = m_dequeue_pos.load(std::memory_order_relaxed);
                Cell& cell = m_buffer[pos & kMask];
                const auto dif = static_cast<std::ptrdiff_t>(cell.sequence.load(std::memory_order_acquire))
                    - static_cast<std::ptrdiff_t>(pos + 1);
                if (dif == 0) {
                    if (m_dequeue_pos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                        item = std::move(cell.storage);
                        cell.sequence.store(pos + kMask + 1, std::memory_order_release);
                        return true;
                    }
                    continue;
                }
                if (dif < 0) {
                    return false;
                }
            }
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
