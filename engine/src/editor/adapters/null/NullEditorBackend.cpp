// do@Redlive

#include "NullEditorBackend.h"

#include <filesystem>

namespace cakery {

BackendCapabilities NullEditorBackend::capabilities() const
{
    BackendCapabilities caps;
    caps.documentRead = true;
    caps.documentWrite = true;
    return caps;
}

bool NullEditorBackend::openProject(const ProjectDescriptor& project)
{
    m_status = {BackendState::OpeningProject, "Opening Editor-Only project."};
    if (project.rootPath.empty() || !std::filesystem::is_directory(project.rootPath)) {
        m_status = {BackendState::Failed, "Project directory does not exist."};
        return false;
    }
    m_projectPath = project.rootPath;
    m_status = {BackendState::Ready, "Editor-Only project opened. Authoring is available; scene preview and simulation are unavailable."};
    return true;
}

bool NullEditorBackend::openDocument(const std::string& documentId)
{
    (void)documentId;
    return true;
}

bool NullEditorBackend::execute(const EditorCommandMessage& command)
{
    (void)command;
    return true;
}

void NullEditorBackend::setEventCallback(std::function<void(const BackendEventMessage&)>)
{
}

bool NullEditorBackend::attachSceneSurface(const SceneSurfaceDescriptor& surface)
{
    m_surfaceAttached = surface.nativeHandle != 0;
    return m_surfaceAttached;
}

void NullEditorBackend::requestSceneSurfaceResize(const ViewportMetrics& metrics)
{
    (void)metrics;
}

bool NullEditorBackend::detachSceneSurface()
{
    m_surfaceAttached = false;
    return true;
}

void NullEditorBackend::tickAtSafePoint()
{
}

void NullEditorBackend::shutdown()
{
    m_projectPath.clear();
    m_surfaceAttached = false;
    m_status = {BackendState::Closed, "Editor-Only backend closed."};
}

BackendStatus NullEditorBackend::status() const
{
    return m_status;
}

std::string NullEditorBackend::diagnostic() const
{
    if (m_status.message.empty()) {
        return "Editor-Only backend is active; scene preview and simulation are unavailable.";
    }
    return m_status.message;
}

} // namespace cakery
