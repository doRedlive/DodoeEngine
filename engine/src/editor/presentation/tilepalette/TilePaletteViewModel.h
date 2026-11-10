// do@Redlive
#pragma once
#include "dopch.h"

#include "core/EditorSession.h"
#include "core/bridge/EditorCommands.h"
#include "presentation/ViewModel.h"

namespace cakery {

struct TilemapTargetOption {
    UInt64 uuid = 0;
    String name;
};

class TilePaletteViewModel : public ViewModel {
public:
    explicit TilePaletteViewModel(EditorSession& session);

    const Json& tilemapState() const { return m_state; }
    bool hasTilemap() const { return m_state.contains("tilemap"); }
    bool hasTilesets() const;
    UInt64 activeTilemapUuid() const;
    DynamicArray<TilemapTargetOption> tilemapTargets() const;
    DynamicArray<String> tilesetNames() const;
    UInt64 tilesetAssetId(const String& name) const;
    FsPath resolveImagePath(const String& url) const;

    void refresh();
    void selectTool(TileToolId tool);
    void activateTarget(UInt64 uuid);
    void executeBrush(const EditorOperation& command);
    void setBrush(int width, int height, DynamicArray<UInt32> gids);
    void flipBrush(bool horizontally);
    void rotateBrush();
    void setRandomBrush(bool enabled);
    void deleteSelection();
    void copySelection();
    void pasteSelection();
    void createTilemap(const TilemapCreateCommand& command);
    void addTileset(const TilemapAddTilesetCommand& command);
    void editTileset(const TilemapEditTilesetCommand& command);
    void removeTileset(UInt64 assetId);
    void resizeTilemap(UInt64 uuid, UInt32 width, UInt32 height);

private:
    EditorSession& m_session;
    Json m_state;
    ScopedConnection m_selectionSubscription;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_tileModeSubscription;
};

} // namespace cakery
