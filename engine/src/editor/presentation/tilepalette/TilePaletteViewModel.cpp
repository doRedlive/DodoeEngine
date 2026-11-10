// do@Redlive

#include "dopch.h"

#include "TilePaletteViewModel.h"

#include <algorithm>
#include <filesystem>

namespace cakery {

TilePaletteViewModel::TilePaletteViewModel(EditorSession& session)
    : m_session(session)
{
    m_selectionSubscription = m_session.selection().subscribe([this]() { refresh(); });
    m_documentSubscription = m_session.documentModel().subscribe([this]() { refresh(); });
    m_tileModeSubscription = ScopedConnection(
        m_session.tileEditModeChanged,
        m_session.tileEditModeChanged.connect([this](bool) { refresh(); }));
    refresh();
}

bool TilePaletteViewModel::hasTilesets() const
{
    return m_state.contains("tilesets") && m_state["tilesets"].is_array() &&
           !m_state["tilesets"].empty();
}

UInt64 TilePaletteViewModel::activeTilemapUuid() const
{
    return m_state.contains("tilemap") ? m_state["tilemap"].value("uuid", UInt64(0)) : 0;
}

DynamicArray<TilemapTargetOption> TilePaletteViewModel::tilemapTargets() const
{
    DynamicArray<TilemapTargetOption> targets;
    for (const EditorEntity& entity : m_session.documentModel().entities()) {
        const bool isTilemap = std::any_of(
            entity.nativeComponents.begin(), entity.nativeComponents.end(),
            [](const EditorComponent& component) { return component.typeName == "TilemapComponent"; });
        if (!isTilemap) {
            continue;
        }
        targets.push_back({entity.uuid, entity.name});
    }
    return targets;
}

DynamicArray<String> TilePaletteViewModel::tilesetNames() const
{
    DynamicArray<String> names;
    if (!m_state.contains("tilesets") || !m_state["tilesets"].is_array()) {
        return names;
    }
    for (const auto& entry : m_state["tilesets"]) {
        names.push_back(entry.value("name", String()));
    }
    return names;
}

UInt64 TilePaletteViewModel::tilesetAssetId(const String& name) const
{
    if (!m_state.contains("tilesets") || !m_state["tilesets"].is_array()) {
        return 0;
    }
    for (const auto& entry : m_state["tilesets"]) {
        if (entry.value("name", String()) == name) {
            return entry.value("asset_id", UInt64(0));
        }
    }
    return 0;
}

FsPath TilePaletteViewModel::resolveImagePath(const String& url) const
{
    if (url.empty()) {
        return {};
    }
    FsPath path(url);
    if (path.is_absolute()) {
        return path;
    }
    const FsPath assetRoot = m_session.assetRoot();
    if (!assetRoot.empty()) {
        const FsPath candidate = assetRoot / url;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    const FsPath project(m_session.project().rootPath);
    if (!project.empty()) {
        const FsPath candidate = project / url;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return path;
}

void TilePaletteViewModel::refresh()
{
    Json state;
    if (m_session.queryTilemapState(String(), state)) {
        m_state = std::move(state);
    } else {
        m_state = Json();
    }
    notifyChanged();
}

void TilePaletteViewModel::selectTool(TileToolId tool)
{
    m_session.execute(TilemapToolCommand{tool});
}

void TilePaletteViewModel::activateTarget(UInt64 uuid)
{
    m_session.execute(TilemapActivateCommand{uuid});
}

void TilePaletteViewModel::executeBrush(const EditorOperation& command)
{
    m_session.execute(command);
}

void TilePaletteViewModel::setBrush(int width, int height, DynamicArray<UInt32> gids)
{
    m_session.execute(TilemapBrushCommand{width, height, std::move(gids)});
}

void TilePaletteViewModel::flipBrush(bool horizontally)
{
    m_session.execute(TilemapBrushFlipCommand{horizontally});
}

void TilePaletteViewModel::rotateBrush()
{
    m_session.execute(TilemapBrushRotateCommand{});
}

void TilePaletteViewModel::setRandomBrush(bool enabled)
{
    m_session.execute(TilemapRandomBrushCommand{enabled});
}

void TilePaletteViewModel::deleteSelection()
{
    m_session.execute(TilemapSelectionDeleteCommand{});
}

void TilePaletteViewModel::copySelection()
{
    m_session.execute(TilemapSelectionCopyCommand{});
}

void TilePaletteViewModel::pasteSelection()
{
    m_session.execute(TilemapSelectionPasteCommand{});
}

void TilePaletteViewModel::createTilemap(const TilemapCreateCommand& command)
{
    m_session.execute(command);
}

void TilePaletteViewModel::addTileset(const TilemapAddTilesetCommand& command)
{
    m_session.execute(command);
}

void TilePaletteViewModel::editTileset(const TilemapEditTilesetCommand& command)
{
    m_session.execute(command);
}

void TilePaletteViewModel::removeTileset(UInt64 assetId)
{
    m_session.execute(TilemapRemoveTilesetCommand{assetId});
}

void TilePaletteViewModel::resizeTilemap(UInt64 uuid, UInt32 width, UInt32 height)
{
    m_session.execute(TilemapResizeCommand{uuid, width, height});
}

} // namespace cakery
