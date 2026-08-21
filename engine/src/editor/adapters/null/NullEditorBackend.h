// do@Redlive

#pragma once

#include "bridge/EditorBackend.h"

namespace cakery {

class NullEditorBackend final : public IEditorBackend {
public:
    BackendCapabilities capabilities() const override;
    bool openProject(const ProjectDescriptor& project) override;
    void requestSceneSurfaceResize(const ViewportMetrics& metrics) override;
    void detachSceneSurface() override;
    void tickAtSafePoint() override;
    void shutdown() override;
    std::string diagnostic() const override;

private:
    std::string m_projectPath;
};

} // namespace cakery
