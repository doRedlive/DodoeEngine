// do@Redlive

#pragma once

#include "dopch.h"

#include <mutex>
#include <condition_variable>

namespace dodoe {

    class WaitGroup {
        std::mutex m_mutex{};
        std::condition_variable m_cv{};
        Int32 m_count{0};

    public:
        explicit WaitGroup(const Int32 count) : m_count(count) { }

        void done() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_count--;
            }
            m_cv.notify_one();
        }

        void wait() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return m_count <= 0; });
        }
    };

} // namespace dodoe
