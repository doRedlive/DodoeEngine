// do@Redlive

#include "task_scheduler.h"

namespace dodoe {

    TaskScheduler& TaskScheduler::Self() {
        static TaskScheduler instance(std::thread::hardware_concurrency());
        return instance;
    }

    TaskScheduler::TaskScheduler(Size_t thread_count)
        : m_thread_count(std::max<Size_t>(1, thread_count)) {
        for (Size_t i = 0; i < m_thread_count; i++) {
            m_threads.emplace_back(&TaskScheduler::workerLoop, this);
        }
    }

    TaskScheduler::~TaskScheduler() {
        {
            std::unique_lock<std::mutex> lock(m_queue_mutex);
            m_stop = true;
        }
        m_condition.notify_all();
        for (auto& thread : m_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
    }

    void TaskScheduler::workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_queue_mutex);
                m_condition.wait(lock, [this] {
                    return m_stop || !m_tasks.empty();
                });
                if (m_stop && m_tasks.empty()) {
                    return;
                }
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
            task();
        }
    }

    void TaskScheduler::parallelFor(Size_t begin, Size_t end,
                                     const std::function<void(Size_t, Size_t)>& fn) {
        if (begin >= end) {
            return;
        }

        Size_t count = end - begin;
        Size_t num_chunks = std::min(count, m_thread_count);
        Size_t chunk_size = count / num_chunks;
        Size_t remainder = count % num_chunks;

        std::atomic<Size_t> completed{0};

        for (Size_t i = 0; i < num_chunks; i++) {
            Size_t chunk_begin = begin + i * chunk_size + std::min(i, remainder);
            Size_t chunk_end = chunk_begin + chunk_size + (i < remainder ? 1 : 0);

            submit([&fn, &completed, num_chunks, chunk_begin, chunk_end]() {
                fn(chunk_begin, chunk_end);
                completed.fetch_add(1, std::memory_order_relaxed);
            });
        }

        while (completed.load(std::memory_order_relaxed) < num_chunks) {
            std::this_thread::yield();
        }
    }

    void TaskScheduler::waitAll() {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        submit([promise]() {
            promise->set_value();
        });
        future.wait();
    }

} // dodoe
