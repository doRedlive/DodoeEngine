// do@Redlive
#pragma once
#include "dopch.h"

#include "core/EditorSession.h"
#include "presentation/ViewModel.h"

namespace cakery {

struct WorkflowRecordRow {
    UInt64 sequence = 0;
    String command;
    String author;
    bool ok = false;
    double durationMs = 0.0;
    String session;
    String raw;
};

class WorkflowViewModel : public ViewModel {
public:
    explicit WorkflowViewModel(EditorSession& session);

    const DynamicArray<WorkflowRecordRow>& records() const { return m_records; }
    const Json& graph() const { return m_graph; }
    const DynamicArray<String>& templates() const { return m_templates; }

    void setQuery(bool mutatingOnly, const String& author);
    void refresh();
    void clearHistory();
    bool captureTemplate(const String& name, String& outMessage);
    Json runTemplate(const String& name);
    String rerun(const Json& record, const Json* argsOverride);
    void selectEntity(const String& uuid);

private:
    void refreshTemplates();

    EditorSession& m_session;
    bool m_mutatingOnly = true;
    String m_author = "all";
    DynamicArray<WorkflowRecordRow> m_records;
    Json m_graph;
    DynamicArray<String> m_templates;
    ScopedConnection m_recorderSubscription;
};

} // namespace cakery
