#include <gtest/gtest.h>

#include "core/EditorSession.h"
#include "core/console/CommandRegistry.h"
#include "core/console/ExecutionRecorder.h"
#include "core/console/WorkflowTemplate.h"

#include <algorithm>
#include <memory>

namespace {

std::unique_ptr<cakery::EditorSession> MakeNullSession() {
    std::unique_ptr<cakery::IEditorBackend> backend;
    return std::make_unique<cakery::EditorSession>(std::move(backend));
}

struct ScopedRecorderClear {
    ~ScopedRecorderClear() { ExecutionRecorder::self().clear(); }
};

TEST(CommandRegistry, UnknownCommandFails) {
    auto session = MakeNullSession();
    ScopedRecorderClear cleanup;
    const CommandResult result =
        CommandRegistry::self().executeStructured(session, "definitely_not_a_command",
                                                  nlohmann::json::object());
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Unknown command"), std::string::npos);
}

TEST(CommandRegistry, ValidationRejectsMissingRequiredParameter) {
    auto session = MakeNullSession();
    ScopedRecorderClear cleanup;
    const CommandResult result = CommandRegistry::self().executeStructured(
        session, "entity.create", nlohmann::json::object());
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Missing required parameter: name"), std::string::npos);
}

TEST(CommandRegistry, ValidationRejectsWrongType) {
    auto session = MakeNullSession();
    ScopedRecorderClear cleanup;
    const CommandResult result =
        CommandRegistry::self().executeStructured(session, "entity.create",
                                                  nlohmann::json{{"name", 123}});
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Invalid type for parameter 'name'"), std::string::npos);
}

TEST(CommandRegistry, ValidationRejectsEnumMismatch) {
    auto session = MakeNullSession();
    ScopedRecorderClear cleanup;
    const CommandResult result = CommandRegistry::self().executeStructured(
        session, "gizmo.setmode", nlohmann::json{{"mode", "Sideways"}});
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("must be one of"), std::string::npos);
}

TEST(CommandRegistry, ValidArgsReachHandlerWithoutDocument) {
    auto session = MakeNullSession();
    ScopedRecorderClear cleanup;
    const CommandResult result = CommandRegistry::self().executeStructured(
        session, "entity.create", nlohmann::json{{"name", "Test"}});
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Failed to create"), std::string::npos);

    const nlohmann::json report = ExecutionRecorder::self().toJSON(8, true, "user");
    ASSERT_TRUE(report.contains("records"));
    ASSERT_TRUE(report["records"].is_array());
    ASSERT_EQ(report["records"].size(), 1u);
    EXPECT_EQ(report["records"][0]["command"], "entity.create");
    EXPECT_EQ(report["records"][0]["author"], "user");
    EXPECT_FALSE(report["records"][0]["ok"]);
}

TEST(CommandRegistry, LineExecuteRecordsAsUserAuthor) {
    auto session = MakeNullSession();
    ScopedRecorderClear cleanup;
    const CommandResult result = CommandRegistry::self().execute(session, "query.entities");
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("No active scene"), std::string::npos);
    const nlohmann::json report = ExecutionRecorder::self().toJSON(8, false, "user");
    ASSERT_EQ(report["records"].size(), 1u);
    EXPECT_EQ(report["records"][0]["command"], "query.entities");
}

TEST(CommandRegistry, ToolSchemaShape) {
    const nlohmann::json tools = CommandRegistry::self().toolSchema();
    ASSERT_TRUE(tools.is_array());
    EXPECT_FALSE(tools.empty());
    for (const auto& tool : tools) {
        ASSERT_TRUE(tool.contains("name") && tool["name"].is_string());
        ASSERT_TRUE(tool.contains("description"));
        ASSERT_TRUE(tool.contains("parameters"));
        EXPECT_EQ(tool["parameters"]["type"], "object");
    }
    bool hasEntityCreate = false;
    for (const auto& tool : tools) {
        if (tool["name"] == "entity.create") {
            hasEntityCreate = true;
            EXPECT_EQ(tool["parameters"]["required"],
                      nlohmann::json::array({"name"}));
        }
    }
    EXPECT_TRUE(hasEntityCreate);
}

