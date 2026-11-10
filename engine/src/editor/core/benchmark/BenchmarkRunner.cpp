// do@Redlive

#include "dopch.h"

#include "BenchmarkRunner.h"

#include "backend/RuntimeBackend.h"

#include <algorithm>
#include <utility>

namespace cakery {

bool BenchmarkRunner::start(RuntimeBackend& backend, int frames,
                            std::function<void(bool, Json)> onFinished,
                            std::function<void(int, int)> onProgress)
{
    if (m_active) {
        return false;
    }
    const int clamped = std::clamp(frames, 16, 3600);
    if (!backend.startBenchmark(clamped)) {
        return false;
    }
    m_frames = clamped;
    m_done = 0;
    m_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    m_finished = std::move(onFinished);
    m_progress = std::move(onProgress);
    m_active = true;
    return true;
}

void BenchmarkRunner::tick(RuntimeBackend* backend)
{
    if (!m_active) {
        return;
    }
    if (!backend) {
        finish(false, Json{{"error", "Runtime unavailable"}});
        return;
    }
    constexpr int kFramesPerTick = 4;
    const int step = std::min(kFramesPerTick, m_frames - m_done);
    for (int i = 0; i < step; ++i) {
        backend->tickAtSafePoint();
    }
    m_done += step;
    if (m_progress) {
        m_progress(m_done, m_frames);
    }
    const bool expired = std::chrono::steady_clock::now() >= m_deadline;
    if (m_done < m_frames && !expired) {
        return;
    }
    Json out;
    const bool ok = backend->pollBenchmark(out);
    if (!ok && !expired) {
        return;
    }
    finish(ok, ok ? std::move(out)
                  : Json{{"error", "Benchmark timed out waiting for frames"}});
}

void BenchmarkRunner::reset()
{
    m_active = false;
    m_finished = nullptr;
    m_progress = nullptr;
}

void BenchmarkRunner::finish(bool ok, Json payload)
{
    m_active = false;
    auto finished = std::move(m_finished);
    m_finished = nullptr;
    m_progress = nullptr;
    if (finished) {
        finished(ok, std::move(payload));
    }
}

} // namespace cakery
