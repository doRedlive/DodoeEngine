#include "dopch.h"

#include <gtest/gtest.h>

#include "core/EditorSession.h"
#include "core/console/CommandRegistry.h"
#include "core/console/ExecutionRecorder.h"
#include "core/console/WorkflowTemplate.h"

#include <algorithm>
#include <memory>

namespace {

Scope<cakery::EditorSession> MakeNullSession() {
    auto session = create_scope<cakery::EditorSession>();
    cakery::RegisterBuiltinCommands(session->commands());
    return session;
}

TEST(CommandRegistry, UnknownCommandFails) {
    auto session = MakeNullSession();
    const CommandResult result =
        session->commands().executeStructured(*session, "definitely_not_a_command",
                                                  Json::object());
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Unknown command"), String::npos);
}

TEST(CommandRegistry, ValidationRejectsMissingRequiredParameter) {
    auto session = MakeNullSession();
    const CommandResult result = session->commands().executeStructured(
        *session, "entity.create", Json::object());
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Missing required parameter: name"), String::npos);
}

TEST(CommandRegistry, ValidationRejectsWrongType) {
    auto session = MakeNullSession();
    const CommandResult result =
        session->commands().executeStructured(*session, "entity.create",
                                                  Json{{"name", 123}});
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Invalid type for parameter 'name'"), String::npos);
}

TEST(CommandRegistry, ValidationRejectsEnumMismatch) {
    auto session = MakeNullSession();
    const CommandResult result = session->commands().executeStructured(
        *session, "gizmo.setmode", Json{{"mode", "Sideways"}});
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("must be one of"), String::npos);
}

TEST(CommandRegistry, ValidArgsReachHandlerWithoutDocument) {
    auto session = MakeNullSession();
    const CommandResult result = session->commands().executeStructured(
        *session, "entity.create", Json{{"name", "Test"}});
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("Failed to create"), String::npos);

    const Json report = session->executionRecorder().toJSON(8, true, "user");
    ASSERT_TRUE(report.contains("records"));
    ASSERT_TRUE(report["records"].is_array());
    ASSERT_EQ(report["records"].size(), 1u);
    EXPECT_EQ(report["records"][0]["command"], "entity.create");
    EXPECT_EQ(report["records"][0]["author"], "user");
    EXPECT_FALSE(report["records"][0]["ok"]);
}

TEST(CommandRegistry, LineExecuteRecordsAsUserAuthor) {
    auto session = MakeNullSession();
    const CommandResult result = session->commands().execute(*session, "query.entities");
    EXPECT_FALSE(result.ok);
    EXPECT_NE(result.message.find("No active scene"), String::npos);
    const Json report = session->executionRecorder().toJSON(8, false, "user");
    ASSERT_EQ(report["records"].size(), 1u);
    EXPECT_EQ(report["records"][0]["command"], "query.entities");
}

TEST(CommandRegistry, ToolSchemaShape) {
    auto session = MakeNullSession();
    const Json tools = session->commands().toolSchema();
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
                      Json::array({"name"}));
        }
    }
    EXPECT_TRUE(hasEntityCreate);
}

TEST(CommandRegistry, AiContextRecordsAiAuthor) {
    auto session = MakeNullSession();
    CommandContext context;
    context.author = "ai";
    const CommandResult result = session->commands().executeStructured(
        *session, "query.entities", Json::object(), context);
    EXPECT_FALSE(result.ok);
    const Json report = session->executionRecorder().toJSON(8, false, "ai");
    ASSERT_EQ(report["records"].size(), 1u);
    EXPECT_EQ(report["records"][0]["author"], "ai");
    const Json userReport = session->executionRecorder().toJSON(8, false, "user");
    EXPECT_EQ(userReport["records"].size(), 0u);
}

TEST(ExecutionRecorder, GraphJSONContainsNodesAndEdges) {
    ExecutionRecorder recorder;
    ExecutionRecord first;
    first.command = "entity.create";
    first.args = Json{{"name", "A"}};
    first.data = Json{{"uuid", "100"}};
    first.ok = true;
    first.author = "ai";
    first.mutating = true;
    recorder.record(std::move(first));
    ExecutionRecord second;
    second.command = "component.add";
    second.args = Json{{"uuid", "100"}, {"type", "Sprite"}};
    second.ok = true;
    second.author = "ai";
    second.mutating = true;
    recorder.record(std::move(second));

    const Json graph = recorder.toGraphJSON(16, true, "ai");
    ASSERT_TRUE(graph.contains("nodes"));
    EXPECT_GE(graph["nodes"].size(), 2u);
}

TEST(WorkflowTemplate, CaptureFiltersNonReplayablePrefixes) {
    Json records = Json::array();
    const auto addRecord = [&records](const String& command, const String& uuid) {
        Json record;
        record["command"] = command;
        record["args"] = Json{{"name", "A"}};
        record["ok"] = true;
        record["data"] = Json{{"uuid", uuid}};
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

    const DynamicArray<WorkflowStep> steps = CaptureWorkflowSteps(records);
    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0].command, "entity.create");
    ASSERT_EQ(steps[0].produces.size(), 1u);
    EXPECT_EQ(steps[0].produces[0], "100");
    EXPECT_EQ(steps[1].command, "component.add");
}

TEST(WorkflowTemplate, LibraryRoundTripInMemory) {
    WorkflowLibrary library;
    DynamicArray<WorkflowStep> steps;
    WorkflowStep step;
    step.command = "entity.create";
    step.args = Json{{"name", "A"}};
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

    const DynamicArray<String> names = library.names();
    EXPECT_TRUE(std::find(names.begin(), names.end(), "roundtrip") != names.end());
}

} // namespace
