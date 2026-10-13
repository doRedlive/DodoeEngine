// do@Redlive

#include "ExecutionRecorder.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace cakery {

namespace {

bool MatchesFilter(const ExecutionRecord& record, bool mutatingOnly, const std::string& author)
{
    if (mutatingOnly && !record.mutating) {
        return false;
    }
    if (!author.empty() && author != "all" && record.author != author) {
        return false;
    }
    return true;
}

bool IsUuidText(const std::string& text)
{
    if (text.empty() || text.size() > 20) {
        return false;
    }
    return std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

void CollectArtifactIds(const nlohmann::json& value, std::unordered_set<std::string>& out)
{
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() == "uuid" && it.value().is_string()) {
                const std::string text = it.value().get<std::string>();
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

nlohmann::json RecordToJSON(const ExecutionRecord& record)
{
    nlohmann::json item;
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

ExecutionRecorder& ExecutionRecorder::self()
{
    static ExecutionRecorder instance;
    return instance;
}

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

std::vector<ExecutionRecord> ExecutionRecorder::records(const std::size_t limit) const
{
    std::vector<ExecutionRecord> result;
    if (limit == 0 || limit >= m_records.size()) {
        result.assign(m_records.begin(), m_records.end());
    } else {
        result.assign(m_records.end() - static_cast<std::ptrdiff_t>(limit), m_records.end());
    }
    return result;
}

nlohmann::json ExecutionRecorder::toJSON(const std::size_t limit, const bool mutatingOnly,
                                         const std::string& author) const
{
    nlohmann::json out = nlohmann::json::object();
    out["total"] = count();
    nlohmann::json items = nlohmann::json::array();
    for (const ExecutionRecord& record : records(limit)) {
        if (!MatchesFilter(record, mutatingOnly, author)) {
            continue;
        }
        items.push_back(RecordToJSON(record));
    }
    out["records"] = std::move(items);
    return out;
}

nlohmann::json ExecutionRecorder::toGraphJSON(const std::size_t limit, const bool mutatingOnly,
                                              const std::string& author) const
{
    std::vector<ExecutionRecord> filtered;
    for (const ExecutionRecord& record : records(limit)) {
        if (MatchesFilter(record, mutatingOnly, author)) {
            filtered.push_back(record);
        }
    }

    std::unordered_map<std::string, std::uint64_t> producer;
    for (const ExecutionRecord& record : filtered) {
        if (!record.mutating || !record.ok) {
            continue;
        }
        std::unordered_set<std::string> artifacts;
        CollectArtifactIds(record.data, artifacts);
        for (const std::string& artifact : artifacts) {
            if (!producer.contains(artifact)) {
                producer[artifact] = record.sequence;
            }
        }
    }

    nlohmann::json out = nlohmann::json::object();
    out["total"] = count();
    out["filtered"] = filtered.size();

    nlohmann::json nodes = nlohmann::json::array();
    nlohmann::json sessions = nlohmann::json::array();
    nlohmann::json currentSession = nullptr;
    for (const ExecutionRecord& record : filtered) {
        nodes.push_back(RecordToJSON(record));
        if (!record.session.empty()) {
            if (!currentSession.is_object() ||
                currentSession.value("id", std::string()) != record.session) {
                if (currentSession.is_object()) {
                    sessions.push_back(std::move(currentSession));
                }
                currentSession = nlohmann::json::object();
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

    nlohmann::json edges = nlohmann::json::array();
    for (const ExecutionRecord& record : filtered) {
        std::unordered_set<std::string> referenced;
        CollectArtifactIds(record.args, referenced);
        for (const std::string& artifact : referenced) {
            const auto it = producer.find(artifact);
            if (it == producer.end() || it->second >= record.sequence) {
                continue;
            }
            nlohmann::json edge;
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
