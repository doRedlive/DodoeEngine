// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>

class QTreeWidget;
class QTreeWidgetItem;

namespace cakery {

class EditorWorkspaceContext;

class HierarchyPanel : public QWidget {
    Q_OBJECT
public:
    explicit HierarchyPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

private:
    void refresh();
    void refreshSelection();
    void onTreeSelectionChanged();
    void onItemEdited(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onCreateEntity();
    void onCreateChildEntity(std::uint64_t parentUuid);
    void onDeleteEntity();
    void onMoveToRoot();
    void onReparentEntity(std::uint64_t uuid, std::uint64_t newParent);

    EditorWorkspaceContext& m_context;
    QTreeWidget* m_tree = nullptr;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_selectionSubscription;
};

} // namespace cakery
