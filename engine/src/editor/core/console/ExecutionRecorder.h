// do@Redlive

#pragma once

#include "core/Signal.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace cakery {

struct ExecutionRecord {
    std::uint64_t sequence = 0;
    std::string command;
    nlohmann::json args;
    bool ok = false;
    std::string message;
    nlohmann::json data;
    std::string author;
    std::string session;
    bool mutating = false;
    double duration_ms = 0.0;
    std::int64_t timestamp_ms = 0;
};

class ExecutionRecorder {
public:
    static ExecutionRecorder& self();

    ExecutionRecorder(const ExecutionRecorder&) = delete;
    ExecutionRecorder& operator=(const ExecutionRecorder&) = delete;

    void record(ExecutionRecord record);
    void clear();

    [[nodiscard]] std::vector<ExecutionRecord> records(std::size_t limit) const;
    [[nodiscard]] std::size_t count() const { return m_records.size(); }

    [[nodiscard]] nlohmann::json toJSON(std::size_t limit, bool mutatingOnly,
                                        const std::string& author) const;
    [[nodiscard]] nlohmann::json toGraphJSON(std::size_t limit, bool mutatingOnly,
                                             const std::string& author) const;

    Signal<> changed;

private:
    ExecutionRecorder() = default;

    static constexpr std::size_t kMaxRecords = 1024;

    std::deque<ExecutionRecord> m_records;
    std::uint64_t m_nextSequence = 1;
};

} // namespace cakery
