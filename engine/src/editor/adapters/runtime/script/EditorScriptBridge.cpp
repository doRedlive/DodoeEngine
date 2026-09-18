// do@Redlive

#include "EditorScriptBridge.h"

#include "core/EditorSession.h"
#include "runtime/function/script/script_command.h"
#include "runtime/function/script/script_glue.h"

#include <combaseapi.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace cakery {

namespace {

struct EditorScriptBindings {
    int (*is_ready)();
    int (*get_selection)(uint64_t* out_uuids, int capacity);
    int (*set_selection)(const uint64_t* uuids, int count);
    int (*get_entity_count)();
    int (*get_entity)(int index, uint64_t* out_uuid, uint64_t* out_parent, char* out_name,
                      int name_capacity);
    int (*get_scene_name)(char* out, int capacity);
    int (*get_project_root)(char* out, int capacity);
    int (*get_play_state)();
    int (*execute_command)(const char* name, const char* payload);
    int (*undo)();
    int (*redo)();
    int (*create_entity)(const char* name, uint64_t* out_uuid);
    int (*delete_entity)(uint64_t uuid);
    int (*rename_entity)(uint64_t uuid, const char* name);
    int (*reparent_entity)(uint64_t uuid, uint64_t parent_uuid);
};

EditorSession* s_session = nullptr;
dodoe::ScriptCallFn s_call = nullptr;
int s_extensionToken = 0;

bool SessionReady() {
    if (!s_session) {
        return false;
    }
    const EditorSessionState state = s_session->state();
    return state == EditorSessionState::Ready || state == EditorSessionState::Degraded;
}

int CopyString(const std::string& value, char* out, int capacity) {
    const int length = static_cast<int>(value.size());
    if (out != nullptr && capacity > length) {
        std::memcpy(out, value.data(), static_cast<std::size_t>(length));
        out[length] = '\0';
    }
    return length;
}

int Ed_IsReady() { return SessionReady() ? 1 : 0; }

int Ed_GetSelection(uint64_t* out_uuids, int capacity) {
    if (!s_session) {
        return 0;
    }
    const std::vector<uint64_t>& selection = s_session->selection().selectedAll();
    const int count = static_cast<int>(selection.size());
    if (out_uuids != nullptr && capacity > 0) {
        const int copy = count < capacity ? count : capacity;
        for (int i = 0; i < copy; ++i) {
            out_uuids[i] = selection[static_cast<std::size_t>(i)];
        }
    }
    return count;
}

int Ed_SetSelection(const uint64_t* uuids, int count) {
    if (!s_session) {
        return 0;
    }
    std::vector<uint64_t> values;
    if (uuids != nullptr && count > 0) {
        values.assign(uuids, uuids + count);
    }
    s_session->selection().selectMany(std::move(values));
    return 1;
}

int Ed_GetEntityCount() {
    if (!s_session) {
        return 0;
    }
    return static_cast<int>(s_session->documentModel().entities().size());
}

int Ed_GetEntity(int index, uint64_t* out_uuid, uint64_t* out_parent, char* out_name,
                 int name_capacity) {
    if (!s_session) {
        return -1;
    }
    const std::vector<EditorEntity>& entities = s_session->documentModel().entities();
    if (index < 0 || static_cast<std::size_t>(index) >= entities.size()) {
        return -1;
    }
    const EditorEntity& entity = entities[static_cast<std::size_t>(index)];
    if (out_uuid != nullptr) {
        *out_uuid = entity.uuid;
    }
    if (out_parent != nullptr) {
        *out_parent = entity.parent;
    }
    return CopyString(entity.name, out_name, name_capacity);
}

int Ed_GetSceneName(char* out, int capacity) {
    if (!s_session) {
        return 0;
    }
    return CopyString(s_session->documentModel().name(), out, capacity);
}

int Ed_GetProjectRoot(char* out, int capacity) {
    if (!s_session) {
        return 0;
    }
    return CopyString(s_session->project().rootPath, out, capacity);
}

int Ed_GetPlayState() {
    if (!s_session) {
        return 0;
    }
    switch (s_session->playState()) {
    case PlayState::Playing: return 1;
    case PlayState::Paused: return 2;
    default: return 0;
    }
}

int Ed_ExecuteCommand(const char* name, const char* payload) {
    if (!s_session || name == nullptr) {
        return 0;
    }
    EditorCommandMessage message;
    message.name = name;
    if (payload != nullptr) {
        message.payload = payload;
    }
    return s_session->execute(std::move(message)) ? 1 : 0;
}

int Ed_Undo() { return s_session && s_session->undo() ? 1 : 0; }

int Ed_Redo() { return s_session && s_session->redo() ? 1 : 0; }

int Ed_CreateEntity(const char* name, uint64_t* out_uuid) {
    if (!s_session || name == nullptr) {
        return 0;
    }
    const uint64_t uuid = s_session->createEntity(name);
    if (out_uuid != nullptr) {
        *out_uuid = uuid;
    }
    return uuid != 0 ? 1 : 0;
}

int Ed_DeleteEntity(uint64_t uuid) {
    return s_session && s_session->deleteEntity(uuid) ? 1 : 0;
}

int Ed_RenameEntity(uint64_t uuid, const char* name) {
    if (!s_session || name == nullptr) {
        return 0;
    }
    return s_session->renameEntity(uuid, name) ? 1 : 0;
}

int Ed_ReparentEntity(uint64_t uuid, uint64_t parent_uuid) {
    return s_session && s_session->reparentEntity(uuid, parent_uuid) ? 1 : 0;
}

bool CallJsonCommand(dodoe::ScriptCommand command, const char* argument, nlohmann::json& out) {
    if (s_call == nullptr) {
        return false;
    }
    void* args[1] = { const_cast<char*>(argument) };
    void* result = nullptr;
    const int rc = s_call(command, args, &result);
    if (rc != 1 || result == nullptr) {
        return false;
    }
    const char* text = static_cast<const char*>(result);
    bool ok = false;
    try {
        out = nlohmann::json::parse(text);
        ok = true;
    } catch (...) {
        ok = false;
    }
    CoTaskMemFree(result);
    return ok;
}

void EmitEditorBindings(dodoe::ScriptCallFn call) {
    if (call == nullptr) {
        return;
    }
    s_call = call;
    const EditorScriptBindings bindings{
        &Ed_IsReady,
        &Ed_GetSelection,
        &Ed_SetSelection,
        &Ed_GetEntityCount,
        &Ed_GetEntity,
        &Ed_GetSceneName,
        &Ed_GetProjectRoot,
        &Ed_GetPlayState,
        &Ed_ExecuteCommand,
        &Ed_Undo,
        &Ed_Redo,
        &Ed_CreateEntity,
        &Ed_DeleteEntity,
        &Ed_RenameEntity,
        &Ed_ReparentEntity,
    };
    void* args[1] = { const_cast<EditorScriptBindings*>(&bindings) };
    call(dodoe::ScriptCommand::RegisterEditorNatives, args, nullptr);
}

} // namespace

