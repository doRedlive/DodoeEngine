// do@Redlive

#include "NullEditorBackend.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

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
    m_project = project;
    m_status = {BackendState::OpeningProject, "Opening Editor-Only project."};
    if (project.rootPath.empty() || !std::filesystem::is_directory(project.rootPath)) {
        m_status = {BackendState::Failed, "Project directory does not exist."};
        return false;
    }
    m_projectPath = project.rootPath;
    m_status = {BackendState::Ready, "Editor-Only project opened. Authoring is available; scene preview and simulation are unavailable."};
    return true;
}

std::string NullEditorBackend::startScenePath() const
{
    std::filesystem::path projectFile(m_project.projectFile);
    const std::filesystem::path projectRoot(m_project.rootPath);
    if (projectFile.empty() && std::filesystem::is_directory(projectRoot)) {
        for (const auto& entry : std::filesystem::directory_iterator(projectRoot)) {
            if (entry.is_regular_file() && entry.path().extension().string() == ".doproj") {
                projectFile = entry.path();
                break;
            }
        }
    }
    if (projectFile.empty() || !std::filesystem::is_regular_file(projectFile)) {
        return {};
    }
    std::ifstream fin(projectFile);
    if (!fin.is_open()) {
        return {};
    }
    nlohmann::json data;
    try {
        fin >> data;
    } catch (const nlohmann::json::exception&) {
        return {};
    }
    if (!data.contains("Project") || !data["Project"].is_object()) {
        return {};
    }
    const auto& project_node = data["Project"];
    if (!project_node.contains("StartSceneName") || !project_node["StartSceneName"].is_string()) {
        return {};
    }
    const std::string asset_directory = project_node.contains("AssetDirectory") && project_node["AssetDirectory"].is_string()
        ? project_node["AssetDirectory"].get<std::string>()
        : std::string("Assets");
    const std::string scene_name = project_node["StartSceneName"].get<std::string>();
    return (projectRoot / asset_directory / "Scenes" / (scene_name + ".doscn")).string();
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
