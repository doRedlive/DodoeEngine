// do@Redlive
#pragma once
#include "dopch.h"

#include "core/Signal.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace cakery {

struct ExecutionRecord {
    UInt64 sequence = 0;
    String command;
    Json args;
    bool ok = false;
    String message;
    Json data;
    String author;
    String session;
    bool mutating = false;
    double duration_ms = 0.0;
    Int64 timestamp_ms = 0;
};

class ExecutionRecorder {
public:
    ExecutionRecorder();

    ExecutionRecorder(const ExecutionRecorder&) = delete;
    ExecutionRecorder& operator=(const ExecutionRecorder&) = delete;

    void record(ExecutionRecord record);
    void clear();

    [[nodiscard]] DynamicArray<ExecutionRecord> records(Size_t limit) const;
    [[nodiscard]] Size_t count() const { return m_records.size(); }

    [[nodiscard]] Json toJSON(Size_t limit, bool mutatingOnly,
                                        const String& author) const;
    [[nodiscard]] Json toGraphJSON(Size_t limit, bool mutatingOnly,
                                             const String& author) const;

    Signal<> changed;

private:
    static constexpr Size_t kMaxRecords = 1024;

    Deque<ExecutionRecord> m_records;
    UInt64 m_nextSequence = 1;
};

} // namespace cakery