TEST(CommandRegistry, AiContextRecordsAiAuthor) {
    auto session = MakeNullSession();
    ScopedRecorderClear cleanup;
    CommandContext context;
    context.author = "ai";
    const CommandResult result = CommandRegistry::self().executeStructured(
        session, "query.entities", nlohmann::json::object(), context);
    EXPECT_FALSE(result.ok);
    const nlohmann::json report = ExecutionRecorder::self().toJSON(8, false, "ai");
    ASSERT_EQ(report["records"].size(), 1u);
    EXPECT_EQ(report["records"][0]["author"], "ai");
    const nlohmann::json userReport = ExecutionRecorder::self().toJSON(8, false, "user");
    EXPECT_EQ(userReport["records"].size(), 0u);
}

TEST(ExecutionRecorder, GraphJSONContainsNodesAndEdges) {
    ExecutionRecorder::self().clear();
    ScopedRecorderClear cleanup;
    ExecutionRecord first;
    first.command = "entity.create";
    first.args = nlohmann::json{{"name", "A"}};
    first.data = nlohmann::json{{"uuid", "100"}};
    first.ok = true;
    first.author = "ai";
    first.mutating = true;
    ExecutionRecorder::self().record(std::move(first));
    ExecutionRecord second;
    second.command = "component.add";
    second.args = nlohmann::json{{"uuid", "100"}, {"type", "Sprite"}};
    second.ok = true;
    second.author = "ai";
    second.mutating = true;
    ExecutionRecorder::self().record(std::move(second));

    const nlohmann::json graph = ExecutionRecorder::self().toGraphJSON(16, true, "ai");
    ASSERT_TRUE(graph.contains("nodes"));
    EXPECT_GE(graph["nodes"].size(), 2u);
}

TEST(WorkflowTemplate, CaptureFiltersNonReplayablePrefixes) {
    nlohmann::json records = nlohmann::json::array();
    const auto addRecord = [&records](const std::string& command, const std::string& uuid) {
        nlohmann::json record;
        record["command"] = command;
        record["args"] = nlohmann::json{{"name", "A"}};
        record["ok"] = true;
        record["data"] = nlohmann::json{{"uuid", uuid}};
        records.push_back(std::move(record));
    };
    addRecord("entity.create", "100");
    addRecord("query.entities", "");
    addRecord("workflow.list", "");
    addRecord("benchmark.run", "");
    addRecord("session.begin", "");
    addRecord("mcp.stop", "");
    addRecord("ai.begin_session", "");
    addRecord("component.add", "");

    const std::vector<WorkflowStep> steps = CaptureWorkflowSteps(records);
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].command, "entity.create");
    ASSERT_EQ(steps[0].produces.size(), 1u);
    EXPECT_EQ(steps[0].produces[0], "100");
    EXPECT_EQ(steps[1].command, "component.add");
}

TEST(WorkflowTemplate, LibraryRoundTripInMemory) {
    auto& library = WorkflowLibrary::self();
    std::vector<WorkflowStep> steps;
    WorkflowStep step;
    step.command = "entity.create";
    step.args = nlohmann::json{{"name", "A"}};
    step.produces.push_back("100");
    steps.push_back(std::move(step));

    library.setStoragePath({});
    library.store("roundtrip", std::move(steps));
    EXPECT_TRUE(library.contains("roundtrip"));
    const auto* stored = library.find("roundtrip");
    ASSERT_NE(stored, nullptr);
    ASSERT_EQ(stored->size(), 1u);
    EXPECT_EQ((*stored)[0].command, "entity.create");
    EXPECT_EQ((*stored)[0].args["name"], "A");

    const std::vector<std::string> names = library.names();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "roundtrip") != names.end());
}

} // namespace
