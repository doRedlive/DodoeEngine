// do@Redlive
#pragma once
#include "dopch.h"

#include "core/EditorSession.h"
#include "presentation/ViewModel.h"

namespace cakery {

struct TileLayerRow {
    UInt64 uuid = 0;
    String name;
    bool visible = true;
    float opacity = 1.0f;
};

struct TileLayersSnapshot {
    bool valid = false;
    UInt64 activeLayer = 0;
    DynamicArray<TileLayerRow> layers;
};

class TileLayersViewModel : public ViewModel {
public:
    explicit TileLayersViewModel(EditorSession& session);

    const TileLayersSnapshot& snapshot() const { return m_snapshot; }

    void refresh();
    void addLayer(const String& name);
    void removeLayer(UInt64 uuid);
    void moveLayer(UInt64 uuid, bool up);
    void activateLayer(UInt64 uuid);
    void setLayerVisible(UInt64 uuid, bool visible);
    void setLayerOpacity(UInt64 uuid, float opacity);
    void renameLayer(UInt64 uuid, const String& name);

private:
    EditorSession& m_session;
    TileLayersSnapshot m_snapshot;
    ScopedConnection m_selectionSubscription;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_tileModeSubscription;
};

} // namespace cakery
