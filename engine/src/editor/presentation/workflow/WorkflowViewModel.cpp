// do@Redlive

#include "dopch.h"

#include "WorkflowViewModel.h"

#include "core/console/CommandRegistry.h"
#include "core/console/ExecutionRecorder.h"
#include "core/console/WorkflowTemplate.h"

namespace cakery {

namespace {

constexpr Size_t kMaxVisibleRecords = 256;
constexpr Size_t kMaxGraphNodes = 128;

} // namespace

WorkflowViewModel::WorkflowViewModel(EditorSession& session)
    : m_session(session)
{
    m_recorderSubscription = m_session.events().subscribe<ExecutionRecordedEvent>(
        [this](const ExecutionRecordedEvent&) { refresh(); });
    refresh();
    refreshTemplates();
}

void WorkflowViewModel::setQuery(bool mutatingOnly, const String& author)
{
    if (m_mutatingOnly == mutatingOnly && m_author == author) {
        return;
    }
    m_mutatingOnly = mutatingOnly;
    m_author = author;
    refresh();
}

void WorkflowViewModel::refresh()
{
    DynamicArray<WorkflowRecordRow> rows;
    const Json report =
        m_session.executionRecorder().toJSON(kMaxVisibleRecords, m_mutatingOnly, m_author);
    if (report.contains("records") && report["records"].is_array()) {
        for (const auto& record : report["records"]) {
            WorkflowRecordRow row;
            row.sequence = record.value("sequence", UInt64(0));
            row.command = record.value("command", String());
            row.author = record.value("author", String());
            row.ok = record.value("ok", false);
            row.durationMs = record.value("duration_ms", 0.0);
            row.session = record.value("session", String());
            row.raw = record.dump();
            rows.push_back(std::move(row));
        }
    }
    m_records = std::move(rows);
    m_graph = m_session.executionRecorder().toGraphJSON(kMaxGraphNodes, m_mutatingOnly, m_author);
    notifyChanged();
}

void WorkflowViewModel::clearHistory()
{
    m_session.executionRecorder().clear();
}

void WorkflowViewModel::refreshTemplates()
{
    m_session.workflows().setStoragePath(WorkflowStoragePath(m_session));
    DynamicArray<String> names;
    for (const String& name : m_session.workflows().names()) {
        names.push_back(name);
    }
    m_templates = std::move(names);
    notifyChanged();
}

bool WorkflowViewModel::captureTemplate(const String& name, String& outMessage)
{
    const Json report =
        m_session.executionRecorder().toJSON(kMaxVisibleRecords, true, m_author);
    DynamicArray<WorkflowStep> steps = CaptureWorkflowSteps(
        report.contains("records") ? report["records"] : Json());
    if (steps.empty()) {
        outMessage = "No replayable commands in the current filter window.";
        return false;
    }
    m_session.workflows().setStoragePath(WorkflowStoragePath(m_session));
    const Size_t count = steps.size();
    m_session.workflows().store(name, std::move(steps));
    refreshTemplates();
    outMessage = "Captured " + ToString(count) + " steps into '" + name + "'.\n"
        "Run it any time from the template list.";
    return true;
}

Json WorkflowViewModel::runTemplate(const String& name)
{
    m_session.workflows().setStoragePath(WorkflowStoragePath(m_session));
    const auto* steps = m_session.workflows().find(name);
    if (!steps || steps->empty()) {
        return Json();
    }
    return RunWorkflow(m_session, name, *steps);
}

String WorkflowViewModel::rerun(const Json& record, const Json* argsOverride)
{
    const String command = record.value("command", String());
    if (command.empty()) {
        return {};
    }
    CommandResult result;
    if (argsOverride) {
        result = m_session.commands().executeStructured(m_session, command, *argsOverride);
    } else {
        const Json args = record.value("args", Json());
        if (args.is_object()) {
            result = m_session.commands().executeStructured(m_session, command, args);
        } else if (args.is_string()) {
            result = m_session.commands().execute(m_session, args.get<String>());
        } else {
            return {};
        }
    }
    return result.ok ? result.message + "\n" + ToString(result.data, 2) : result.message;
}

void WorkflowViewModel::selectEntity(const String& uuid)
{
    Json args;
    args["uuid"] = uuid;
    m_session.commands().executeStructured(m_session, "entity.select", args);
}

} // namespace cakery
