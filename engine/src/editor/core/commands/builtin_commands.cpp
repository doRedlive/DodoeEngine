// do@Redlive

#include "core/console/CommandRegistry.h"

#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"
#include "core/EditorSession.h"

#include <cstdlib>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace cakery {

namespace {

std::uint64_t ParseUuid(const std::string& text) {
    if (text.empty()) return 0;
    return static_cast<std::uint64_t>(std::strtoull(text.c_str(), nullptr, 10));
}

std::uint64_t primarySelection(EditorSession& session) {
    return session.selection().primary();
}

std::size_t FindComponentIndex(EditorSession& session, std::uint64_t uuid, const std::string& typeName) {
    const EditorEntity* entity = session.documentModel().findEntity(uuid);
    if (!entity) return static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        if (entity->nativeComponents[i].typeName == typeName) return i;
    }
    return static_cast<std::size_t>(-1);
}

nlohmann::json ParseNumberArray(const std::string& value, std::size_t expected, bool integer) {
    nlohmann::json arr = nlohmann::json::array();
    std::size_t start = 0;
    while (arr.size() < expected) {
        std::size_t comma = value.find(',', start);
        std::string token = value.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        std::size_t b = token.find_first_not_of(" \t");
        if (b == std::string::npos) return nlohmann::json();
        std::size_t e = token.find_last_not_of(" \t");
        token = token.substr(b, e - b + 1);
        char* end = nullptr;
        double num = std::strtod(token.c_str(), &end);
        if (!end || *end != '\0') return nlohmann::json();
        if (integer) {
            arr.push_back(static_cast<long long>(num));
        } else {
            arr.push_back(num);
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    if (arr.size() != expected) return nlohmann::json();
    return arr;
}

nlohmann::json ParseFieldValue(const nlohmann::json& currentValue, const std::string& value) {
    if (currentValue.is_number_integer() || currentValue.is_number_unsigned()) {
        char* end = nullptr;
        long long num = std::strtoll(value.c_str(), &end, 10);
        if (!end || *end != '\0') return nlohmann::json();
        return nlohmann::json(num);
    }
    if (currentValue.is_number_float()) {
        char* end = nullptr;
        double num = std::strtod(value.c_str(), &end);
        if (!end || *end != '\0') return nlohmann::json();
        return nlohmann::json(num);
    }
    if (currentValue.is_boolean()) {
        if (value == "true" || value == "1") return nlohmann::json(true);
        if (value == "false" || value == "0") return nlohmann::json(false);
        return nlohmann::json();
    }
    if (currentValue.is_array()) {
        bool integer = !currentValue.empty() &&
            (currentValue[0].is_number_integer() || currentValue[0].is_number_unsigned());
        return ParseNumberArray(value, currentValue.size(), integer);
    }
    if (currentValue.is_string()) {
        return nlohmann::json(value);
    }
    nlohmann::json parsed = nlohmann::json::parse(value, nullptr, false);
    if (parsed.is_discarded()) return nlohmann::json();
    return parsed;
}

} // namespace

void RegisterBuiltinCommands() {
    static bool registered = false;
    if (registered) return;
    registered = true;

    auto& reg = CommandRegistry::self();

    reg.add({"entity.create", "Create a new GameObject",
             "entity.create name=<string> [parent=<uuid>] [preset=<string>]",
             {{"name", "string", "GameObject name", true},
              {"parent", "uuid", "Parent GameObject UUID", false},
              {"preset", "string", "Preset type (Cube, Sphere, etc.)", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::string name = args.named.value("name", args.named.value("preset", "GameObject"));
                 auto cmd = std::make_unique<CreateEntityCommand>(name);
                 std::string label = cmd->label();
                 session.history().execute(std::move(cmd), session.documentModel());
                 session.notifyDocumentChanged();
                 return CommandResult::Ok(label);
             }});

    reg.add({"entity.delete", "Delete a GameObject and its children",
             "entity.delete <uuid>",
             {{"uuid", "uuid", "GameObject UUID", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::uint64_t uuid;
                 if (!args.positional.empty()) {
                     uuid = ParseUuid(args.positional[0]);
                 } else {
                     uuid = primarySelection(session);
                 }
                 if (uuid == 0) return CommandResult::Err("No GameObject specified");
                 session.history().execute(std::make_unique<DeleteEntityCommand>(uuid), session.documentModel());
                 session.selection().remove(uuid);
                 session.notifyDocumentChanged();
                 return CommandResult::Ok("GameObject deleted");
             }});

    reg.add({"entity.rename", "Rename a GameObject",
             "entity.rename <uuid> <name>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"name", "string", "New name", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::uint64_t uuid;
                 std::string newName;
                 if (args.positional.size() >= 2) {
                     uuid = ParseUuid(args.positional[0]);
                     newName = args.positional[1];
                 } else if (args.positional.size() == 1) {
                     uuid = primarySelection(session);
                     newName = args.positional[0];
                 } else {
                     return CommandResult::Err("Usage: entity.rename <uuid> <name>");
                 }
                 if (uuid == 0) return CommandResult::Err("No GameObject specified");
                 const EditorEntity* entity = session.documentModel().findEntity(uuid);
                 if (!entity) return CommandResult::Err("GameObject not found");
                 session.history().execute(std::make_unique<RenameEntityCommand>(uuid, newName), session.documentModel());
                 session.notifyDocumentChanged();
                 return CommandResult::Ok("Renamed to: " + newName);
             }});

    reg.add({"entity.select", "Select GameObjects by UUID",
             "entity.select <uuid...>",
             {{"uuid", "uuid", "GameObject UUID", true}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 if (args.positional.empty()) {
                     return CommandResult::Err("Usage: entity.select <uuid...>");
                 }
                 std::vector<std::uint64_t> uuids;
                 for (auto& p : args.positional) {
                     uuids.push_back(ParseUuid(p));
                 }
                 session.selection().selectMany(uuids);
                 return CommandResult::Ok("Selected " + std::to_string(uuids.size()) + " GameObjects");
             }});

    reg.add({"component.add", "Add a component to a GameObject",
             "component.add <uuid> <Type>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"type", "string", "Component type name", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::uint64_t uuid;
                 std::string compType;
                 if (args.positional.size() >= 2) {
                     uuid = ParseUuid(args.positional[0]);
                     compType = args.positional[1];
                 } else if (args.positional.size() == 1) {
                     uuid = primarySelection(session);
                     compType = args.positional[0];
                 } else {
                     return CommandResult::Err("Usage: component.add <uuid> <Type>");
                 }
                 if (uuid == 0) return CommandResult::Err("Invalid UUID");
                 session.history().execute(
                     std::make_unique<AddComponentCommand>(uuid, EditorComponent{compType, nlohmann::json::object()}),
                     session.documentModel());
                 session.notifyDocumentChanged();
                 return CommandResult::Ok("Added " + compType);
             }});

    reg.add({"component.remove", "Remove a component from a GameObject",
             "component.remove <uuid> <Type>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"type", "string", "Component type name", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::uint64_t uuid;
                 std::string compType;
                 if (args.positional.size() >= 2) {
                     uuid = ParseUuid(args.positional[0]);
                     compType = args.positional[1];
                 } else if (args.positional.size() == 1) {
                     uuid = primarySelection(session);
                     compType = args.positional[0];
                 } else {
                     return CommandResult::Err("Usage: component.remove <uuid> <Type>");
                 }
                 if (uuid == 0) return CommandResult::Err("Invalid UUID");
                 std::size_t idx = FindComponentIndex(session, uuid, compType);
                 if (idx == static_cast<std::size_t>(-1)) {
                     return CommandResult::Err("Component not found: " + compType);
                 }
                 session.history().execute(std::make_unique<RemoveComponentCommand>(uuid, idx), session.documentModel());
                 session.notifyDocumentChanged();
                 return CommandResult::Ok("Removed " + compType);
             }});

    reg.add({"component.set", "Set a component field value",
             "component.set <uuid> <Type>.<field> <value>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"field", "string", "ComponentType.fieldName", true},
              {"value", "string", "New value", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 if (args.positional.size() < 3) {
                     return CommandResult::Err("Usage: component.set <uuid> <Type>.<field> <value>");
                 }
                 std::uint64_t uuid = ParseUuid(args.positional[0]);
                 std::string compField = args.positional[1];
                 std::string value = args.positional[2];

                 auto dot = compField.find('.');
                 if (dot == std::string::npos) {
                     return CommandResult::Err("Format: Type.fieldName");
                 }
                 std::string comp = compField.substr(0, dot);
                 std::string field = compField.substr(dot + 1);

                 std::size_t idx = FindComponentIndex(session, uuid, comp);
                 if (idx == static_cast<std::size_t>(-1)) {
                     return CommandResult::Err("Unknown component type: " + comp);
                 }
                 const EditorEntity* entity = session.documentModel().findEntity(uuid);
                 if (!entity) return CommandResult::Err("GameObject not found");
                 const nlohmann::json& compValue = entity->nativeComponents[idx].value;
                 if (!compValue.contains(field)) {
                     return CommandResult::Err("Unknown field: " + field);
                 }
                 nlohmann::json newVal = ParseFieldValue(compValue[field], value);
                 if (newVal.is_null()) {
                     return CommandResult::Err("Cannot parse '" + value + "' for field " + field);
                 }
                 session.history().execute(
                     std::make_unique<SetFieldValueCommand>(uuid, idx, field, newVal),
                     session.documentModel());
                 session.notifyDocumentChanged();
                 return CommandResult::Ok("Set " + compField + " = " + value);
             }});

    reg.add({"scene.new", "Create a new empty scene",
             "scene.new [name]",
             {{"name", "string", "Scene name", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::string name = args.named.value("name", args.positional.empty() ? "Untitled" : args.positional[0]);
                 session.documentModel().newScene(name);
                 session.history().clear();
                 session.selection().clear();
                 session.notifyDocumentChanged();
                 return CommandResult::Ok("New scene: " + name);
             }});

    reg.add({"scene.save", "Save current scene",
             "scene.save",
             {},
             true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 if (!session.saveDocument("")) {
                     return CommandResult::Err("Save failed (no scene or no file path)");
                 }
                 return CommandResult::Ok("Scene saved");
             }});

    reg.add({"scene.saveas", "Save scene as new file",
             "scene.saveas [path]",
             {{"path", "string", "Scene file path", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::string path = args.named.value("path", args.positional.empty() ? "" : args.positional[0]);
                 if (path.empty()) return CommandResult::Err("No path specified");
                 if (!session.saveDocument(path)) return CommandResult::Err("Save failed");
                 return CommandResult::Ok("Saved to " + path);
             }});

    reg.add({"scene.open", "Open a scene file",
             "scene.open [path]",
             {{"path", "string", "Scene file path", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::string path = args.named.value("path", args.positional.empty() ? "" : args.positional[0]);
                 if (path.empty()) return CommandResult::Err("No path specified");
                 if (!session.openDocument(path)) return CommandResult::Err("Failed to open " + path);
                 return CommandResult::Ok("Opened " + path);
             }});

    reg.add({"asset.save_all", "Save all modified assets",
             "asset.save_all",
             {},
             true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.execute({"asset.save_all", ""});
                 return CommandResult::Ok("Save requested");
             }});

    reg.add({"asset.import", "Import an asset into the project",
             "asset.import [path]",
             {{"path", "string", "Asset file path", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const std::string path = args.named.value(
                     "path", args.positional.empty() ? std::string() : args.positional[0]);
                 if (path.empty()) {
                     return CommandResult::Err("No asset path specified");
                 }
                 if (!session.execute({"asset.import", path})) {
                     return CommandResult::Err("Asset import request failed");
                 }
                 return CommandResult::Ok("Import requested: " + path);
             }});

    reg.add({"asset.reimport", "Reimport an existing project asset",
             "asset.reimport <path>",
             {{"path", "string", "Asset file path", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const std::string path = args.named.value(
                     "path", args.positional.empty() ? std::string() : args.positional[0]);
                 if (path.empty()) {
                     return CommandResult::Err("No asset path specified");
                 }
                 if (!session.execute({"asset.reimport", path})) {
                     return CommandResult::Err("Asset reimport request failed");
                 }
                 return CommandResult::Ok("Reimport requested: " + path);
             }});

    reg.add({"query.entities", "List GameObjects in the active scene",
             "query.entities",
             {},
             false,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 if (!session.documentModel().hasDocument()) return CommandResult::Err("No active scene");
                 nlohmann::json result = nlohmann::json::array();
                 for (const EditorEntity& entity : session.documentModel().entities()) {
                     nlohmann::json e;
                     e["name"] = entity.name;
                     e["uuid"] = std::to_string(entity.uuid);
                     result.push_back(e);
                 }
                 return CommandResult::Ok(std::to_string(result.size()) + " GameObjects", result);
             }});

    reg.add({"query.components", "List components on a GameObject",
             "query.components <uuid>",
             {{"uuid", "uuid", "GameObject UUID", true}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::uint64_t uuid;
                 if (!args.positional.empty()) {
                     uuid = ParseUuid(args.positional[0]);
                 } else {
                     uuid = primarySelection(session);
                 }
                 if (uuid == 0) return CommandResult::Err("No GameObject specified");
                 const EditorEntity* entity = session.documentModel().findEntity(uuid);
                 if (!entity) return CommandResult::Err("GameObject not found");
                 nlohmann::json result = nlohmann::json::array();
                 for (const auto& comp : entity->nativeComponents) {
                     result.push_back(comp.typeName);
                 }
                 return CommandResult::Ok("", result);
             }});

    reg.add({"query.assets", "Search imported project assets",
             "query.assets [filter]",
             {{"filter", "string", "Name, path, type, extension, or GUID substring", false}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const std::string filter = args.named.value(
                     "filter", args.positional.empty() ? std::string() : args.positional[0]);
                 std::vector<AssetBrowserEntry> assets;
                 if (!session.listAssets(assets)) {
                     return CommandResult::Err("Asset database is unavailable");
                 }
                 nlohmann::json result = nlohmann::json::array();
                 for (const auto& asset : assets) {
                     if (!filter.empty()) {
                         const std::string haystack = asset.name + " " + asset.path + " " +
                             asset.type + " " + asset.extension + " " +
                             std::to_string(static_cast<std::uint64_t>(asset.uuid));
                         if (haystack.find(filter) == std::string::npos) {
                             continue;
                         }
                     }
                     nlohmann::json item;
                     item["uuid"] = std::to_string(static_cast<std::uint64_t>(asset.uuid));
                     item["name"] = asset.name;
                     item["path"] = asset.path;
                     item["type"] = asset.type;
                     item["extension"] = asset.extension;
                     item["dirty"] = asset.dirty;
                     item["dependencies"] = asset.dependencies;
                     result.push_back(std::move(item));
                 }
                 return CommandResult::Ok(std::to_string(result.size()) + " assets", result);
             }});

    reg.add({"history.undo", "Undo last command",
             "history.undo", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 if (!session.history().canUndo()) return CommandResult::Err("Nothing to undo");
                 session.undo();
                 return CommandResult::Ok("Undo");
             }});

    reg.add({"history.redo", "Redo last undone command",
             "history.redo", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 if (!session.history().canRedo()) return CommandResult::Err("Nothing to redo");
                 session.redo();
                 return CommandResult::Ok("Redo");
             }});

    reg.add({"playmode.play", "Enter play mode",
             "playmode.play", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.execute({"play", ""});
                 return CommandResult::Ok();
             }});

    reg.add({"playmode.pause", "Pause or resume play mode",
             "playmode.pause", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.execute({"pause", ""});
                 return CommandResult::Ok();
             }});

    reg.add({"playmode.stop", "Stop play mode",
             "playmode.stop", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.execute({"stop", ""});
                 return CommandResult::Ok();
             }});

    reg.add({"gizmo.setmode", "Set gizmo tool mode",
             "gizmo.setmode mode=<Translate|Rotate|Scale|None>",
             {{"mode", "string", "Gizmo mode", true}}, false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 std::string mode = args.named.value("mode", args.positional.empty() ? "" : args.positional[0]);
                 std::string backendMode = "none";
                 if (mode == "Translate" || mode == "Move") backendMode = "translate";
                 else if (mode == "Rotate") backendMode = "rotate";
                 else if (mode == "Scale") backendMode = "scale";
                 session.execute({"gizmo_mode", backendMode});
                 return CommandResult::Ok("Gizmo: " + mode);
             }});

    reg.add({"layout.menu", "Show layout context menu",
             "layout.menu", {}, false,
             [](EditorSession&, const CommandArgs&) -> CommandResult {
                 return CommandResult::Ok();
             }});
}

} // namespace cakery
