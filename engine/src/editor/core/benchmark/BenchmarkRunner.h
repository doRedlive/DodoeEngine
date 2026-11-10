// do@Redlive
#pragma once
#include "dopch.h"

#include <chrono>
#include <functional>


namespace cakery {

class RuntimeBackend;

class BenchmarkRunner {
public:
    bool active() const { return m_active; }

    bool start(RuntimeBackend& backend, int frames,
               std::function<void(bool, Json)> onFinished,
               std::function<void(int, int)> onProgress);

    void tick(RuntimeBackend* backend);

    void reset();

private:
    void finish(bool ok, Json payload);

    bool m_active = false;
    int m_frames = 0;
    int m_done = 0;
    std::chrono::steady_clock::time_point m_deadline{};
    std::function<void(bool, Json)> m_finished;
    std::function<void(int, int)> m_progress;
};

} // namespace cakery
