// do@Redlive

#include "dopch.h"

#include "ExecutionRecorder.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace cakery {

namespace {

bool MatchesFilter(const ExecutionRecord& record, bool mutatingOnly, const String& author)
{
    if (mutatingOnly && !record.mutating) {
        return false;
    }
    if (!author.empty() && author != "all" && record.author != author) {
        return false;
    }
    return true;
}

bool IsUuidText(const String& text)
{
    if (text.empty() || text.size() > 20) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

void CollectArtifactIds(const Json& value, UnorderedSet<String>& out)
{
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() == "uuid" && it.value().is_string()) {
                const String text = it.value().get<String>();
                if (IsUuidText(text)) {
                    out.insert(text);
                }
            } else {
                CollectArtifactIds(it.value(), out);
            }
        }
    } else if (value.is_array()) {
        for (const auto& item : value) {
            CollectArtifactIds(item, out);
        }
    }
}

Json RecordToJSON(const ExecutionRecord& record)
{
    Json item;
    item["sequence"] = record.sequence;
    item["command"] = record.command;
    item["args"] = record.args;
    item["ok"] = record.ok;
    item["message"] = record.message;
    item["data"] = record.data;
    item["author"] = record.author;
    item["session"] = record.session;
    item["mutating"] = record.mutating;
    item["duration_ms"] = record.duration_ms;
    item["timestamp_ms"] = record.timestamp_ms;
    return item;
}

} // anonymous namespace

ExecutionRecorder::ExecutionRecorder() = default;

void ExecutionRecorder::record(ExecutionRecord record)
{
    record.sequence = m_nextSequence++;
    record.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_records.push_back(std::move(record));
    if (m_records.size() > kMaxRecords) {
        m_records.pop_front();
    }
    changed.fire();
}

void ExecutionRecorder::clear()
{
    m_records.clear();
    changed.fire();
}

DynamicArray<ExecutionRecord> ExecutionRecorder::records(const Size_t limit) const
{
    DynamicArray<ExecutionRecord> result;
    if (limit == 0 || limit >= m_records.size()) {
        result.assign(m_records.begin(), m_records.end());
    } else {
        result.assign(m_records.end() - static_cast<std::ptrdiff_t>(limit), m_records.end());
    }
    return result;
}

Json ExecutionRecorder::toJSON(const Size_t limit, const bool mutatingOnly,
                                         const String& author) const
{
    Json out = Json::object();
    out["total"] = count();
    Json items = Json::array();
    for (const ExecutionRecord& record : records(limit)) {
        if (!MatchesFilter(record, mutatingOnly, author)) {
            continue;
        }
        items.push_back(RecordToJSON(record));
    }
    out["records"] = std::move(items);
    return out;
}

Json ExecutionRecorder::toGraphJSON(const Size_t limit, const bool mutatingOnly,
                                              const String& author) const
{
    DynamicArray<ExecutionRecord> filtered;
    for (const ExecutionRecord& record : records(limit)) {
        if (MatchesFilter(record, mutatingOnly, author)) {
            filtered.push_back(record);
        }
    }

    UnorderedMap<String, UInt64> producer;
    for (const ExecutionRecord& record : filtered) {
        if (!record.mutating || !record.ok) {
            continue;
        }
        UnorderedSet<String> artifacts;
        CollectArtifactIds(record.data, artifacts);
        for (const String& artifact : artifacts) {
            if (!producer.contains(artifact)) {
                producer[artifact] = record.sequence;
            }
        }
    }

    Json out = Json::object();
    out["total"] = count();
    out["filtered"] = filtered.size();

    Json nodes = Json::array();
    Json sessions = Json::array();
    Json currentSession = nullptr;
    for (const ExecutionRecord& record : filtered) {
        nodes.push_back(RecordToJSON(record));
        if (!record.session.empty()) {
            if (!currentSession.is_object() ||
                currentSession.value("id", String()) != record.session) {
                if (currentSession.is_object()) {
                    sessions.push_back(std::move(currentSession));
                }
                currentSession = Json::object();
                currentSession["id"] = record.session;
                currentSession["first_sequence"] = record.sequence;
                currentSession["records"] = 0;
            }
            currentSession["last_sequence"] = record.sequence;
            currentSession["records"] = currentSession["records"].get<int>() + 1;
        }
    }
    if (currentSession.is_object()) {
        sessions.push_back(std::move(currentSession));
    }
    out["nodes"] = std::move(nodes);
    out["sessions"] = std::move(sessions);

    Json edges = Json::array();
    for (const ExecutionRecord& record : filtered) {
        UnorderedSet<String> referenced;
        CollectArtifactIds(record.args, referenced);
        for (const String& artifact : referenced) {
            const auto it = producer.find(artifact);
            if (it == producer.end() || it->second >= record.sequence) {
                continue;
            }
            Json edge;
            edge["from"] = it->second;
            edge["to"] = record.sequence;
            edge["artifact"] = artifact;
            edges.push_back(std::move(edge));
        }
    }
    out["edges"] = std::move(edges);
    return out;
}

} // namespace cakery
