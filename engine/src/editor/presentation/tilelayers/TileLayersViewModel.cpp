// do@Redlive

#include "dopch.h"

#include "TileLayersViewModel.h"

#include "core/bridge/EditorCommands.h"

namespace cakery {

TileLayersViewModel::TileLayersViewModel(EditorSession& session)
    : m_session(session)
{
    m_selectionSubscription = m_session.events().subscribe<SelectionChangedEvent>(
        [this](const SelectionChangedEvent&) { refresh(); });
    m_documentSubscription = m_session.events().subscribe<DocumentChangedEvent>(
        [this](const DocumentChangedEvent&) { refresh(); });
    m_tileModeSubscription = m_session.events().subscribe<TileEditModeChangedEvent>(
        [this](const TileEditModeChangedEvent&) { refresh(); });
    refresh();
}

void TileLayersViewModel::refresh()
{
    TileLayersSnapshot next;
    Json state;
    next.valid = m_session.queryTilemapState(String(), state);
    if (next.valid) {
        next.activeLayer = state.contains("active_layer")
            ? state["active_layer"].get<UInt64>()
            : 0;
        if (state.contains("layers") && state["layers"].is_array()) {
            for (const auto& entry : state["layers"]) {
                TileLayerRow row;
                row.uuid = entry.value("uuid", UInt64(0));
                row.name = entry.value("name", String());
                row.visible = entry.value("visible", true);
                row.opacity = entry.value("opacity", 1.0f);
                next.layers.push_back(std::move(row));
            }
        }
    }
    m_snapshot = std::move(next);
    notifyChanged();
}

void TileLayersViewModel::addLayer(const String& name)
{
    m_session.execute(TilemapLayerAddCommand{name});
}

void TileLayersViewModel::removeLayer(UInt64 uuid)
{
    m_session.execute(TilemapLayerRemoveCommand{uuid});
}

void TileLayersViewModel::moveLayer(UInt64 uuid, bool up)
{
    m_session.execute(TilemapLayerMoveCommand{uuid, up});
}

void TileLayersViewModel::activateLayer(UInt64 uuid)
{
    m_session.execute(TilemapLayerActiveCommand{uuid});
}

void TileLayersViewModel::setLayerVisible(UInt64 uuid, bool visible)
{
    m_session.execute(TilemapLayerVisibleCommand{uuid, visible});
}

void TileLayersViewModel::setLayerOpacity(UInt64 uuid, float opacity)
{
    m_session.execute(TilemapLayerOpacityCommand{uuid, opacity});
}

void TileLayersViewModel::renameLayer(UInt64 uuid, const String& name)
{
    m_session.execute(TilemapLayerRenameCommand{uuid, name});
}

} // namespace cakery
