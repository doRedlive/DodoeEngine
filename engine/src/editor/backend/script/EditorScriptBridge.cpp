// do@Redlive

#include "dopch.h"

#include "EditorScriptBridge.h"

#include "core/EditorSession.h"
#include "runtime/function/script/script_command.h"
#include "runtime/function/script/script_glue.h"

#include <combaseapi.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cakery {

namespace {

struct EditorScriptBindings {
    int (*is_ready)();
    int (*get_selection)(UInt64* out_uuids, int capacity);
    int (*set_selection)(const UInt64* uuids, int count);
    int (*get_entity_count)();
    int (*get_entity)(int index, UInt64* out_uuid, UInt64* out_parent, char* out_name,
                      int name_capacity);
    int (*get_scene_name)(char* out, int capacity);
    int (*get_project_root)(char* out, int capacity);
    int (*get_play_state)();
    int (*execute_command)(const char* name, const char* payload);
    int (*undo)();
    int (*redo)();
    int (*create_entity)(const char* name, UInt64* out_uuid);
    int (*delete_entity)(UInt64 uuid);
    int (*rename_entity)(UInt64 uuid, const char* name);
    int (*reparent_entity)(UInt64 uuid, UInt64 parent_uuid);
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

int CopyString(StringView value, char* out, int capacity) {
    const int length = static_cast<int>(value.size());
    if (out != nullptr && capacity > length) {
        std::memcpy(out, value.data(), static_cast<Size_t>(length));
        out[length] = '\0';
    }
    return length;
}

UInt64 ParseUuid(const String& text) {
    return text.empty() ? 0 : static_cast<UInt64>(
        std::strtoull(text.c_str(), nullptr, 10));
}

TileToolId ParseTileTool(const String& name) {
    if (name == "brush") return TileToolId::Brush;
    if (name == "fill") return TileToolId::Fill;
    if (name == "erase") return TileToolId::Erase;
    if (name == "rect") return TileToolId::Rect;
    if (name == "line") return TileToolId::Line;
    if (name == "picker") return TileToolId::Picker;
    return TileToolId::Select;
}

bool ParseLegacyCommand(const String& name, const String& payload,
                        EditorOperation& out) {
    try {
        if (name == "document_changed") {
            out = DocumentChangedCommand{Json::parse(payload)};
            return true;
        }
        if (name == "scene_mouse_down") {
            SceneMouseDownCommand command;
            int alt = 0, ctrl = 0, shift = 0;
            if (std::sscanf(payload.c_str(), "%f,%f,%d,%d,%d,%d", &command.x, &command.y,
                            &command.button, &alt, &ctrl, &shift) < 3) {
                return false;
            }
            command.alt = alt != 0;
            command.ctrl = ctrl != 0;
            command.shift = shift != 0;
            out = std::move(command);
            return true;
        }
        if (name == "scene_mouse_move") {
            SceneMouseMoveCommand command;
            int ctrl = 0, shift = 0, alt = 0;
            if (std::sscanf(payload.c_str(), "%f,%f,%d,%d,%d", &command.x, &command.y,
                            &ctrl, &shift, &alt) < 2) {
                return false;
            }
            command.ctrl = ctrl != 0;
            command.shift = shift != 0;
            command.alt = alt != 0;
            out = std::move(command);
            return true;
        }
        if (name == "scene_mouse_up") {
            SceneMouseUpCommand command;
            if (std::sscanf(payload.c_str(), "%d", &command.button) < 1) {
                return false;
            }
            out = command;
            return true;
        }
        if (name == "scene_mouse_wheel") {
            SceneMouseWheelCommand command;
            if (std::sscanf(payload.c_str(), "%f", &command.delta) < 1) {
                return false;
            }
            out = command;
            return true;
        }
        if (name == "scene_key") {
            SceneKeyCommand command;
            int down = 0;
            if (std::sscanf(payload.c_str(), "%d,%d", &command.key, &down) < 2) {
                return false;
            }
            command.down = down != 0;
            out = command;
            return true;
        }
        if (name == "selection_changed") {
            out = SelectionChangedCommand{ParseUuid(payload)};
            return true;
        }
        if (name == "gizmo_mode") {
            out = GizmoModeCommand{gizmoModeFromName(payload.empty() ? "none" : payload)};
            return true;
        }
        if (name == "gizmo_snap") {
            out = GizmoSnapCommand{payload == "1" || payload == "true"};
            return true;
        }
        if (name == "gizmo_snap_step") {
            GizmoSnapStepCommand command;
            if (std::sscanf(payload.c_str(), "%f,%f,%f", &command.translate,
                            &command.rotate, &command.scale) < 1) {
                return false;
            }
            out = command;
            return true;
        }
        if (name == "camera_mode") {
            out = CameraModeCommand{payload == "2d"};
            return true;
        }
        if (name == "scene.import_asset") {
            SceneImportAssetCommand command;
            std::istringstream stream(payload);
            String line;
            DynamicArray<String> lines;
            while (std::getline(stream, line)) {
                lines.push_back(line);
            }
            if (lines.size() < 3 ||
                std::sscanf(lines[0].c_str(), "%f,%f", &command.x, &command.y) < 2) {
                return false;
            }
            for (Size_t i = 1; i + 1 < lines.size(); i += 2) {
                command.paths.push_back(String(lines[i + 1]));
            }
            out = std::move(command);
            return true;
        }
        if (name == "prefab.export") {
            const Size_t comma = payload.find(',');
            if (comma == String::npos) {
                return false;
            }
            out = PrefabExportCommand{ParseUuid(payload.substr(0, comma)),
                                      String(payload.substr(comma + 1))};
            return true;
        }
        if (name == "play") { out = PlayActionCommand{PlayAction::Play}; return true; }
        if (name == "pause") { out = PlayActionCommand{PlayAction::Pause}; return true; }
        if (name == "resume") { out = PlayActionCommand{PlayAction::Resume}; return true; }
        if (name == "stop") { out = PlayActionCommand{PlayAction::Stop}; return true; }
        if (name == "asset.save_all") { out = AssetSaveAllCommand{}; return true; }
        if (name == "asset.refresh") { out = AssetRefreshCommand{}; return true; }
        if (name == "asset.import") { out = AssetImportCommand{String(payload)}; return true; }
        if (name == "asset.reimport") { out = AssetReimportCommand{String(payload)}; return true; }
        if (name == "script.tool_action") { out = ScriptToolActionCommand{String(payload)}; return true; }
        if (name == "asset.update_settings") {
            const Json parsed = Json::parse(payload);
            if (!parsed.contains("path") || !parsed["path"].is_string() ||
                !parsed.contains("settings")) {
                return false;
            }
            out = AssetUpdateSettingsCommand{
                String(parsed["path"].get<String>()), parsed["settings"]};
            return true;
        }
        if (name == "tilemap.create") {
            const Json parsed = payload.empty()
                ? Json::object() : Json::parse(payload);
            TilemapCreateCommand command;
            command.name = String(parsed.value("name", String("Tilemap")));
            command.width = parsed.value("width", 20u);
            command.height = parsed.value("height", 20u);
            command.tileWidth = parsed.value("tile_width", 16u);
            command.tileHeight = parsed.value("tile_height", 16u);
            out = std::move(command);
            return true;
        }
        if (name == "tilemap.activate") {
            out = TilemapActivateCommand{ParseUuid(payload)};
            return true;
        }
        if (name == "tilemap.tool") {
            out = TilemapToolCommand{ParseTileTool(payload)};
            return true;
        }
        if (name == "tilemap.brush_flip") {
            out = TilemapBrushFlipCommand{payload == "x"};
            return true;
        }
        if (name == "tilemap.brush_rotate") { out = TilemapBrushRotateCommand{}; return true; }
        if (name == "tilemap.random_brush") {
            out = TilemapRandomBrushCommand{payload == "1"};
            return true;
        }
        if (name == "tilemap.selection_delete") { out = TilemapSelectionDeleteCommand{}; return true; }
        if (name == "tilemap.selection_copy") { out = TilemapSelectionCopyCommand{}; return true; }
        if (name == "tilemap.selection_paste") { out = TilemapSelectionPasteCommand{}; return true; }
        if (name == "tilemap.edit_tileset") {
            const Json parsed = Json::parse(payload);
            TilemapEditTilesetCommand command;
            command.assetId = parsed.value("asset_id", UInt64(0));
            command.tileWidth = parsed.value("tile_width", 16u);
            command.tileHeight = parsed.value("tile_height", 16u);
            command.margin = parsed.value("margin", 0u);
            command.spacing = parsed.value("spacing", 0u);
            out = command;
            return true;
        }
        if (name == "tilemap.brush") {
            const Json parsed = Json::parse(payload);
            TilemapBrushCommand command;
            command.width = parsed.value("w", 1);
            command.height = parsed.value("h", 1);
            if (parsed.contains("gids") && parsed["gids"].is_array()) {
                for (const auto& gid : parsed["gids"]) {
                    command.gids.push_back(gid.get<UInt32>());
                }
            }
            out = std::move(command);
            return true;
        }
        if (name == "tilemap.layer_active") {
            out = TilemapLayerActiveCommand{ParseUuid(payload)};
            return true;
        }
        if (name == "tilemap.layer_visible") {
            UInt64 uuid = 0;
            int visible = 0;
            if (std::sscanf(payload.c_str(), "%llu,%d", &uuid, &visible) < 2) {
                return false;
            }
            out = TilemapLayerVisibleCommand{uuid, visible != 0};
            return true;
        }
        if (name == "tilemap.layer_opacity") {
            UInt64 uuid = 0;
            float opacity = 1.0f;
            if (std::sscanf(payload.c_str(), "%llu,%f", &uuid, &opacity) < 2) {
                return false;
            }
            out = TilemapLayerOpacityCommand{uuid, opacity};
            return true;
        }
        if (name == "tilemap.layer_rename") {
            const Size_t comma = payload.find(',');
            if (comma == String::npos) {
                return false;
            }
            out = TilemapLayerRenameCommand{ParseUuid(payload.substr(0, comma)),
                                            String(payload.substr(comma + 1))};
            return true;
        }
        if (name == "tilemap.layer_add") {
            out = TilemapLayerAddCommand{String(payload)};
            return true;
        }
        if (name == "tilemap.layer_move") {
            const Size_t comma = payload.find(',');
            if (comma == String::npos) {
                return false;
            }
            out = TilemapLayerMoveCommand{ParseUuid(payload.substr(0, comma)),
                                          payload.substr(comma + 1) == "up"};
            return true;
        }
        if (name == "tilemap.layer_remove") {
            out = TilemapLayerRemoveCommand{ParseUuid(payload)};
            return true;
        }
        if (name == "tilemap.add_tileset") {
            const Json parsed = Json::parse(payload);
            TilemapAddTilesetCommand command;
            command.image = String(parsed.value("image", String()));
            command.tileWidth = parsed.value("tile_width", 16u);
            command.tileHeight = parsed.value("tile_height", 16u);
            command.margin = parsed.value("margin", 0u);
            command.spacing = parsed.value("spacing", 0u);
            out = std::move(command);
            return true;
        }
        if (name == "tilemap.resize") {
            UInt64 uuid = 0;
            unsigned int w = 0, h = 0;
            if (std::sscanf(payload.c_str(), "%llu,%u,%u", &uuid, &w, &h) < 3) {
                return false;
            }
            out = TilemapResizeCommand{uuid, w, h};
            return true;
        }
        if (name == "tilemap.remove_tileset") {
            out = TilemapRemoveTilesetCommand{ParseUuid(payload)};
            return true;
        }
    } catch (const Json::exception&) {
        return false;
    }
    return false;
}

int Ed_IsReady() { return SessionReady() ? 1 : 0; }

int Ed_GetSelection(UInt64* out_uuids, int capacity) {
    if (!s_session) {
        return 0;
    }
    const DynamicArray<UInt64>& selection = s_session->selection().selectedAll();
    const int count = static_cast<int>(selection.size());
    if (out_uuids != nullptr && capacity > 0) {
        const int copy = count < capacity ? count : capacity;
        for (int i = 0; i < copy; ++i) {
            out_uuids[i] = selection[static_cast<Size_t>(i)];
        }
    }
    return count;
}

int Ed_SetSelection(const UInt64* uuids, int count) {
    if (!s_session) {
        return 0;
    }
    DynamicArray<UInt64> values;
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

int Ed_GetEntity(int index, UInt64* out_uuid, UInt64* out_parent, char* out_name,
                 int name_capacity) {
    if (!s_session) {
        return -1;
    }
    const DynamicArray<EditorEntity>& entities = s_session->documentModel().entities();
    if (index < 0 || static_cast<Size_t>(index) >= entities.size()) {
        return -1;
    }
    const EditorEntity& entity = entities[static_cast<Size_t>(index)];
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
    const String payloadText = payload != nullptr ? payload : "";
    EditorOperation command;
    if (!ParseLegacyCommand(name, payloadText, command)) {
        return 0;
    }
    return s_session->execute(std::move(command)) ? 1 : 0;
}

int Ed_Undo() { return s_session && s_session->undo() ? 1 : 0; }

int Ed_Redo() { return s_session && s_session->redo() ? 1 : 0; }

int Ed_CreateEntity(const char* name, UInt64* out_uuid) {
    if (!s_session || name == nullptr) {
        return 0;
    }
    const UInt64 uuid = s_session->createEntity(name);
    if (out_uuid != nullptr) {
        *out_uuid = uuid;
    }
    return uuid != 0 ? 1 : 0;
}

int Ed_DeleteEntity(UInt64 uuid) {
    return s_session && s_session->deleteEntity(uuid) ? 1 : 0;
}

int Ed_RenameEntity(UInt64 uuid, const char* name) {
    if (!s_session || name == nullptr) {
        return 0;
    }
    return s_session->renameEntity(uuid, name) ? 1 : 0;
}

int Ed_ReparentEntity(UInt64 uuid, UInt64 parent_uuid) {
    return s_session && s_session->reparentEntity(uuid, parent_uuid) ? 1 : 0;
}

bool CallJsonCommand(dodoe::ScriptCommand command, const char* argument, Json& out) {
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
        out = Json::parse(text);
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

bool EditorScriptBridge::GetCustomInspectorUI(const String& typeName, Json& out) {
    out = nullptr;
    if (typeName.empty()) {
        return false;
    }
    return CallJsonCommand(dodoe::ScriptCommand::EditorGetInspectorUI, typeName.c_str(), out);
}

bool EditorScriptBridge::ListEditorWindows(DynamicArray<Pair<String, String>>& out) {
    out.clear();
    Json json;
    if (!CallJsonCommand(dodoe::ScriptCommand::EditorListWindows, nullptr, json)) {
        return false;
    }
    if (!json.is_object() || !json.contains("windows") || !json["windows"].is_array()) {
        return false;
    }
    for (const Json& item : json["windows"]) {
        out.emplace_back(item.value("id", String()), item.value("title", String()));
    }
    return true;
}

bool EditorScriptBridge::GetEditorWindowUI(const String& id, Json& out) {
    out = nullptr;
    if (id.empty()) {
        return false;
    }
    return CallJsonCommand(dodoe::ScriptCommand::EditorGetWindowUI, id.c_str(), out);
}

bool EditorScriptBridge::DispatchEditorEvent(const String& owner, const String& ownerId,
                                             const String& controlId, const String& eventName,
                                             const Json& value) {
    if (s_call == nullptr) {
        return false;
    }
    const String valueText = ToString(value);
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
