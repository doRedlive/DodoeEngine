// do@Redlive

#include "EditorSession.h"

namespace cakery {

EditorSession::EditorSession(std::unique_ptr<IEditorBackend> backend)
    : m_backend(std::move(backend))
{
}

EditorSession::~EditorSession()
{
    shutdown();
}

bool EditorSession::openProject(ProjectDescriptor project)
{
    if (!m_backend || project.rootPath.empty()) {
        m_state = EditorSessionState::Failed;
        return false;
    }

    m_state = EditorSessionState::OpeningProject;
    m_project = std::move(project);
    if (!m_backend->openProject(m_project)) {
        m_state = EditorSessionState::Failed;
        return false;
    }

    const BackendCapabilities backendCapabilities = m_backend->capabilities();
    m_state = backendCapabilities.documentRead || backendCapabilities.documentWrite ||
              backendCapabilities.scenePreview || backendCapabilities.simulation
        ? EditorSessionState::Ready
        : EditorSessionState::Degraded;
    return true;
}

void EditorSession::submitViewportMetrics(ViewportMetrics metrics)
{
    if (!m_backend || m_state == EditorSessionState::Closing || m_state == EditorSessionState::Closed) {
        return;
    }
    if (metrics.logicalWidth < 1 || metrics.logicalHeight < 1 ||
        metrics.pixelWidth < 1 || metrics.pixelHeight < 1 ||
        metrics.sequence <= m_lastViewportSequence) {
        return;
    }

    m_lastViewportSequence = metrics.sequence;
    m_backend->requestSceneSurfaceResize(metrics);
}

void EditorSession::tick()
{
    if (m_backend && (m_state == EditorSessionState::Ready || m_state == EditorSessionState::Degraded)) {
        m_backend->tickAtSafePoint();
    }
}

void EditorSession::shutdown()
{
    if (!m_backend || m_state == EditorSessionState::Closed) {
        return;
    }

    m_state = EditorSessionState::Closing;
    m_backend->detachSceneSurface();
    m_backend->shutdown();
    m_state = EditorSessionState::Closed;
}

EditorSessionState EditorSession::state() const
{
    return m_state;
}

const ProjectDescriptor& EditorSession::project() const
{
    return m_project;
}

BackendCapabilities EditorSession::capabilities() const
{
    return m_backend ? m_backend->capabilities() : BackendCapabilities{};
}

std::string EditorSession::diagnostic() const
{
    return m_backend ? m_backend->diagnostic() : "No editor backend is available.";
}

} // namespace cakery
