// do@Redlive

#pragma once

#include "dopch.h"

#include "runtime/core/memory/memory.h"
#include "runtime/core/memory/thread_allocator.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace dodoe {

    class ThreadPool {
        std::vector<std::thread> threads_;
        std::queue<std::function<void()>> tasks_;
        std::mutex queue_mutex_;
        std::condition_variable condition_;
        bool stop_;
    public:
        ThreadPool(size_t thread_size) : stop_(false) {
            for (size_t i = 0; i < thread_size; i++) {
                threads_.emplace_back([this]{
                    Memory::InitThread();
                    while (true) {
                        UInt64 cur = Memory::CurrentFrameEpoch();
                        ThreadAllocator* ta = threadAllocatorPtr();
                        if (ta && ta->last_reset_epoch.exchange(cur) != cur) {
                            ta->frame.reset();
                        }

                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(queue_mutex_);
                            condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                            if (stop_ && tasks_.empty()) {
                                Memory::ShutdownThread();
                                return;
                            }
                            task = std::move(tasks_.front());
                            tasks_.pop();
                        }
                        task();
                    }
                });
            }
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                stop_ = true;
            }
            condition_.notify_all();
            for (std::thread& thread : threads_) {
                thread.join();
            }
        }

        template<typename F, typename... Args>
        void enqueue(F&& f, Args&&... args) {
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                tasks_.emplace([f, args...] { f(args...); });
            }
            condition_.notify_one();
        }
    };

} // dodoe