// do@Redlive

#include "dopch.h"

#include "core/console/CommandRegistry.h"
#include "core/console/AICommandBridge.h"
#include "core/console/ExecutionRecorder.h"
#include "core/console/McpServer.h"
#include "core/console/WorkflowTemplate.h"

#include "core/commands/EditorCommand.h"
#include "core/document/EditorDocumentModel.h"
#include "core/EditorSession.h"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cakery {

namespace {

UInt64 ParseUuid(const String& text) {
    if (text.empty()) return 0;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (!end || *end != '\0' || value == 0) return 0;
    return static_cast<UInt64>(value);
}

UInt64 primarySelection(EditorSession& session) {
    return session.selection().primary();
}

Size_t FindComponentIndex(EditorSession& session, UInt64 uuid, const String& typeName) {
    const EditorEntity* entity = session.documentModel().findEntity(uuid);
    if (!entity) return static_cast<Size_t>(-1);
    for (Size_t i = 0; i < entity->nativeComponents.size(); ++i) {
        if (entity->nativeComponents[i].typeName == typeName) return i;
    }
    return static_cast<Size_t>(-1);
}

const Json* FindPathValue(const Json& root, const String& path) {
    const Json* node = &root;
    Size_t start = 0;
    while (start <= path.size()) {
        const Size_t dot = path.find('.', start);
        const Size_t end = dot == String::npos ? path.size() : dot;
        if (!node->is_object()) {
            return nullptr;
        }
        const auto it = node->find(path.substr(start, end - start));
        if (it == node->end()) {
            return nullptr;
        }
        node = &(*it);
        if (dot == String::npos) {
            break;
        }
        start = dot + 1;
    }
    return node;
}

Json ParseNumberArray(const String& value, Size_t expected, bool integer) {    Json arr = Json::array();
    Size_t start = 0;
    while (arr.size() < expected) {
        Size_t comma = value.find(',', start);
        String token = value.substr(start, comma == String::npos ? String::npos : comma - start);
        Size_t b = token.find_first_not_of(" \t");
        if (b == String::npos) return Json();
        Size_t e = token.find_last_not_of(" \t");
        token = token.substr(b, e - b + 1);
        char* end = nullptr;
        double num = std::strtod(token, &end);
        if (!end || *end != '\0') return Json();
        if (integer) {
            arr.push_back(static_cast<long long>(num));
        } else {
            arr.push_back(num);
        }
        if (comma == String::npos) break;
        start = comma + 1;
    }
    if (arr.size() != expected) return Json();
    return arr;
}

Json ParseFieldValue(const Json& currentValue, const String& value) {
    if (currentValue.is_number_integer() || currentValue.is_number_unsigned()) {
        char* end = nullptr;
        long long num = std::strtoll(value.c_str(), &end, 10);
        if (!end || *end != '\0') return Json();
        return Json(num);
    }
    if (currentValue.is_number_float()) {
        char* end = nullptr;
        double num = std::strtod(value, &end);
        if (!end || *end != '\0') return Json();
        return Json(num);
    }
    if (currentValue.is_boolean()) {
        if (value == "true" || value == "1") return Json(true);
        if (value == "false" || value == "0") return Json(false);
        return Json();
    }
    if (currentValue.is_array()) {
        bool integer = !currentValue.empty() &&
            (currentValue[0].is_number_integer() || currentValue[0].is_number_unsigned());
        return ParseNumberArray(value, currentValue.size(), integer);
    }
    if (currentValue.is_string()) {
        return Json(value);
    }
    Json parsed = Json::parse(value, nullptr, false);
    if (parsed.is_discarded()) return Json();
    return parsed;
}

} // namespace