void EditorScriptBridge::Initialize(EditorSession* session) {
    s_session = session;
    dodoe::ScriptGlue::AddHostExtension(&s_extensionToken, &EmitEditorBindings);
}

void EditorScriptBridge::Shutdown() {
    dodoe::ScriptGlue::RemoveHostExtension(&s_extensionToken);
    s_session = nullptr;
    s_call = nullptr;
}

bool EditorScriptBridge::IsRegistered() {
    return s_session != nullptr;
}

bool EditorScriptBridge::GetCustomInspectorUI(const std::string& typeName, nlohmann::json& out) {
    out = nullptr;
    if (typeName.empty()) {
        return false;
    }
    return CallJsonCommand(dodoe::ScriptCommand::EditorGetInspectorUI, typeName.c_str(), out);
}

bool EditorScriptBridge::ListEditorWindows(std::vector<std::pair<std::string, std::string>>& out) {
    out.clear();
    nlohmann::json json;
    if (!CallJsonCommand(dodoe::ScriptCommand::EditorListWindows, nullptr, json)) {
        return false;
    }
    if (!json.is_object() || !json.contains("windows") || !json["windows"].is_array()) {
        return false;
    }
    for (const nlohmann::json& item : json["windows"]) {
        out.emplace_back(item.value("id", std::string()), item.value("title", std::string()));
    }
    return true;
}

bool EditorScriptBridge::GetEditorWindowUI(const std::string& id, nlohmann::json& out) {
    out = nullptr;
    if (id.empty()) {
        return false;
    }
    return CallJsonCommand(dodoe::ScriptCommand::EditorGetWindowUI, id.c_str(), out);
}

bool EditorScriptBridge::DispatchEditorEvent(const std::string& owner, const std::string& ownerId,
                                             const std::string& controlId, const std::string& eventName,
                                             const nlohmann::json& value) {
    if (s_call == nullptr) {
        return false;
    }
    const std::string valueText = value.dump();
    void* args[5] = {
        const_cast<char*>(owner.c_str()),
        const_cast<char*>(ownerId.c_str()),
        const_cast<char*>(controlId.c_str()),
        const_cast<char*>(eventName.c_str()),
        const_cast<char*>(valueText.c_str()),
    };
    return s_call(dodoe::ScriptCommand::EditorDispatchEvent, args, nullptr) == 1;
}

} // namespace cakery
