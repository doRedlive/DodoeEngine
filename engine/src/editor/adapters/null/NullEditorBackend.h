// do@Redlive

#pragma once

#include "bridge/EditorBackend.h"

namespace cakery {

class NullEditorBackend final : public IEditorBackend {
public:
    BackendCapabilities capabilities() const override;
    bool openProject(const ProjectDescriptor& project) override;
    bool openDocument(const std::string& documentId) override;
    std::string startScenePath() const override;
    bool execute(const EditorCommandMessage& command) override;
    void setEventCallback(std::function<void(const BackendEventMessage&)>) override;
    bool attachSceneSurface(const SceneSurfaceDescriptor& surface) override;
    void requestSceneSurfaceResize(const ViewportMetrics& metrics) override;
    bool detachSceneSurface() override;
    void tickAtSafePoint() override;
    void shutdown() override;
    BackendStatus status() const override;
    std::string diagnostic() const override;

private:
    std::string m_projectPath;
    ProjectDescriptor m_project;
    BackendStatus m_status;
    bool m_surfaceAttached = false;
};

} // namespace cakery
