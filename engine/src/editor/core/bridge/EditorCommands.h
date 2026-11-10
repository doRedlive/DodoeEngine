// do@Redlive

#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>


#include "dopch.h"

namespace cakery {

enum class GizmoMode {
    None,
    Translate,
    Rotate,
    Scale
};

enum class PlayAction {
    Play,
    Pause,
    Resume,
    Stop
};

enum class TileToolId {
    Select,
    Brush,
    Erase,
    Fill,
    Rect,
    Line,
    Picker
};

struct DocumentChangedCommand {
    Json snapshot;
};

struct SceneMouseDownCommand {
    float x = 0.0f;
    float y = 0.0f;
    int button = 0;
    bool alt = false;
    bool ctrl = false;
    bool shift = false;
};

struct SceneMouseMoveCommand {
    float x = 0.0f;
    float y = 0.0f;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct SceneMouseUpCommand {
    int button = 0;
};

struct SceneMouseWheelCommand {
    float delta = 0.0f;
};

struct SceneKeyCommand {
    int key = 0;
    bool down = false;
};

struct SelectionChangedCommand {
    UInt64 uuid = 0;
};

struct GizmoModeCommand {
    GizmoMode mode = GizmoMode::None;
};

struct GizmoSnapCommand {
    bool enabled = false;
};

struct GizmoSnapStepCommand {
    float translate = 0.25f;
    float rotate = 15.0f;
    float scale = 0.1f;
};

struct CameraModeCommand {
    bool ortho2d = false;
};

struct SceneImportAssetCommand {
    float x = 0.0f;
    float y = 0.0f;
    DynamicArray<String> paths;
};

struct PrefabExportCommand {
    UInt64 uuid = 0;
    String path;
};

struct PlayActionCommand {
    PlayAction action = PlayAction::Play;
};

struct AssetSaveAllCommand {};

struct AssetRefreshCommand {};

struct AssetImportCommand {
    String sourcePath;
};

struct AssetReimportCommand {
    String path;
};

struct ScriptToolActionCommand {
    String path;
};

struct AssetUpdateSettingsCommand {
    String path;
    Json settings = Json::object();
};

struct TilemapCreateCommand {
    String name = "Tilemap";
    UInt32 width = 20;
    UInt32 height = 20;
    UInt32 tileWidth = 16;
    UInt32 tileHeight = 16;
};

struct TilemapActivateCommand {
    UInt64 uuid = 0;
};

struct TilemapToolCommand {
    TileToolId tool = TileToolId::Select;
};

struct TilemapBrushFlipCommand {
    bool horizontal = true;
};

struct TilemapBrushRotateCommand {};

struct TilemapRandomBrushCommand {
    bool enabled = false;
};

struct TilemapSelectionDeleteCommand {};

struct TilemapSelectionCopyCommand {};

struct TilemapSelectionPasteCommand {};

struct TilemapBrushCommand {
    int width = 1;
    int height = 1;
    DynamicArray<UInt32> gids;
};

struct TilemapEditTilesetCommand {
    UInt64 assetId = 0;
    UInt32 tileWidth = 16;
    UInt32 tileHeight = 16;
    UInt32 margin = 0;
    UInt32 spacing = 0;
};

struct TilemapAddTilesetCommand {
    String image;
    UInt32 tileWidth = 16;
    UInt32 tileHeight = 16;
    UInt32 margin = 0;
    UInt32 spacing = 0;
};

struct TilemapRemoveTilesetCommand {
    UInt64 assetId = 0;
};

struct TilemapResizeCommand {
    UInt64 uuid = 0;
    UInt32 width = 0;
    UInt32 height = 0;
};

struct TilemapLayerAddCommand {
    String name = "Layer";
};

struct TilemapLayerRemoveCommand {
    UInt64 uuid = 0;
};

struct TilemapLayerMoveCommand {
    UInt64 uuid = 0;
    bool up = false;
};

struct TilemapLayerActiveCommand {
    UInt64 uuid = 0;
};

struct TilemapLayerVisibleCommand {
    UInt64 uuid = 0;
    bool visible = true;
};

struct TilemapLayerOpacityCommand {
    UInt64 uuid = 0;
    float opacity = 1.0f;
};

struct TilemapLayerRenameCommand {
    UInt64 uuid = 0;
    String name;
};

using EditorOperation = std::variant<
    DocumentChangedCommand,
    SceneMouseDownCommand,
    SceneMouseMoveCommand,
    SceneMouseUpCommand,
    SceneMouseWheelCommand,
    SceneKeyCommand,
    SelectionChangedCommand,
    GizmoModeCommand,
    GizmoSnapCommand,
    GizmoSnapStepCommand,
    CameraModeCommand,
    SceneImportAssetCommand,
    PrefabExportCommand,
    PlayActionCommand,
    AssetSaveAllCommand,
    AssetRefreshCommand,
    AssetImportCommand,
    AssetReimportCommand,
    ScriptToolActionCommand,
    AssetUpdateSettingsCommand,
    TilemapCreateCommand,
    TilemapActivateCommand,
    TilemapToolCommand,
    TilemapBrushFlipCommand,
    TilemapBrushRotateCommand,
    TilemapRandomBrushCommand,
    TilemapSelectionDeleteCommand,
    TilemapSelectionCopyCommand,
    TilemapSelectionPasteCommand,
    TilemapBrushCommand,
    TilemapEditTilesetCommand,
    TilemapAddTilesetCommand,
    TilemapRemoveTilesetCommand,
    TilemapResizeCommand,
    TilemapLayerAddCommand,
    TilemapLayerRemoveCommand,
    TilemapLayerMoveCommand,
    TilemapLayerActiveCommand,
    TilemapLayerVisibleCommand,
    TilemapLayerOpacityCommand,
    TilemapLayerRenameCommand>;

const char* gizmoModeName(GizmoMode mode);
GizmoMode gizmoModeFromName(StringView name);

} // namespace cakery
