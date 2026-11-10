// do@Redlive
#pragma once
#include "dopch.h"


#include <string>
#include <unordered_map>
#include <vector>

namespace cakery {

class EditorSession;

struct WorkflowStep {
    String command;
    Json args;
    DynamicArray<String> produces;
};

class WorkflowLibrary {
public:
    WorkflowLibrary();

    WorkflowLibrary(const WorkflowLibrary&) = delete;
    WorkflowLibrary& operator=(const WorkflowLibrary&) = delete;

    void setStoragePath(const String& path);
    DynamicArray<String> names() const;
    bool contains(const String& name) const;
    const DynamicArray<WorkflowStep>* find(const String& name) const;
    void store(const String& name, DynamicArray<WorkflowStep> steps);
    Size_t size() const { return m_templates.size(); }

private:
    void loadFromDisk();
    void saveToDisk();

    String m_storagePath;
    bool m_loaded = false;
    UnorderedMap<String, DynamicArray<WorkflowStep>> m_templates;
};

String WorkflowStoragePath(EditorSession& session);
DynamicArray<WorkflowStep> CaptureWorkflowSteps(const Json& records);
Json RunWorkflow(EditorSession& session, const String& name,
                           const DynamicArray<WorkflowStep>& steps);

} // namespace cakery
