// do@Redlive

#include "WorkflowTemplate.h"

#include "CommandRegistry.h"

#include "core/EditorSession.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <utility>

namespace cakery {

namespace {

std::unordered_set<std::string> CollectDataUuids(const nlohmann::json& data)
{
    std::unordered_set<std::string> uuids;
    if (data.is_object()) {
        const auto it = data.find("uuid");
        if (it != data.end() && it->is_string()) {
            uuids.insert(it->get<std::string>());
        }
    }
    return uuids;
}

bool IsReplayableCommand(const std::string& command)
{
    static constexpr const char* kNonReplayablePrefixes[] = {
        "query.", "workflow.", "benchmark.", "session.",
    };
    for (const char* prefix : kNonReplayablePrefixes) {
        if (command.rfind(prefix, 0) == 0) {
            return false;
        }
    }
    return !command.empty();
}

void SubstituteUuids(nlohmann::json& value, const std::unordered_map<std::string, std::string>& map)
{
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() == "uuid" && it.value().is_string()) {
                const auto mapped = map.find(it.value().get<std::string>());
                if (mapped != map.end()) {
                    it.value() = mapped->second;
                }
            } else {
                SubstituteUuids(it.value(), map);
            }
        }
    } else if (value.is_array()) {
        for (auto& item : value) {
            SubstituteUuids(item, map);
        }
    }
}

nlohmann::json StepsToJson(const std::vector<WorkflowStep>& steps)
{
    nlohmann::json array = nlohmann::json::array();
    for (const WorkflowStep& step : steps) {
        nlohmann::json item;
        item["command"] = step.command;
        item["args"] = step.args;
        item["produces"] = step.produces;
        array.push_back(std::move(item));
    }
    return array;
}

std::vector<WorkflowStep> StepsFromJson(const nlohmann::json& array)
{
    std::vector<WorkflowStep> steps;
    if (!array.is_array()) {
        return steps;
    }
    steps.reserve(array.size());
    for (const auto& item : array) {
        if (!item.is_object()) {
            continue;
        }
        const auto commandIt = item.find("command");
        const auto argsIt = item.find("args");
        if (commandIt == item.end() || !commandIt->is_string() ||
            argsIt == item.end() || !argsIt->is_object()) {
            continue;
        }
        WorkflowStep step;
        step.command = commandIt->get<std::string>();
        step.args = *argsIt;
        const auto producesIt = item.find("produces");
        if (producesIt != item.end() && producesIt->is_array()) {
            for (const auto& uuid : *producesIt) {
                if (uuid.is_string()) {
                    step.produces.push_back(uuid.get<std::string>());
                }
            }
        }
        steps.push_back(std::move(step));
    }
    return steps;
}

} // anonymous namespace

WorkflowLibrary& WorkflowLibrary::self()
{
    static WorkflowLibrary instance;
    return instance;
}

void WorkflowLibrary::setStoragePath(const std::string& path)
{
    if (m_loaded) {
        return;
    }
    m_storagePath = path;
    m_loaded = true;
    if (!m_storagePath.empty()) {
        loadFromDisk();
    }
}

std::vector<std::string> WorkflowLibrary::names() const
{
    std::vector<std::string> result;
    result.reserve(m_templates.size());
    for (const auto& [name, steps] : m_templates) {
        result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool WorkflowLibrary::contains(const std::string& name) const
{
    return m_templates.contains(name);
}

const std::vector<WorkflowStep>* WorkflowLibrary::find(const std::string& name) const
{
    const auto it = m_templates.find(name);
    return it == m_templates.end() ? nullptr : &it->second;
}

void WorkflowLibrary::store(const std::string& name, std::vector<WorkflowStep> steps)
{
    m_templates[name] = std::move(steps);
    if (!m_storagePath.empty()) {
        saveToDisk();
    }
}

void WorkflowLibrary::loadFromDisk()
{
    std::ifstream file(m_storagePath);
    if (!file.is_open()) {
        return;
    }
    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::exception&) {
        return;
    }
    if (!root.is_object() || !root.contains("templates") || !root["templates"].is_object()) {
        return;
    }
    for (auto it = root["templates"].begin(); it != root["templates"].end(); ++it) {
        std::vector<WorkflowStep> steps = StepsFromJson(it.value());
        if (!steps.empty()) {
            m_templates[it.key()] = std::move(steps);
        }
    }
}

void WorkflowLibrary::saveToDisk()
{
    nlohmann::json root = nlohmann::json::object();
    nlohmann::json templates = nlohmann::json::object();
    for (const auto& [name, steps] : m_templates) {
        templates[name] = StepsToJson(steps);
    }
    root["templates"] = std::move(templates);

    std::ofstream file(m_storagePath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return;
    }
    file << root.dump(4) << std::endl;
}

std::string WorkflowStoragePath(EditorSession& session)
{
    const std::string root = session.project().rootPath;
    if (root.empty()) {
        return {};
    }
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::path(root) / "Configs";
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return {};
    }
    return (dir / "workflows.json").string();
}

std::vector<WorkflowStep> CaptureWorkflowSteps(const nlohmann::json& records)
{
    std::vector<WorkflowStep> steps;
    if (!records.is_array()) {
        return steps;
    }
    for (const auto& record : records) {
        if (!record.is_object()) {
            continue;
        }
        const std::string command = record.value("command", std::string());
        if (!IsReplayableCommand(command)) {
            continue;
        }
        const auto argsIt = record.find("args");
        if (argsIt == record.end() || !argsIt->is_object()) {
            continue;
        }
        WorkflowStep step;
        step.command = command;
        step.args = *argsIt;
        for (const std::string& uuid : CollectDataUuids(record.contains("data") ? record["data"] : nlohmann::json())) {
            step.produces.push_back(uuid);
        }
        steps.push_back(std::move(step));
    }
    return steps;
}

nlohmann::json RunWorkflow(EditorSession& session, const std::string& name,
                           const std::vector<WorkflowStep>& steps)
{
    std::unordered_map<std::string, std::string> uuidMap;
    nlohmann::json out = nlohmann::json::object();
    out["name"] = name;
    out["steps"] = nlohmann::json::array();

    bool allOk = true;
    int index = 0;
    for (const WorkflowStep& step : steps) {
        nlohmann::json args = step.args;
        SubstituteUuids(args, uuidMap);

        CommandContext context;
        context.author = "workflow";
        const CommandResult result =
            CommandRegistry::self().executeStructured(session, step.command, args, context);

        nlohmann::json stepOut = nlohmann::json::object();
        stepOut["index"] = index;
        stepOut["command"] = step.command;
        stepOut["ok"] = result.ok;
        stepOut["message"] = result.message;
        out["steps"].push_back(std::move(stepOut));

        if (!result.ok) {
            out["aborted_at"] = index;
            allOk = false;
            break;
        }

        if (result.data.is_object()) {
            const auto uuidIt = result.data.find("uuid");
            if (uuidIt != result.data.end() && uuidIt->is_string()) {
                for (const std::string& oldUuid : step.produces) {
                    uuidMap[oldUuid] = uuidIt->get<std::string>();
                }
            }
        }
        ++index;
    }
    out["ok"] = allOk;
    out["executed"] = allOk ? static_cast<int>(steps.size()) : index + 1;
    return out;
}

} // namespace cakery
