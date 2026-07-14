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
#include "framework/selection/SelectionManager.h"
#include "framework/document/SceneDocument.h"
#include "framework/playmode/PlayModeController.h"
#include "framework/gizmo/GizmoService.h"
#include "framework/core/UuidResolve.h"

#include "runtime/function/world/scene.h"
#include "runtime/function/world/entity.h"
#include "runtime/core/meta/component_db.h"
#include "runtime/core/utils/json.h"

namespace cakery {

static dodoe::Uuid primarySelection(EditorContext& ctx) {
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
                 dodoe::Uuid parentUuid;
                 if (args.named.contains("parent")) {
                     parentUuid = dodoe::Uuid::fromString(args.named["parent"].get<std::string>());
                 }
                 auto cmd = std::make_unique<CreateEntityCommand>(
                     dodoe::Uuid::generate(), name,
                     parentUuid.isValid() ? std::optional<dodoe::Uuid>(parentUuid) : std::nullopt);
                 std::string label = cmd->label();
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok(label);
             }});

    reg.add({"entity.delete", "Delete an entity and its children",
             "entity.delete <uuid>",
             {{"uuid", "uuid", "Entity UUID", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::Uuid uuid;
                 if (!args.positional.empty()) {
                     uuid = dodoe::Uuid::fromString(args.positional[0]);
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
                 dodoe::Uuid uuid;
                 std::string newName;
                 if (args.positional.size() >= 2) {
                     uuid = dodoe::Uuid::fromString(args.positional[0]);
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
                 std::string oldName = entity.name();
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
                 std::vector<dodoe::Uuid> uuids;
                 for (auto& p : args.positional) {
                     uuids.push_back(dodoe::Uuid::fromString(p));
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
                 dodoe::Uuid childUuid = dodoe::Uuid::fromString(args.positional[0]);
                 dodoe::Uuid newParentUuid = dodoe::Uuid::fromString(args.positional[1]);
                 auto cmd = std::make_unique<ReparentEntityCommand>(childUuid, newParentUuid);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Reparented");
             }});

    reg.add({"component.add", "Add a component to an entity",
             "component.add <uuid> <Type>",
             {{"uuid", "uuid", "Entity UUID", true},
              {"type", "string", "Component type name", true}},
             true,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::Uuid uuid;
                 std::string compType;
                 if (args.positional.size() >= 2) {
                     uuid = dodoe::Uuid::fromString(args.positional[0]);
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
                 dodoe::Uuid uuid;
                 std::string compType;
                 if (args.positional.size() >= 2) {
                     uuid = dodoe::Uuid::fromString(args.positional[0]);
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
                 dodoe::Uuid uuid = dodoe::Uuid::fromString(args.positional[0]);
                 std::string compField = args.positional[1];
                 std::string value = args.positional[2];

                 auto dot = compField.find('.');
                 if (dot == std::string::npos) {
                     return CommandResult::Err("Format: Type.fieldName");
                 }
                 std::string comp = compField.substr(0, dot);
                 std::string field = compField.substr(dot + 1);

                 dodoe::Json oldVal;
                 dodoe::Json newVal = value;
                 auto cmd = std::make_unique<SetFieldValueCommand>(uuid, comp, field, oldVal, newVal);
                 ctx.commands().execute(std::move(cmd));
                 return CommandResult::Ok("Set " + compField + " = " + value);
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
                 scene->each([&](dodoe::Entity entity) {
                     dodoe::Json e;
                     e["name"] = entity.name();
                     e["uuid"] = std::to_string(static_cast<uint64_t>(entity.uuid()));
                     result.push_back(e);
                 });
                 return CommandResult::Ok(std::to_string(result.size()) + " entities", result);
             }});

    reg.add({"query.components", "List components on an entity",
             "query.components <uuid>",
             {{"uuid", "uuid", "Entity UUID", true}},
             false,
             [](EditorContext& ctx, const CommandArgs& args) -> CommandResult {
                 dodoe::Uuid uuid;
                 if (!args.positional.empty()) {
                     uuid = dodoe::Uuid::fromString(args.positional[0]);
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
                         result.push_back(entry.name);
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