void RegisterBuiltinCommands(CommandRegistry& registry) {
    auto& reg = registry;

    reg.add({"entity.create", "Create a new GameObject",
             "entity.create name=<string> [parent=<uuid>] [preset=<string>]",
             {{"name", "string", "GameObject name", true},
              {"parent", "uuid", "Parent GameObject UUID", false},
              {"preset", "string", "Preset type (Cube, Sphere, etc.)", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                  String name = args.named.value("name", args.named.value("preset", "GameObject"));
                  const UInt64 uuid = session.createEntity(name);
                  if (uuid == 0) {
                      return CommandResult::Err("Failed to create GameObject");
                  }
                  return CommandResult::Ok("Create GameObject",
                                           Json{{"uuid", ToString(uuid)}, {"name", name}});
              }});

    reg.add({"entity.delete", "Delete a GameObject and its children",
             "entity.delete <uuid>",
             {{"uuid", "uuid", "GameObject UUID", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                  UInt64 uuid;
                  if (!args.positional.empty()) {
                      uuid = ParseUuid(args.positional[0]);
                  } else {
                      uuid = primarySelection(session);
                   }
                   if (uuid == 0) return CommandResult::Err("No GameObject specified");
                   if (!session.deleteEntity(uuid)) {
                       return CommandResult::Err("GameObject not found");
                   }
                   return CommandResult::Ok("GameObject deleted",
                                            Json{{"uuid", ToString(uuid)}});
              }});

    reg.add({"entity.rename", "Rename a GameObject",
             "entity.rename <uuid> <name>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"name", "string", "New name", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 UInt64 uuid;
                 String newName;
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
                   if (!session.renameEntity(uuid, newName)) {
                       return CommandResult::Err("Failed to rename GameObject");
                   }
                   return CommandResult::Ok("Renamed to: " + newName,
                                            Json{{"uuid", ToString(uuid)}, {"name", newName}});
              }});

    reg.add({"entity.select", "Select GameObjects by UUID",
             "entity.select <uuid...>",
             {{"uuid", "uuid", "GameObject UUID", true}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 if (args.positional.empty()) {
                     return CommandResult::Err("Usage: entity.select <uuid...>");
                 }
                 DynamicArray<UInt64> uuids;
                 for (auto& p : args.positional) {
                     uuids.push_back(ParseUuid(p));
                 }
                 session.selection().selectMany(uuids);
                 return CommandResult::Ok("Selected " + ToString(uuids.size()) + " GameObjects");
             }});

    reg.add({"component.add", "Add a component to a GameObject",
             "component.add <uuid> <Type>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"type", "string", "Component type name", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 UInt64 uuid;
                 String compType;
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
                  if (!session.addComponent(uuid, EditorComponent{compType, Json::object()})) {
                      return CommandResult::Err("GameObject not found");
                  }
                  return CommandResult::Ok("Added " + compType,
                                           Json{{"uuid", ToString(uuid)}, {"type", compType}});
              }});

    reg.add({"component.remove", "Remove a component from a GameObject",
             "component.remove <uuid> <Type>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"type", "string", "Component type name", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 UInt64 uuid;
                 String compType;
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
                 Size_t idx = FindComponentIndex(session, uuid, compType);
                 if (idx == static_cast<Size_t>(-1)) {
                     return CommandResult::Err("Component not found: " + compType);
                 }
                   if (!session.removeComponent(uuid, idx)) {
                       return CommandResult::Err("Failed to remove component: " + compType);
                   }
                   return CommandResult::Ok("Removed " + compType,
                                            Json{{"uuid", ToString(uuid)}, {"type", compType}});
              }});

    reg.add({"component.set", "Set a component field value (supports nested dotted paths like transform.position.x)",
             "component.set <uuid> <Type>.<field> <value>",
             {{"uuid", "uuid", "GameObject UUID", true},
              {"field", "string", "ComponentType.fieldName (dotted path for nested fields)", true},
              {"value", "string", "New value", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 if (args.positional.size() < 3) {
                     return CommandResult::Err("Usage: component.set <uuid> <Type>.<field> <value>");
                 }
                 UInt64 uuid = ParseUuid(args.positional[0]);
                 String compField = args.positional[1];
                 String value = args.positional[2];

                 auto dot = compField.find('.');
                 if (dot == String::npos) {
                     return CommandResult::Err("Format: Type.fieldName");
                 }
                 String comp = compField.substr(0, dot);
                 String field = compField.substr(dot + 1);
                 if (field.empty()) {
                     return CommandResult::Err("Format: Type.fieldName");
                 }

                 Size_t idx = FindComponentIndex(session, uuid, comp);
                 if (idx == static_cast<Size_t>(-1)) {
                     return CommandResult::Err("Unknown component type: " + comp);
                 }
                 const EditorEntity* entity = session.documentModel().findEntity(uuid);
                 if (!entity) return CommandResult::Err("GameObject not found");
                 const Json& compValue = entity->nativeComponents[idx].value;
                 const bool flat = compValue.is_object() && compValue.contains(field);
                 const Json* current = flat ? &compValue[field]
                                                      : FindPathValue(compValue, field);
                 if (!current) {
                     return CommandResult::Err("Unknown field: " + field);
                 }
                 Json newVal = ParseFieldValue(*current, value);
                 if (newVal.is_null()) {
                     return CommandResult::Err("Cannot parse '" + value + "' for field " + field);
                 }
                 if (flat) {
                     if (!session.history().execute(
                             create_scope<SetFieldValueCommand>(uuid, idx, field, newVal),
                             session.documentModel())) {
                         return CommandResult::Err("Value is unchanged or the update failed");
                     }
                     session.notifyDocumentChanged();
                 } else {
                     if (!session.updateComponentFieldOnEntities({uuid}, comp, field, newVal, false)) {
                         return CommandResult::Err("Value is unchanged or the update failed");
                     }
                 }
                 return CommandResult::Ok("Set " + compField + " = " + value,
                                          Json{{"uuid", ToString(uuid)},
                                                         {"field", compField},
                                                         {"value", newVal}});
               }});

    reg.add({"scene.new", "Create a new empty scene",
             "scene.new [name]",
             {{"name", "string", "Scene name", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 String name = args.named.value("name", args.positional.empty() ? "Untitled" : args.positional[0]);
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
                 String path = args.named.value("path", args.positional.empty() ? "" : args.positional[0]);
                 if (path.empty()) return CommandResult::Err("No path specified");
                 if (!session.saveDocument(path)) return CommandResult::Err("Save failed");
                 return CommandResult::Ok("Saved to " + path);
             }});

    reg.add({"scene.open", "Open a scene file",
             "scene.open [path]",
             {{"path", "string", "Scene file path", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 String path = args.named.value("path", args.positional.empty() ? "" : args.positional[0]);
                 if (path.empty()) return CommandResult::Err("No path specified");
                 if (!session.openDocument(path)) return CommandResult::Err("Failed to open " + path);
                 return CommandResult::Ok("Opened " + path);
             }});

    reg.add({"asset.save_all", "Save all modified assets",
             "asset.save_all",
             {},
             true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                  if (!session.execute(AssetSaveAllCommand{})) {
                       return CommandResult::Err("Asset save request failed");
                   }
                 return CommandResult::Ok("Save requested");
             }});

    reg.add({"asset.import", "Import an asset into the project",
             "asset.import [path]",
             {{"path", "string", "Asset file path", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String path = args.named.value(
                     "path", args.positional.empty() ? String() : args.positional[0]);
                 if (path.empty()) {
                     return CommandResult::Err("No asset path specified");
                 }
                  if (!session.execute(AssetImportCommand{String(path)})) {
                      return CommandResult::Err("Asset import request failed");
                  }
                 return CommandResult::Ok("Import requested: " + path);
             }});

    reg.add({"asset.reimport", "Reimport an existing project asset",
             "asset.reimport <path>",
             {{"path", "string", "Asset file path", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String path = args.named.value(
                     "path", args.positional.empty() ? String() : args.positional[0]);
                 if (path.empty()) {
                     return CommandResult::Err("No asset path specified");
                 }
                  if (!session.execute(AssetReimportCommand{String(path)})) {
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
                 Json result = Json::array();
                 for (const EditorEntity& entity : session.documentModel().entities()) {
                     Json e;
                     e["name"] = entity.name;
                     e["uuid"] = ToString(entity.uuid);
                     result.push_back(e);
                 }
                 return CommandResult::Ok(ToString(result.size()) + " GameObjects", result);
             }});

    reg.add({"query.components", "List components on a GameObject",
             "query.components <uuid>",
             {{"uuid", "uuid", "GameObject UUID", true}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 UInt64 uuid;
                 if (!args.positional.empty()) {
                     uuid = ParseUuid(args.positional[0]);
                 } else {
                     uuid = primarySelection(session);
                 }
                 if (uuid == 0) return CommandResult::Err("No GameObject specified");
                 const EditorEntity* entity = session.documentModel().findEntity(uuid);
                 if (!entity) return CommandResult::Err("GameObject not found");
                 Json result = Json::array();
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
                 const String filter = args.named.value(
                     "filter", args.positional.empty() ? String() : args.positional[0]);
                 DynamicArray<AssetBrowserEntry> assets;
                 if (!session.listAssets(assets)) {
                     return CommandResult::Err("Asset database is unavailable");
                 }
                 Json result = Json::array();
                 for (const auto& asset : assets) {
                     if (!filter.empty()) {
                         const String haystack = asset.name + " " + asset.path + " " +
                             asset.type + " " + asset.extension + " " +
                             ToString(static_cast<UInt64>(asset.uuid));
                         if (haystack.find(filter) == String::npos) {
                             continue;
                         }
                     }
                     Json item;
                     item["uuid"] = ToString(static_cast<UInt64>(asset.uuid));
                     item["name"] = asset.name;
                     item["path"] = asset.path;
                     item["type"] = asset.type;
                     item["extension"] = asset.extension;
                     item["dirty"] = asset.dirty;
                     item["dependencies"] = asset.dependencies;
                     result.push_back(std::move(item));
                 }
                 return CommandResult::Ok(ToString(result.size()) + " assets", result);
             }});

    reg.add({"query.perf.frame_stats", "Report recent frame render telemetry: timings, draw calls, dispatches, barriers",
             "query.perf.frame_stats",
             {},
             false,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 Json out;
                 if (!session.queryFrameStats(out)) {
                     return CommandResult::Err("Frame telemetry unavailable (runtime not booted)");
                 }
                 return CommandResult::Ok("", std::move(out));
             }});

    reg.add({"query.perf.passes", "Report the last frame's render graph: passes, topology and culling",
             "query.perf.passes",
             {},
             false,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 Json out;
                 if (!session.queryRenderGraphSnapshot(out)) {
                     return CommandResult::Err("Render graph snapshot unavailable (requires a debug build with a booted runtime)");
                 }
                 return CommandResult::Ok("", std::move(out));
             }});

    reg.add({"query.render.settings", "Report current render settings, viewport size and resolved features",
             "query.render.settings",
             {},
             false,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 Json out;
                 if (!session.queryRenderSettings(out)) {
                     return CommandResult::Err("Render settings unavailable (runtime not booted)");
                 }
                 return CommandResult::Ok("", std::move(out));
             }});

    reg.add({"render.set", "Set a runtime render setting: aa_mode (0 None, 1 MSAA, 2 FXAA, 3 TAA) or msaa_sample_count (1/2/4/8)",
             "render.set key=<aa_mode|msaa_sample_count> value=<int>",
             {{"key", "string", "Setting key", true,
               {"aa_mode", "msaa_sample_count"}},
              {"value", "integer", "Setting value", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String key = args.named.value("key", String());
                 if (key.empty()) {
                     return CommandResult::Err("No setting key specified");
                 }
                 const auto vit = args.named.find("value");
                 if (vit == args.named.end() || !vit->is_number_integer()) {
                     return CommandResult::Err("No setting value specified");
                 }
                 Json overlay;
                 overlay[key] = vit->get<int>();
                 if (!session.applyRenderSettingsOverlay(overlay)) {
                     return CommandResult::Err("Setting rejected (runtime not booted or value out of range)");
                 }
                 return CommandResult::Ok("Render setting " + key + " updated");
             }});

    reg.add({"benchmark.run", "Step the runtime for N frames collecting render telemetry, then report a summary (blocking)",
             "benchmark.run [frames]",
             {{"frames", "integer", "Frame count (16-3600, default 240)", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 int frames = 240;
                 const auto fit = args.named.find("frames");
                 if (fit != args.named.end() && fit->is_number_integer()) {
                     frames = fit->get<int>();
                 } else if (!args.positional.empty()) {
                     frames = std::atoi(args.positional[0]);
                 }
                 Json out;
                 if (!session.runBenchmark(frames, out)) {
                     return CommandResult::Err("Benchmark unavailable (runtime not booted)");
                 }
                 return CommandResult::Ok("", std::move(out));
             }});

    reg.add({"benchmark.compare", "Compare the summaries of the last two benchmark.run invocations",
             "benchmark.compare",
             {},
             false,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 Json out;
                 if (!session.queryBenchmarkComparison(out)) {
                     return CommandResult::Err("No benchmark pair to compare (run benchmark.run twice)");
                 }
                 return CommandResult::Ok("", std::move(out));
             }});

    reg.add({"workflow.list", "Report recorded command executions with inputs, outputs and results",
             "workflow.list [limit] [mutating_only] [author]",
             {{"limit", "integer", "Max records from the newest end, 0 = all (default 128)", false},
              {"mutating_only", "boolean", "Only mutating commands (default true)", false},
              {"author", "string", "Author filter", false, {"all", "user", "ai", "workflow"}}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const int limit = args.named.value("limit", 128);
                 const bool mutatingOnly = args.named.value("mutating_only", true);
                 const String author = args.named.value("author", String("all"));
                 return CommandResult::Ok("", session.executionRecorder().toJSON(
                     static_cast<Size_t>(limit < 0 ? 0 : limit), mutatingOnly, author));
             }});

    reg.add({"workflow.graph", "Report the execution workflow graph: nodes, artifact dependency edges and sessions",
             "workflow.graph [limit] [mutating_only] [author]",
             {{"limit", "integer", "Max records from the newest end, 0 = all (default 256)", false},
              {"mutating_only", "boolean", "Only mutating commands (default true)", false},
              {"author", "string", "Author filter", false, {"all", "user", "ai", "workflow"}}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const int limit = args.named.value("limit", 256);
                 const bool mutatingOnly = args.named.value("mutating_only", true);
                 const String author = args.named.value("author", String("all"));
                 return CommandResult::Ok("", session.executionRecorder().toGraphJSON(
                     static_cast<Size_t>(limit < 0 ? 0 : limit), mutatingOnly, author));
             }});

    reg.add({"workflow.clear", "Clear the execution recorder history",
             "workflow.clear",
             {},
             true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.executionRecorder().clear();
                 return CommandResult::Ok("Execution history cleared");
             }});

    reg.add({"workflow.capture", "Capture recent mutating command executions as a named reusable workflow template",
             "workflow.capture name=<string> [count=<int>] [author=<all|user|ai|workflow>]",
             {{"name", "string", "Template name", true},
              {"count", "integer", "Max recent records to capture (default 50)", false},
              {"author", "string", "Author filter for captured records", false,
               {"all", "user", "ai", "workflow"}}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String name = args.named.value("name", String());
                 if (name.empty()) {
                     return CommandResult::Err("No template name specified");
                 }
                 const int count = std::clamp(args.named.value("count", 50), 1, 1024);
                 const String author = args.named.value("author", String("all"));
                 const Json report =
                     session.executionRecorder().toJSON(static_cast<Size_t>(count), true, author);
                 const DynamicArray<WorkflowStep> steps = CaptureWorkflowSteps(
                     report.contains("records") ? report["records"] : Json());
                 if (steps.empty()) {
                     return CommandResult::Err("No replayable commands found in the capture window");
                 }
                 session.workflows().setStoragePath(WorkflowStoragePath(session));
                 const Size_t stepCount = steps.size();
                 session.workflows().store(name, std::move(steps));
                 return CommandResult::Ok("Captured workflow '" + name + "'",
                                          Json{{"name", name}, {"steps", stepCount}});
             }});

    reg.add({"workflow.run", "Replay a captured workflow template with entity UUID rebinding",
             "workflow.run name=<string>",
             {{"name", "string", "Template name", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String name = args.named.value("name", String());
                 session.workflows().setStoragePath(WorkflowStoragePath(session));
                 const DynamicArray<WorkflowStep>* steps = session.workflows().find(name);
                 if (!steps || steps->empty()) {
                     return CommandResult::Err("Unknown workflow template: " + name);
                 }
                 return CommandResult::Ok("", RunWorkflow(session, name, *steps));
             }});

    reg.add({"workflow.templates", "List captured workflow templates",
             "workflow.templates",
             {},
             false,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.workflows().setStoragePath(WorkflowStoragePath(session));
                 Json items = Json::array();
                 for (const String& name : session.workflows().names()) {
                     const auto* steps = session.workflows().find(name);
                     Json item;
                     item["name"] = name;
                     item["steps"] = steps ? steps->size() : 0;
                     items.push_back(std::move(item));
                 }
                 return CommandResult::Ok("", std::move(items));
             }});

    reg.add({"workflow.export", "Export a workflow template as JSON",
             "workflow.export name=<string>",
             {{"name", "string", "Template name", true}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String name = args.named.value("name", String());
                 session.workflows().setStoragePath(WorkflowStoragePath(session));
                 const auto* steps = session.workflows().find(name);
                 if (!steps) {
                     return CommandResult::Err("Unknown workflow template: " + name);
                 }
                 Json data;
                 data["name"] = name;
                 data["steps"] = Json::array();
                 for (const WorkflowStep& step : *steps) {
                     Json item;
                     item["command"] = step.command;
                     item["args"] = step.args;
                     item["produces"] = step.produces;
                     data["steps"].push_back(std::move(item));
                 }
                 return CommandResult::Ok("", std::move(data));
             }});

    reg.add({"workflow.import", "Import a workflow template from JSON",
             "workflow.import data=<object>",
             {{"data", "object", "Template JSON: {name, steps: [{command, args, produces}]}", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const Json data = args.named.value("data", Json::object());
                 if (!data.is_object() || !data.contains("name") || !data["name"].is_string()) {
                     return CommandResult::Err("Template data must be an object with a string 'name'");
                 }
                 const String name = data["name"].get<String>();
                 const auto stepsIt = data.find("steps");
                 if (stepsIt == data.end() || !stepsIt->is_array()) {
                     return CommandResult::Err("Template data must contain a 'steps' array");
                 }
                 DynamicArray<WorkflowStep> steps;
                 for (const auto& item : *stepsIt) {
                     if (!item.is_object() || !item.contains("command") || !item["command"].is_string() ||
                         !item.contains("args") || !item["args"].is_object()) {
                         return CommandResult::Err("Each step must have a string 'command' and an object 'args'");
                     }
                     WorkflowStep step;
                     step.command = item["command"].get<String>();
                     step.args = item["args"];
                     if (item.contains("produces") && item["produces"].is_array()) {
                         for (const auto& uuid : item["produces"]) {
                             if (uuid.is_string()) {
                                 step.produces.push_back(uuid.get<String>());
                             }
                         }
                     }
                     steps.push_back(std::move(step));
                 }
                 if (steps.empty()) {
                     return CommandResult::Err("Template has no valid steps");
                 }
                 session.workflows().setStoragePath(WorkflowStoragePath(session));
                 session.workflows().store(name, std::move(steps));
                 return CommandResult::Ok("Imported workflow '" + name + "'");
             }});

    reg.add({"mcp.start", "Start the built-in MCP server (Streamable HTTP, loopback only)",
             "mcp.start [port]",
             {{"port", "integer", "TCP port (0 picks a free port, default 7800)", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 UInt16 port = kDefaultMcpPort;
                 const auto nit = args.named.find("port");
                 if (nit != args.named.end() && nit->is_number_integer()) {
                     const long long requested = nit->get<long long>();
                     if (requested < 0 || requested > 65535) {
                         return CommandResult::Err("Port out of range (0-65535)");
                     }
                     port = static_cast<UInt16>(requested);
                 } else if (!args.positional.empty()) {
                     char* end = nullptr;
                     const unsigned long requested =
                         std::strtoul(args.positional[0].c_str(), &end, 10);
                     if (!end || *end != '\0' || requested > 65535) {
                         return CommandResult::Err("Invalid port");
                     }
                     port = static_cast<UInt16>(requested);
                 }
                 if (!session.mcpServer().start(session, port)) {
                     return CommandResult::Err("Failed to start MCP server (port in use?)");
                 }
                 return CommandResult::Ok("MCP server listening on 127.0.0.1:" +
                                              ToString(session.mcpServer().port()),
                                          Json{{"port", session.mcpServer().port()}});
             }});

    reg.add({"mcp.stop", "Stop the built-in MCP server",
             "mcp.stop", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 if (!session.mcpServer().isRunning()) {
                     return CommandResult::Err("MCP server is not running");
                 }
                 session.mcpServer().stop();
                 return CommandResult::Ok("MCP server stopped");
             }});

    reg.add({"mcp.status", "Report MCP server state",
             "mcp.status", {}, false,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 Json out;
                 out["running"] = session.mcpServer().isRunning();
                 out["port"] = session.mcpServer().port();
                 out["pending"] = session.mcpServer().pendingCount();
                 return CommandResult::Ok("", std::move(out));
             }});

    reg.add({"ai.begin_session", "Begin an AI transaction session with an edit-history checkpoint",
             "ai.begin_session [goal]",
             {{"goal", "string", "Session goal", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 String goal = args.named.value(
                     "goal", args.positional.empty() ? String() : args.positional[0]);
                 const String id = session.aiBridge().beginSession(goal);
                 if (id.empty()) {
                     return CommandResult::Err("No editor session attached");
                 }
                 return CommandResult::Ok("AI session " + id,
                                          Json{{"session", id}, {"goal", goal}});
             }});

    reg.add({"ai.end_session", "End the AI transaction session (keep=false reverts to the checkpoint)",
             "ai.end_session [keep]",
             {{"keep", "boolean", "Keep changes (default true)", false}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 if (!session.aiBridge().inSession()) {
                     return CommandResult::Err("No active AI session");
                 }
                 bool keep = true;
                 const auto kit = args.named.find("keep");
                 if (kit != args.named.end() && kit->is_boolean()) {
                     keep = kit->get<bool>();
                 } else if (!args.positional.empty()) {
                     keep = args.positional[0] == "true" || args.positional[0] == "1";
                 }
                 session.aiBridge().endSession(keep);
                 return CommandResult::Ok(keep ? "AI session committed" : "AI session reverted",
                                          Json{{"kept", keep}});
             }});

    reg.add({"query.logs", "Report backend log entries",
             "query.logs [limit] [level]",
             {{"limit", "integer", "Max entries from the newest end (default 128)", false},
              {"level", "string", "Minimum level", false,
               {"trace", "debug", "info", "warning", "error", "critical"}}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 int limit = args.named.value("limit", 128);
                 const String minLevel = args.named.value("level", String("trace"));
                 static const Pair<const char*, int> kLevels[] = {
                     {"trace", 0}, {"debug", 1}, {"info", 2},
                     {"warning", 3}, {"error", 4}, {"critical", 5}};
                 int minRank = 0;
                 for (const auto& [name, rank] : kLevels) {
                     if (minLevel == name) minRank = rank;
                 }
                 DynamicArray<BackendLogEntry> entries;
                 if (!session.listLogs(entries)) {
                     return CommandResult::Err("Logs unavailable");
                 }
                 if (limit < 0) limit = 0;
                 const int total = static_cast<int>(entries.size());
                 const int skip = total > limit ? total - limit : 0;
                 Json out = Json::array();
                 static const char* kLevelNames[] = {"trace", "debug", "info",
                                                     "warning", "error", "critical"};
                 for (int i = skip; i < total; ++i) {
                     const int rank = static_cast<int>(entries[i].level);
                     if (rank < minRank) continue;
                     Json item;
                     item["sequence"] = entries[i].sequence;
                     item["level"] = kLevelNames[rank];
        item["source"] = entries[i].source;
        item["message"] = entries[i].message;
                     item["repeat"] = entries[i].repeatCount;
                     out.push_back(std::move(item));
                 }
                 return CommandResult::Ok(ToString(out.size()) + " log entries",
                                          std::move(out));
             }});

    reg.add({"query.history", "Report the undo/redo stack state",
             "query.history [limit]",
             {{"limit", "integer", "Max stack labels (default 16)", false}},
             false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 int limit = std::clamp(args.named.value("limit", 16), 0, 256);
                 Json out;
                 out["can_undo"] = session.history().canUndo();
                 out["can_redo"] = session.history().canRedo();
                 out["undo_count"] = session.history().undoCount();
                 out["redo_count"] = session.history().redoCount();
                 out["undo_label"] = session.history().undoLabel();
                 out["redo_label"] = session.history().redoLabel();
                 Json undoStack = Json::array();
                 const auto& undo = session.history().undoStack();
                 int count = 0;
                 for (auto it = undo.rbegin(); it != undo.rend() && count < limit; ++it, ++count) {
                     undoStack.push_back((*it)->label());
                 }
                 out["undo_stack"] = std::move(undoStack);
                 Json redoStack = Json::array();
                 const auto& redo = session.history().redoStack();
                 count = 0;
                 for (auto it = redo.rbegin(); it != redo.rend() && count < limit; ++it, ++count) {
                     redoStack.push_back((*it)->label());
                 }
                 out["redo_stack"] = std::move(redoStack);
                 return CommandResult::Ok("", std::move(out));
             }});

    reg.add({"mcp.deny", "Deny AI/MCP execution of commands with this prefix",
             "mcp.deny <prefix>",
             {{"prefix", "string", "Command name prefix (e.g. entity.delete)", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String prefix = args.named.value(
                     "prefix", args.positional.empty() ? String() : args.positional[0]);
                 if (prefix.empty()) {
                     return CommandResult::Err("No prefix specified");
                 }
                 auto& bridge = session.aiBridge();
                 auto list = bridge.denylist();
                 if (std::find(list.begin(), list.end(), prefix) == list.end()) {
                     list.push_back(prefix);
                     bridge.setDenylist(std::move(list));
                 }
                 return CommandResult::Ok("Denied for AI/MCP: " + prefix,
                                          Json{{"denylist", bridge.denylist()}});
             }});

    reg.add({"mcp.allow", "Remove a prefix from the AI/MCP denylist",
             "mcp.allow <prefix>",
             {{"prefix", "string", "Command name prefix", true}},
             true,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 const String prefix = args.named.value(
                     "prefix", args.positional.empty() ? String() : args.positional[0]);
                 if (prefix.empty()) {
                     return CommandResult::Err("No prefix specified");
                 }
                 auto& bridge = session.aiBridge();
                 auto list = bridge.denylist();
                 list.erase(std::remove(list.begin(), list.end(), prefix), list.end());
                 bridge.setDenylist(std::move(list));
                 return CommandResult::Ok("Allowed for AI/MCP: " + prefix,
                                          Json{{"denylist", bridge.denylist()}});
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
                 session.execute(PlayActionCommand{PlayAction::Play});
                 return CommandResult::Ok();
             }});

    reg.add({"playmode.pause", "Pause or resume play mode",
             "playmode.pause", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.execute(PlayActionCommand{PlayAction::Pause});
                 return CommandResult::Ok();
             }});

    reg.add({"playmode.stop", "Stop play mode",
             "playmode.stop", {}, true,
             [](EditorSession& session, const CommandArgs&) -> CommandResult {
                 session.execute(PlayActionCommand{PlayAction::Stop});
                 return CommandResult::Ok();
             }});

    reg.add({"gizmo.setmode", "Set gizmo tool mode",
             "gizmo.setmode mode=<Translate|Rotate|Scale|None>",
             {{"mode", "string", "Gizmo mode", true,
               {"Translate", "Move", "Rotate", "Scale", "None"}}}, false,
             [](EditorSession& session, const CommandArgs& args) -> CommandResult {
                 String mode = args.named.value("mode", args.positional.empty() ? "" : args.positional[0]);
                 GizmoMode backendMode = GizmoMode::None;
                 if (mode == "Translate" || mode == "Move") backendMode = GizmoMode::Translate;
                 else if (mode == "Rotate") backendMode = GizmoMode::Rotate;
                 else if (mode == "Scale") backendMode = GizmoMode::Scale;
                 session.execute(GizmoModeCommand{backendMode});
                 return CommandResult::Ok("Gizmo: " + mode);
             }});

    reg.add({"layout.menu", "Show layout context menu",
             "layout.menu", {}, false,
             [](EditorSession&, const CommandArgs&) -> CommandResult {
                 return CommandResult::Ok();
             }});
}

} // namespace cakery
