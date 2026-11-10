// do@Redlive

#include "dopch.h"

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

UnorderedSet<String> CollectDataUuids(const Json& data)
{
    UnorderedSet<String> uuids;
    if (data.is_object()) {
        const auto it = data.find("uuid");
        if (it != data.end() && it->is_string()) {
            uuids.insert(it->get<String>());
        }
    }
    return uuids;
}

bool IsReplayableCommand(const String& command)
{
    static constexpr const char* kNonReplayablePrefixes[] = {
        "query.", "workflow.", "benchmark.", "session.", "mcp.", "ai.",
    };
    for (const char* prefix : kNonReplayablePrefixes) {
        if (command.rfind(prefix, 0) == 0) {
            return false;
        }
    }
    return !command.empty();
}

void SubstituteUuids(Json& value, const UnorderedMap<String, String>& map)
{
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end(); ++it) {
            if (it.key() == "uuid" && it.value().is_string()) {
                const auto mapped = map.find(it.value().get<String>());
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

Json StepsToJson(const DynamicArray<WorkflowStep>& steps)
{
    Json array = Json::array();
    for (const WorkflowStep& step : steps) {
        Json item;
        item["command"] = step.command;
        item["args"] = step.args;
        item["produces"] = step.produces;
        array.push_back(std::move(item));
    }
    return array;
}

DynamicArray<WorkflowStep> StepsFromJson(const Json& array)
{
    DynamicArray<WorkflowStep> steps;
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
        step.command = commandIt->get<String>();
        step.args = *argsIt;
        const auto producesIt = item.find("produces");
        if (producesIt != item.end() && producesIt->is_array()) {
            for (const auto& uuid : *producesIt) {
                if (uuid.is_string()) {
                    step.produces.push_back(uuid.get<String>());
                }
            }
        }
        steps.push_back(std::move(step));
    }
    return steps;
}

} // anonymous namespace

WorkflowLibrary::WorkflowLibrary() = default;

void WorkflowLibrary::setStoragePath(const String& path)
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

DynamicArray<String> WorkflowLibrary::names() const
{
    DynamicArray<String> result;
    result.reserve(m_templates.size());
    for (const auto& [name, steps] : m_templates) {
        result.push_back(name);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool WorkflowLibrary::contains(const String& name) const
{
    return m_templates.contains(name);
}

const DynamicArray<WorkflowStep>* WorkflowLibrary::find(const String& name) const
{
    const auto it = m_templates.find(name);
    return it == m_templates.end() ? nullptr : &it->second;
}

void WorkflowLibrary::store(const String& name, DynamicArray<WorkflowStep> steps)
{
    m_templates[name] = std::move(steps);
    if (!m_storagePath.empty()) {
        saveToDisk();
    }
}

void WorkflowLibrary::loadFromDisk()
{
    std::ifstream file(m_storagePath.c_str());
    if (!file.is_open()) {
        return;
    }
    Json root;
    try {
        file >> root;
    } catch (const Json::exception&) {
        return;
    }
    if (!root.is_object() || !root.contains("templates") || !root["templates"].is_object()) {
        return;
    }
    for (auto it = root["templates"].begin(); it != root["templates"].end(); ++it) {
        DynamicArray<WorkflowStep> steps = StepsFromJson(it.value());
        if (!steps.empty()) {
            m_templates[String(it.key())] = std::move(steps);
        }
    }
}

void WorkflowLibrary::saveToDisk()
{
    Json root = Json::object();
    Json templates = Json::object();
    for (const auto& [name, steps] : m_templates) {
        templates[name] = StepsToJson(steps);
    }
    root["templates"] = std::move(templates);

    std::ofstream file(m_storagePath.c_str(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return;
    }
    file << root.dump(4) << std::endl;
}

String WorkflowStoragePath(EditorSession& session)
{
    const String root = session.project().rootPath;
    if (root.empty()) {
        return {};
    }
    std::error_code ec;
    const FsPath dir = FsPath(root) / "Configs";
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return {};
    }
    return String((dir / "workflows.json").string());
}

DynamicArray<WorkflowStep> CaptureWorkflowSteps(const Json& records)
{
    DynamicArray<WorkflowStep> steps;
    if (!records.is_array()) {
        return steps;
    }
    for (const auto& record : records) {
        if (!record.is_object()) {
            continue;
        }
        const String command = record.value("command", String());
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
        for (const String& uuid : CollectDataUuids(record.contains("data") ? record["data"] : Json())) {
            step.produces.push_back(uuid);
        }
        steps.push_back(std::move(step));
    }
    return steps;
}

Json RunWorkflow(EditorSession& session, const String& name,
                           const DynamicArray<WorkflowStep>& steps)
{
    UnorderedMap<String, String> uuidMap;
    Json out = Json::object();
    out["name"] = name;
    out["steps"] = Json::array();

    bool allOk = true;
    int index = 0;
    for (const WorkflowStep& step : steps) {
        Json args = step.args;
        SubstituteUuids(args, uuidMap);

        CommandContext context;
        context.author = "workflow";
        const CommandResult result =
            session.commands().executeStructured(session, step.command, args, context);

        Json stepOut = Json::object();
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
                for (const String& oldUuid : step.produces) {
                    uuidMap[oldUuid] = uuidIt->get<String>();
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
