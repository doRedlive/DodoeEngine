// do@Redlive

#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace cakery {

struct BackendCapabilities {
    bool documentRead = false;
    bool documentWrite = false;
    bool scenePreview = false;
    bool simulation = false;
};

enum class BackendState {
    Created,
    OpeningProject,
    Ready,
    Degraded,
    Failed,
    Closing,
    Closed
};

struct BackendStatus {
    BackendState state = BackendState::Created;
    std::string message;
};

struct ProjectDescriptor {
    std::string rootPath;
};

struct SceneSurfaceDescriptor {
    std::uintptr_t nativeHandle = 0;
};

struct EditorCommandMessage {
    std::string name;
    std::string payload;
};

struct BackendEventMessage {
    std::string name;
    std::string payload;
};

struct ViewportMetrics {
    int logicalWidth = 0;
    int logicalHeight = 0;
    float devicePixelRatio = 1.0f;
    int pixelWidth = 0;
    int pixelHeight = 0;
    std::uintptr_t nativeHandle = 0;
    std::uint64_t sequence = 0;
};

class IEditorBackend {
public:
    virtual ~IEditorBackend() = default;

    virtual BackendCapabilities capabilities() const = 0;
    virtual bool openProject(const ProjectDescriptor& project) = 0;
    virtual bool openDocument(const std::string& documentId) = 0;
    virtual bool execute(const EditorCommandMessage& command) = 0;
    virtual void setEventCallback(std::function<void(const BackendEventMessage&)> callback) = 0;
    virtual bool attachSceneSurface(const SceneSurfaceDescriptor& surface) = 0;
    virtual void requestSceneSurfaceResize(const ViewportMetrics& metrics) = 0;
    virtual bool detachSceneSurface() = 0;
    virtual void tickAtSafePoint() = 0;
    virtual void shutdown() = 0;
    virtual BackendStatus status() const = 0;
    virtual std::string diagnostic() const = 0;
};

} // namespace cakery
