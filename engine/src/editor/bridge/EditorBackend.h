// do@Redlive

#pragma once

#include <cstdint>
#include <string>

namespace cakery {

struct BackendCapabilities {
    bool documentRead = false;
    bool documentWrite = false;
    bool scenePreview = false;
    bool simulation = false;
};

struct ProjectDescriptor {
    std::string rootPath;
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
    virtual void requestSceneSurfaceResize(const ViewportMetrics& metrics) = 0;
    virtual void detachSceneSurface() = 0;
    virtual void tickAtSafePoint() = 0;
    virtual void shutdown() = 0;
    virtual std::string diagnostic() const = 0;
};

} // namespace cakery
