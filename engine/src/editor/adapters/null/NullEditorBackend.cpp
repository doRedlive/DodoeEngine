// do@Redlive

#include "NullEditorBackend.h"

namespace cakery {

BackendCapabilities NullEditorBackend::capabilities() const
{
    return {};
}

bool NullEditorBackend::openProject(const ProjectDescriptor& project)
{
    m_projectPath = project.rootPath;
    return true;
}

void NullEditorBackend::requestSceneSurfaceResize(const ViewportMetrics& metrics)
{
    (void)metrics;
}

void NullEditorBackend::detachSceneSurface()
{
}

void NullEditorBackend::tickAtSafePoint()
{
}

void NullEditorBackend::shutdown()
{
    m_projectPath.clear();
}

std::string NullEditorBackend::diagnostic() const
{
    return "Scene preview is unavailable in Editor-Only mode.";
}

} // namespace cakery
