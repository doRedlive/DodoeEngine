// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>


class QListWidget;

namespace cakery {

class EditorWorkspaceContext;
class TileLayersViewModel;

class TileLayersPanel final : public QWidget {
    Q_OBJECT
public:
    explicit TileLayersPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~TileLayersPanel() override;

    void refresh();

private:
    void onViewModelChanged();
    void onAddLayer();
    void onRemoveLayer();
    void onMoveLayer(bool up);
    void onActivateLayer(UInt64 uuid);
    void onToggleVisible(UInt64 uuid, bool visible);
    void onOpacityChanged(UInt64 uuid, float opacity);
    void onRenameLayer(UInt64 uuid, const QString& currentName);

    EditorWorkspaceContext& m_context;
    Scope<TileLayersViewModel> m_viewModel;
    ScopedConnection m_viewModelConnection;
    QListWidget* m_list = nullptr;
};

} // namespace cakery
