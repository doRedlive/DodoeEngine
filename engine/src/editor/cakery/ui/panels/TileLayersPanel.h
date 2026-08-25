// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>

#include <nlohmann/json.hpp>

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
    void onActivateLayer(std::uint64_t uuid);
    void onToggleVisible(std::uint64_t uuid, bool visible);
    void onOpacityChanged(std::uint64_t uuid, float opacity);
    void onRenameLayer(std::uint64_t uuid, const QString& currentName);

    EditorWorkspaceContext& m_context;
    QListWidget* m_list = nullptr;
    nlohmann::json m_state;
    ScopedConnection m_selectionConnection;
    ScopedConnection m_documentConnection;
    ScopedConnection m_tileModeConnection;
};

} // namespace cakery
