// do@Redlive
#pragma once
#include "dopch.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>


#include "EditorCommands.h"

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
    String message;
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
    String message;
    String source;
    BackendLogLevel level = BackendLogLevel::Info;
    UInt32 repeatCount = 1;
    UInt64 sequence = 0;
};

struct ProjectDescriptor {
    String rootPath;
    String projectFile;
};

struct SceneSurfaceDescriptor {
    std::uintptr_t nativeHandle = 0;
    int logicalWidth = 0;
    int logicalHeight = 0;
    float devicePixelRatio = 1.0f;
    int pixelWidth = 0;
    int pixelHeight = 0;
};

struct BackendEventMessage {
    String name;
    String payload;
};

struct ViewportMetrics {
    int logicalWidth = 0;
    int logicalHeight = 0;
    float devicePixelRatio = 1.0f;
    int pixelWidth = 0;
    int pixelHeight = 0;
    std::uintptr_t nativeHandle = 0;
    UInt64 sequence = 0;
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
    String name;
    int value = 0;
};

struct InspectorFieldMetadata {
    String name;
    String typeName;
    InspectorFieldKind kind = InspectorFieldKind::Unknown;
    bool hidden = false;
    bool readOnly = false;
    String tooltip;
    bool hasRange = false;
    float rangeMin = 0.0f;
    float rangeMax = 0.0f;
    DynamicArray<InspectorEnumValue> enumValues;
};

struct AssetBrowserEntry {
    UInt64 uuid = 0;
    String path;
    String name;
    String type;
    String extension;
    bool dirty = false;
    DynamicArray<UInt64> dependencies;
};

struct AssetImportSettings {
    String importer;
    Json settings = Json::object();
};

} // namespace cakery
