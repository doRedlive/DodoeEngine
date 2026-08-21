// do@Redlive

#pragma once

#include "bridge/EditorBackend.h"

#include <memory>
#include <string>
#include <utility>

namespace cakery {

enum class EditorSessionState {
    Created,
    OpeningProject,
    Ready,
    Degraded,
    Failed,
    Closing,
    Closed
};

class EditorSession {
public:
    explicit EditorSession(std::unique_ptr<IEditorBackend> backend);
    ~EditorSession();

    EditorSession(const EditorSession&) = delete;
    EditorSession& operator=(const EditorSession&) = delete;

    bool openProject(ProjectDescriptor project);
    void submitViewportMetrics(ViewportMetrics metrics);
    void tick();
    void shutdown();

    EditorSessionState state() const;
    const ProjectDescriptor& project() const;
    BackendCapabilities capabilities() const;
    std::string diagnostic() const;

private:
    std::unique_ptr<IEditorBackend> m_backend;
    ProjectDescriptor m_project;
    EditorSessionState m_state = EditorSessionState::Created;
    std::uint64_t m_lastViewportSequence = 0;
};

} // namespace cakery
