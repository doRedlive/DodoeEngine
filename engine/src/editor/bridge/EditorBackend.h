// do@Redlive

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace cakery {

class EditorSession;

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

enum class BackendLogLevel {
    Trace,
    Debug,
    Info,
    Warning,
    Error,
    Critical,
};

struct BackendLogEntry {
    std::string message;
    std::string source;
    BackendLogLevel level = BackendLogLevel::Info;
    std::uint32_t repeatCount = 1;
    std::uint64_t sequence = 0;
};

struct ProjectDescriptor {
    std::string rootPath;
    std::string projectFile;
};

struct SceneSurfaceDescriptor {
    std::uintptr_t nativeHandle = 0;
    int logicalWidth = 0;
    int logicalHeight = 0;
    float devicePixelRatio = 1.0f;
    int pixelWidth = 0;
    int pixelHeight = 0;
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

enum class InspectorFieldKind {
    Unknown,
    Bool,
    Integer,
    UnsignedInteger,
    Float,
    Double,
    String,
    Enum,
    Vector,
    Color,
    Struct,
    Array,
    AssetHandle,
    UUID
};

struct InspectorEnumValue {
    std::string name;
    int value = 0;
};

struct InspectorFieldMetadata {
    std::string name;
    std::string typeName;
    InspectorFieldKind kind = InspectorFieldKind::Unknown;
    bool hidden = false;
    bool readOnly = false;
    std::string tooltip;
    bool hasRange = false;
    float rangeMin = 0.0f;
    float rangeMax = 0.0f;
    std::vector<InspectorEnumValue> enumValues;
};

struct AssetBrowserEntry {
    std::uint64_t uuid = 0;
    std::string path;
    std::string name;
    std::string type;
    std::string extension;
    bool dirty = false;
    std::vector<std::uint64_t> dependencies;
};

struct AssetImportSettings {
    std::string importer;
    nlohmann::json settings = nlohmann::json::object();
};

class IEditorBackend {
public:
    virtual ~IEditorBackend() = default;

    virtual BackendCapabilities capabilities() const = 0;
    virtual bool openProject(const ProjectDescriptor& project) = 0;
    virtual bool openDocument(const std::string& documentId) = 0;
    virtual std::string startScenePath() const = 0;
    virtual bool execute(const EditorCommandMessage& command) = 0;
    // Returns reflected field metadata when the backend has a native type registry.
    // Callers must keep JSON values as the document source of truth.
    virtual bool inspectComponent(const std::string&, std::vector<InspectorFieldMetadata>&) const { return false; }
    virtual bool listAssets(std::vector<AssetBrowserEntry>& entries) const {
        entries.clear();
        return false;
    }
    virtual bool getAssetImportSettings(const std::string&, AssetImportSettings&) const { return false; }
    virtual bool listLogs(std::vector<BackendLogEntry>& entries) const {
        entries.clear();
        return false;
    }
    virtual bool clearLogs() { return false; }
    virtual bool listToolActions(std::vector<std::string>& actions) const {
        actions.clear();
        return false;
    }
    virtual bool invokeToolAction(const std::string& path) { return false; }
    virtual void setEditorSession(EditorSession* /*session*/) {}
    virtual bool queryTilemapState(const std::string& /*tilemapUuid*/, nlohmann::json& out) const {
        out = nullptr;
        return false;
    }
    // Returns {width, height, data(base64 RGBA)} for a cached asset thumbnail,
    // or false when the backend has no generator for the asset type.
    virtual bool queryAssetThumbnail(const std::string& /*path*/, int /*size*/, nlohmann::json& out) const {
        out = nullptr;
        return false;
    }
    virtual void setEventCallback(std::function<void(const BackendEventMessage&)> callback) = 0;
    virtual bool assetRefreshPending() const { return false; }
    virtual void assetRefreshProgress(std::size_t& done, std::size_t& total) const {
        done = 0;
        total = 0;
    }
    virtual bool attachSceneSurface(const SceneSurfaceDescriptor& surface) = 0;
    virtual void requestSceneSurfaceResize(const ViewportMetrics& metrics) = 0;
    virtual bool detachSceneSurface() = 0;
    virtual void tickAtSafePoint() = 0;
    virtual void shutdown() = 0;
    virtual BackendStatus status() const = 0;
    virtual std::string diagnostic() const = 0;
};

} // namespace cakery
