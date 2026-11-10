// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>


class QListWidget;

namespace cakery {

class EditorWorkspaceContext;

class TileLayersPanel final : public QWidget {
    Q_OBJECT
public:
    explicit TileLayersPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~TileLayersPanel() override;

    void refresh();

private:
    void onAddLayer();
    void onRemoveLayer();
    void onMoveLayer(bool up);
    void onActivateLayer(UInt64 uuid);
    void onToggleVisible(UInt64 uuid, bool visible);
    void onOpacityChanged(UInt64 uuid, float opacity);
    void onRenameLayer(UInt64 uuid, const QString& currentName);

    EditorWorkspaceContext& m_context;
    QListWidget* m_list = nullptr;
    Json m_state;
    ScopedConnection m_selectionConnection;
    ScopedConnection m_documentConnection;
    ScopedConnection m_tileModeConnection;
};

} // namespace cakery
