// do@Redlive

#pragma once

#include "dopch.h"

#include <thread>
#include <future>
#include <functional>

namespace dodoe {

    class TaskScheduler {
        DynamicArray<std::thread> m_threads{};
        std::queue<std::function<void()>> m_tasks{};
        std::mutex m_queue_mutex{};
        std::condition_variable m_condition{};
        std::atomic<Size_t> m_active_count{0};
        Bool m_stop{false};
        Size_t m_thread_count{0};

    public:
        static TaskScheduler& Self();

        explicit TaskScheduler(Size_t thread_count);
        ~TaskScheduler();

        TaskScheduler(const TaskScheduler&) = delete;
        TaskScheduler& operator=(const TaskScheduler&) = delete;

        void parallelFor(Size_t begin, Size_t end,
                         const std::function<void(Size_t, Size_t)>& fn);

        template<typename F, typename... Args>
        auto async(F&& f, Args&&... args)
            -> std::future<decltype(f(args...))> {
            using ReturnType = decltype(f(args...));
            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::future<ReturnType> result = task->get_future();
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_tasks.emplace([this, task]() {
                    m_active_count.fetch_add(1, std::memory_order_relaxed);
                    (*task)();
                    m_active_count.fetch_sub(1, std::memory_order_relaxed);
                });
            }
            m_condition.notify_one();
            return result;
        }

        template<typename F, typename... Args>
        void submit(F&& f, Args&&... args) {
            auto bound = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_tasks.emplace([this, bound]() {
                    m_active_count.fetch_add(1, std::memory_order_relaxed);
                    bound();
                    m_active_count.fetch_sub(1, std::memory_order_relaxed);
                });
            }
            m_condition.notify_one();
        }

        void waitAll();

        [[nodiscard]] Size_t threadCount() const { return m_thread_count; }

    private:
        void workerLoop();
    };

} // dodoe
