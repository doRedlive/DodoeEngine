// do@Redlive

#pragma once

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace cakery {

class EditorSession;

struct WorkflowStep {
    std::string command;
    nlohmann::json args;
    std::vector<std::string> produces;
};

class WorkflowLibrary {
public:
    static WorkflowLibrary& self();

    WorkflowLibrary(const WorkflowLibrary&) = delete;
    WorkflowLibrary& operator=(const WorkflowLibrary&) = delete;

    void setStoragePath(const std::string& path);
    std::vector<std::string> names() const;
    bool contains(const std::string& name) const;
    const std::vector<WorkflowStep>* find(const std::string& name) const;
    void store(const std::string& name, std::vector<WorkflowStep> steps);
    std::size_t size() const { return m_templates.size(); }

private:
    WorkflowLibrary() = default;

    void loadFromDisk();
    void saveToDisk();

    std::string m_storagePath;
    bool m_loaded = false;
    std::unordered_map<std::string, std::vector<WorkflowStep>> m_templates;
};

std::string WorkflowStoragePath(EditorSession& session);
std::vector<WorkflowStep> CaptureWorkflowSteps(const nlohmann::json& records);
nlohmann::json RunWorkflow(EditorSession& session, const std::string& name,
                           const std::vector<WorkflowStep>& steps);

} // namespace cakery
