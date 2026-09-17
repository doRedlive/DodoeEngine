// do@Redlive

#pragma once

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>

class QLineEdit;
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
    void applyFilter();
    bool filterItem(QTreeWidgetItem* item, const QString& needle);
    void onTreeSelectionChanged();
    void onSelectAll();
    void onItemEdited(QTreeWidgetItem* item, int column);
    void onContextMenu(const QPoint& pos);
    void onCreateEntity();
    void onCreateChildEntity(std::uint64_t parentUuid);
    void onDeleteEntity();
    void onMoveToRoot();
    void onRenameSelected();
    void onCopyEntities();
    void onCutEntities();
    void onPasteEntities();
    void onDuplicateEntities();
    void onSaveAsPrefab();
    void onReparentEntity(std::uint64_t uuid, std::uint64_t newParent);

    EditorWorkspaceContext& m_context;
    QLineEdit* m_filter = nullptr;
    QTreeWidget* m_tree = nullptr;
    ScopedConnection m_documentSubscription;
    ScopedConnection m_selectionSubscription;
};

} // namespace cakery
