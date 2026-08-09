#include "CommandRegistry.h"

#include "framework/EditorContext.h"
#include "framework/command/CommandStack.h"
#include "framework/command/commands/CreateEntityCommand.h"
#include "framework/command/commands/DeleteEntityCommand.h"
#include "framework/command/commands/RenameEntityCommand.h"
#include "framework/command/commands/AddComponentCommand.h"
#include "framework/command/commands/RemoveComponentCommand.h"
#include "framework/command/commands/SetFieldValueCommand.h"
#include "framework/command/commands/ReparentEntityCommand.h"
#include "framework/command/commands/PaintTilesCommand.h"
#include "framework/command/commands/CreateTilemapCommand.h"
#include "framework/command/commands/CreateTileLayerCommand.h"
#include "framework/selection/SelectionManager.h"
#include "framework/document/SceneDocument.h"
#include "framework/playmode/PlayModeController.h"
#include "framework/gizmo/GizmoService.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/function/world/components/hierarchy_component.h"
#include "runtime/function/world/components/tilemap/tilemap_component.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/meta/reflection/reflection.h"
#include "runtime/core/utils/json.h"

#include <cstdlib>
#include <cstring>

namespace cakery {

namespace {

dodoe::Json ParseNumberArray(const std::string& value, std::size_t expected, bool integer)
{
    dodoe::Json arr = dodoe::Json::array();
    std::size_t start = 0;
    while (arr.size() < expected) {
        std::size_t comma = value.find(',', start);
        std::string token = value.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        std::size_t b = token.find_first_not_of(" \t");
        if (b == std::string::npos) return dodoe::Json();
        std::size_t e = token.find_last_not_of(" \t");
        token = token.substr(b, e - b + 1);
        char* end = nullptr;
        double num = std::strtod(token.c_str(), &end);
        if (!end || *end != '\0') return dodoe::Json();
        if (integer) {
            arr.push_back(static_cast<long long>(num));
        } else {
            arr.push_back(num);
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    if (arr.size() != expected) return dodoe::Json();
    return arr;
}

dodoe::Json ParseFieldValue(const char* typeName, const std::string& value)
{
    if (!typeName || !typeName[0]) return dodoe::Json();

    if (std::strcmp(typeName, "float") == 0 || std::strcmp(typeName, "double") == 0) {
        char* end = nullptr;
        double num = std::strtod(value.c_str(), &end);
        if (!end || *end != '\0') return dodoe::Json();
        return dodoe::Json(num);
    }
    if (std::strcmp(typeName, "int") == 0 || std::strcmp(typeName, "int32_t") == 0) {
        char* end = nullptr;
        long long num = std::strtoll(value.c_str(), &end, 10);
        if (!end || *end != '\0') return dodoe::Json();
        return dodoe::Json(static_cast<int>(num));
    }
    if (std::strcmp(typeName, "unsigned int") == 0 || std::strcmp(typeName, "uint32_t") == 0) {
        char* end = nullptr;
        unsigned long long num = std::strtoull(value.c_str(), &end, 10);
        if (!end || *end != '\0') return dodoe::Json();
        return dodoe::Json(static_cast<unsigned int>(num));
    }
    if (std::strcmp(typeName, "bool") == 0) {
        if (value == "true" || value == "1") return dodoe::Json(true);
        if (value == "false" || value == "0") return dodoe::Json(false);
        return dodoe::Json();
    }
    if (std::strcmp(typeName, "Vector2f") == 0) return ParseNumberArray(value, 2, false);
    if (std::strcmp(typeName, "Vector2i") == 0) return ParseNumberArray(value, 2, true);
    if (std::strcmp(typeName, "Vector3f") == 0) return ParseNumberArray(value, 3, false);
    if (std::strcmp(typeName, "Vector3i") == 0) return ParseNumberArray(value, 3, true);
    if (std::strcmp(typeName, "Vector4f") == 0) return ParseNumberArray(value, 4, false);
    if (std::strcmp(typeName, "Vector4i") == 0) return ParseNumberArray(value, 4, true);
    if (std::strcmp(typeName, "Color") == 0) return ParseNumberArray(value, 4, false);
    if (std::strcmp(typeName, "UUID") == 0) {
        return dodoe::Json(static_cast<uint64_t>(dodoe::UUID::FromString(dodoe::String(value.c_str()))));
    }
    if (std::strcmp(typeName, "String") == 0 || std::strcmp(typeName, "dodoe::String") == 0
        || std::strcmp(typeName, "std::string") == 0) {
        return dodoe::Json(value);
    }

    dodoe::Json parsed = dodoe::Json::parse(value, nullptr, false);
    if (parsed.is_discarded()) return dodoe::Json();
    return parsed;
}

} // namespace

static dodoe::UUID primarySelection(EditorContext& ctx) {
    return ctx.selection().primary();
}

void RegisterBuiltinCommands() {
    auto& reg = CommandRegistry::self();

    reg.add({"entity.create", "Create a new entity",
             "entity.create name=<string> [parent=<uuid>] [preset=<string>]",
             {{"name", "string", "Entity name", true},
              {"parent", "uuid", "Parent entity UUID", false},
              {"preset", "string", "Preset type (Cube, Sphere, etc.)", false}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 std::string name = args.named.value("name", args.named.value("preset", "Entity"));
                 dodoe::UUID parentUuid;
                 if (args.named.contains("parent")) {
                     parentUuid = dodoe::UUID::FromString(dodoe::String(args.named["parent"].get<std::string>().c_str()));
                 }
                 auto cmd = std::make_unique<CreateEntityCommand>(
                     dodoe::UUID::Generate(), name,
                     parentUuid.isValid() ? std::optional<dodoe::UUID>(parentUuid) : std::nullopt);
                 std::string label = cmd->label();
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok(label);
             }});

    reg.add({"entity.delete", "Delete an entity and its children",
             "entity.delete <uuid>",
             {{"uuid", "uuid", "Entity UUID", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::UUID uuid;
                 if (!args.positional.empty()) {
                     uuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                 } else {
                     uuid = primarySelection(ctx);
                 }
                 if (!uuid.isValid()) return CommandResult::Err("No entity specified");
                 auto cmd = std::make_unique<DeleteEntityCommand>(uuid);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Entity deleted");
             }});

    reg.add({"entity.rename", "Rename an entity",
             "entity.rename <uuid> <name>",
             {{"uuid", "uuid", "Entity UUID", true},
              {"name", "string", "New name", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::UUID uuid;
                 std::string newName;
                 if (args.positional.size() >= 2) {
                     uuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                     newName = args.positional[1];
                 } else if (args.positional.size() == 1) {
                     uuid = primarySelection(ctx);
                     newName = args.positional[0];
                 } else {
                     return CommandResult::Err("Usage: entity.rename <uuid> <name>");
                 }
                 if (!uuid.isValid()) return CommandResult::Err("No entity specified");
                 auto* scene = ctx.activeScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 auto entity = ResolveEntity(scene, uuid);
                 if (!entity.valid()) return CommandResult::Err("Entity not found");
                 std::string oldName(entity.name().c_str());
                 auto cmd = std::make_unique<RenameEntityCommand>(uuid, oldName, newName);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Renamed to: " + newName);
             }});

    reg.add({"entity.select", "Select entities by UUID",
             "entity.select <uuid...>",
             {{"uuid", "uuid", "Entity UUID", true}},
             false,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 if (args.positional.empty()) {
                     return CommandResult::Err("Usage: entity.select <uuid...>");
                 }
                 std::vector<dodoe::UUID> uuids;
                 for (auto& p : args.positional) {
                     uuids.push_back(dodoe::UUID::FromString(dodoe::String(p.c_str())));
                 }
                 ctx.selection().selectMany(uuids);
                 return CommandResult::Ok("Selected " + std::to_string(uuids.size()) + " entities");
             }});

    reg.add({"entity.reparent", "Change entity parent",
             "entity.reparent <uuid> <parent_uuid>",
             {{"uuid", "uuid", "Entity UUID", true},
              {"parent", "uuid", "New parent UUID", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 if (args.positional.size() < 2) {
                     return CommandResult::Err("Usage: entity.reparent <uuid> <parent_uuid>");
                 }
                 dodoe::UUID childUuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                 dodoe::UUID newParentUuid = dodoe::UUID::FromString(dodoe::String(args.positional[1].c_str()));
                 auto* scene = ctx.activeScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 auto child = ResolveEntity(scene, childUuid);
                 if (!child.valid()) return CommandResult::Err("Child entity not found");
                 dodoe::UUID oldParent;
                 if (child.hasComponent<dodoe::HierarchyComponent>()) {
                     oldParent = child.getComponent<dodoe::HierarchyComponent>().parent_uuid;
                 }
                 auto cmd = std::make_unique<ReparentEntityCommand>(childUuid, oldParent, newParentUuid);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Reparented");
             }});

    reg.add({"component.add", "Add a component to an entity",
             "component.add <uuid> <Type>",
             {{"uuid", "uuid", "Entity UUID", true},
              {"type", "string", "Component type name", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::UUID uuid;
                 std::string compType;
                 if (args.positional.size() >= 2) {
                     uuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                     compType = args.positional[1];
                 } else if (args.positional.size() == 1) {
                     uuid = primarySelection(ctx);
                     compType = args.positional[0];
                 } else {
                     return CommandResult::Err("Usage: component.add <uuid> <Type>");
                 }
                 if (!uuid.isValid()) return CommandResult::Err("Invalid UUID");
                 auto cmd = std::make_unique<AddComponentCommand>(uuid, compType);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Added " + compType);
             }});

    reg.add({"component.remove", "Remove a component from an entity",
             "component.remove <uuid> <Type>",
             {{"uuid", "uuid", "Entity UUID", true},
              {"type", "string", "Component type name", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::UUID uuid;
                 std::string compType;
                 if (args.positional.size() >= 2) {
                     uuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                     compType = args.positional[1];
                 } else if (args.positional.size() == 1) {
                     uuid = primarySelection(ctx);
                     compType = args.positional[0];
                 } else {
                     return CommandResult::Err("Usage: component.remove <uuid> <Type>");
                 }
                 if (!uuid.isValid()) return CommandResult::Err("Invalid UUID");
                 auto cmd = std::make_unique<RemoveComponentCommand>(uuid, compType);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Removed " + compType);
             }});

    reg.add({"component.set", "Set a component field value",
             "component.set <uuid> <Type>.<field> <value>",
             {{"uuid", "uuid", "Entity UUID", true},
              {"field", "string", "ComponentType.fieldName", true},
              {"value", "string", "New value", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 if (args.positional.size() < 3) {
                     return CommandResult::Err("Usage: component.set <uuid> <Type>.<field> <value>");
                 }
                 dodoe::UUID uuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                 std::string compField = args.positional[1];
                 std::string value = args.positional[2];

                 auto dot = compField.find('.');
                 if (dot == std::string::npos) {
                     return CommandResult::Err("Format: Type.fieldName");
                 }
                 std::string comp = compField.substr(0, dot);
                 std::string field = compField.substr(dot + 1);

                 dodoe::TypeMeta meta = dodoe::TypeMeta::newMetaFromName(dodoe::String(comp.c_str()));
                 if (!meta.isValid()) {
                     return CommandResult::Err("Unknown component type: " + comp);
                 }
                 dodoe::FieldAccessor acc = meta.get_field_by_name(field.c_str());
                 const char* typeName = acc.getFieldTypeName();
                 if (!typeName || !typeName[0] || std::strcmp(typeName, "unknownType") == 0) {
                     return CommandResult::Err("Unknown field: " + field);
                 }

                 dodoe::Json newVal = ParseFieldValue(typeName, value);
                 if (newVal.is_null()) {
                     return CommandResult::Err("Cannot parse '" + value + "' as " + typeName);
                 }
                 dodoe::Json oldVal;
                 auto cmd = std::make_unique<SetFieldValueCommand>(uuid, comp, field, oldVal, newVal);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Set " + compField + " = " + value);
             }});

    reg.add({"tilemap.create", "Create a new tilemap entity with a default layer",
             "tilemap.create name=<string> width=<int> height=<int>",
             {{"name", "string", "Tilemap name", true},
              {"width", "int", "Map width in tiles", true},
              {"height", "int", "Map height in tiles", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 auto* scene = ctx.activeScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 std::string name = args.named.value("name", args.positional.empty() ? std::string("Tilemap") : args.positional[0]);
                 std::string wStr = args.named.value("width", args.positional.size() > 1 ? args.positional[1] : std::string());
                 std::string hStr = args.named.value("height", args.positional.size() > 2 ? args.positional[2] : std::string());
                 if (wStr.empty() || hStr.empty()) {
                     return CommandResult::Err("Usage: tilemap.create name=<string> width=<int> height=<int>");
                 }
                 char* wEnd = nullptr;
                 char* hEnd = nullptr;
                 long w = std::strtol(wStr.c_str(), &wEnd, 10);
                 long h = std::strtol(hStr.c_str(), &hEnd, 10);
                 if (!wEnd || *wEnd != '\0' || !hEnd || *hEnd != '\0' || w <= 0 || h <= 0) {
                     return CommandResult::Err("width/height must be positive integers");
                 }
                 auto cmd = std::make_unique<CreateTilemapCommand>(
                     dodoe::String(name.c_str()), static_cast<dodoe::UInt32>(w), static_cast<dodoe::UInt32>(h));
                 auto* executed = ctx.commands().execute(std::move(cmd));
                 if (!executed) return CommandResult::Err("Failed to create tilemap");
                 auto* created = static_cast<CreateTilemapCommand*>(executed);
                 return CommandResult::Ok("Created tilemap '" + name + "' ("
                                          + std::to_string(static_cast<uint64_t>(created->created())) + ")");
             }});

    reg.add({"tilemap.layer", "Create a new tile layer under a tilemap",
             "tilemap.layer <tilemap_uuid> name=<string> width=<int> height=<int>",
             {{"tilemap", "uuid", "Parent tilemap entity UUID", true},
              {"name", "string", "Layer name", true},
              {"width", "int", "Layer width in tiles", true},
              {"height", "int", "Layer height in tiles", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 auto* scene = ctx.activeScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 dodoe::UUID tilemapUuid;
                 if (!args.positional.empty()) {
                     tilemapUuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                 } else if (args.named.contains("tilemap")) {
                     tilemapUuid = dodoe::UUID::FromString(dodoe::String(args.named["tilemap"].get<std::string>().c_str()));
                 }
                 if (!tilemapUuid.isValid()) return CommandResult::Err("No tilemap UUID specified");
                 auto tilemapEntity = ResolveEntity(scene, tilemapUuid);
                 if (!tilemapEntity.valid()) return CommandResult::Err("Tilemap entity not found");
                 if (!tilemapEntity.hasComponent<dodoe::TilemapComponent>()) {
                     return CommandResult::Err("Entity is not a tilemap");
                 }
                 std::string name = args.named.value("name", args.positional.size() > 1 ? args.positional[1] : std::string("Layer"));
                 std::string wStr = args.named.value("width", args.positional.size() > 2 ? args.positional[2] : std::string());
                 std::string hStr = args.named.value("height", args.positional.size() > 3 ? args.positional[3] : std::string());
                 if (wStr.empty() || hStr.empty()) {
                     return CommandResult::Err("Usage: tilemap.layer <tilemap_uuid> name=<string> width=<int> height=<int>");
                 }
                 char* wEnd = nullptr;
                 char* hEnd = nullptr;
                 long w = std::strtol(wStr.c_str(), &wEnd, 10);
                 long h = std::strtol(hStr.c_str(), &hEnd, 10);
                 if (!wEnd || *wEnd != '\0' || !hEnd || *hEnd != '\0' || w <= 0 || h <= 0) {
                     return CommandResult::Err("width/height must be positive integers");
                 }
                 auto cmd = std::make_unique<CreateTileLayerCommand>(
                     tilemapUuid, dodoe::String(name.c_str()), static_cast<dodoe::UInt32>(w), static_cast<dodoe::UInt32>(h));
                 auto* executed = ctx.commands().execute(std::move(cmd));
                 if (!executed) return CommandResult::Err("Failed to create layer");
                 auto* created = static_cast<CreateTileLayerCommand*>(executed);
                 return CommandResult::Ok("Created layer '" + name + "' on tilemap ("
                                          + std::to_string(static_cast<uint64_t>(created->created())) + ")");
             }});

    reg.add({"scene.new", "Create a new empty scene",
             "scene.new [name]",
             {{"name", "string", "Scene name", false}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 std::string name = args.named.value("name", args.positional.empty() ? "Untitled" : args.positional[0]);
                 ctx.document().newScene(name);
                 return CommandResult::Ok("New scene: " + name);
             }});

    reg.add({"scene.save", "Save current scene",
             "scene.save",
             {},
             true,
             [](EditorContext& ctx, const CommandArgs&) -> CommandResult {
                 if (ctx.document().save()) {
                     return CommandResult::Ok("Scene saved");
                 }
                 return CommandResult::Err("Save failed");
             }});

    reg.add({"query.entities", "List entities in the active scene",
             "query.entities",
             {},
             false,
             [](EditorContext& ctx, const CommandArgs&) -> CommandResult {
                 auto* scene = ctx.activeScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 dodoe::Json result = dodoe::Json::array();
                 auto entities = scene->getEntities();
                 for (auto& entity : entities) {
                     dodoe::Json e;
                     e["name"] = entity.name().c_str();
                     e["uuid"] = std::to_string(static_cast<uint64_t>(entity.uuid()));
                     result.push_back(e);
                 }
                 return CommandResult::Ok(std::to_string(result.size()) + " entities", result);
             }});

    reg.add({"query.components", "List components on an entity",
             "query.components <uuid>",
             {{"uuid", "uuid", "Entity UUID", true}},
             false,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::UUID uuid;
                 if (!args.positional.empty()) {
                     uuid = dodoe::UUID::FromString(dodoe::String(args.positional[0].c_str()));
                 } else {
                     uuid = primarySelection(ctx);
                 }
                 if (!uuid.isValid()) return CommandResult::Err("No entity specified");
                 auto* scene = ctx.activeScene();
                 if (!scene) return CommandResult::Err("No active scene");
                 auto entity = ResolveEntity(scene, uuid);
                 if (!entity.valid()) return CommandResult::Err("Entity not found");
                 dodoe::Json result = dodoe::Json::array();
                 auto& db = dodoe::ComponentDB::self();
                 for (auto& entry : db.entries()) {
                     if (db.hasComponent(entity, entry.name)) {
                         result.push_back(entry.name.c_str());
                     }
                 }
                 return CommandResult::Ok("", result);
             }});

    reg.add({"history.undo", "Undo last command",
             "history.undo", {}, true,
             [](EditorContext& ctx, const CommandArgs&) -> CommandResult {
                 if (!ctx.commands().canUndo()) return CommandResult::Err("Nothing to undo");
                 ctx.commands().undo();
                 return CommandResult::Ok("Undo");
             }});

    reg.add({"history.redo", "Redo last undone command",
             "history.redo", {}, true,
             [](EditorContext& ctx, const CommandArgs&) -> CommandResult {
                 if (!ctx.commands().canRedo()) return CommandResult::Err("Nothing to redo");
                 ctx.commands().redo();
                 return CommandResult::Ok("Redo");
             }});

    reg.add({"scene.open", "Open a scene file",
             "scene.open [path]",
             {{"path", "string", "Scene file path", false}}, true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 std::string path = args.named.value("path", args.positional.empty() ? "" : args.positional[0]);
                 if (path.empty()) return CommandResult::Err("No path specified");
                 ctx.document().openScene(path);
                 return CommandResult::Ok("Opened " + path);
             }});

    reg.add({"scene.saveas", "Save scene as new file",
             "scene.saveas", {}, true,
             [](EditorContext&, const CommandArgs&) -> CommandResult {
                 return CommandResult::Ok();
             }});

    reg.add({"asset.import", "Import an asset into the project",
             "asset.import [path]", {}, true,
             [](EditorContext&, const CommandArgs&) -> CommandResult {
                 return CommandResult::Ok();
             }});

    reg.add({"playmode.play", "Enter play mode",
             "playmode.play", {}, true,
             [](EditorContext& ctx, const CommandArgs&) -> CommandResult {
                 auto& pm = ctx.playMode();
                 if (pm.state() == PlayState::Edit) pm.play(); else pm.stop();
                 return CommandResult::Ok();
             }});

    reg.add({"playmode.pause", "Pause or resume play mode",
             "playmode.pause", {}, true,
             [](EditorContext& ctx, const CommandArgs&) -> CommandResult {
                 auto& pm = ctx.playMode();
                 if (pm.state() == PlayState::Playing) pm.pause();
                 else if (pm.state() == PlayState::Paused) pm.resume();
                 return CommandResult::Ok();
             }});

    reg.add({"playmode.stop", "Stop play mode",
             "playmode.stop", {}, true,
             [](EditorContext& ctx, const CommandArgs&) -> CommandResult {
                 ctx.playMode().stop();
                 return CommandResult::Ok();
             }});

    reg.add({"gizmo.setmode", "Set gizmo tool mode",
             "gizmo.setmode mode=<Translate|Rotate|Scale|None>",
             {{"mode", "string", "Gizmo mode", true}}, false,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 std::string mode = args.named.value("mode", args.positional.empty() ? "" : args.positional[0]);
                 if (mode == "Translate" || mode == "Move") ctx.gizmos().setMode(GizmoMode::Translate);
                 else if (mode == "Rotate") ctx.gizmos().setMode(GizmoMode::Rotate);
                 else if (mode == "Scale") ctx.gizmos().setMode(GizmoMode::Scale);
                 else ctx.gizmos().setMode(GizmoMode::None);
                 return CommandResult::Ok("Gizmo: " + mode);
             }});

    reg.add({"layout.menu", "Show layout context menu",
             "layout.menu", {}, false,
             [](EditorContext&, const CommandArgs&) -> CommandResult {
                 return CommandResult::Ok();
             }});
}

} // namespace cakery
