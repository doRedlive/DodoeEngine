// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/Signal.h"

#include <cstdint>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace cakery {

class EditorWorkspaceContext;
class HierarchyViewModel;

class HierarchyPanel : public QWidget {
    Q_OBJECT
public:
    explicit HierarchyPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~HierarchyPanel() override;

signals:
    void newTilemapRequested();

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
    void onCreateChildEntity(UInt64 parentUuid);
    void onDeleteEntity();
    void onMoveToRoot();
    void onRenameSelected();
    void onCopyEntities();
    void onCutEntities();
    void onPasteEntities();
    void onDuplicateEntities();
    void onSaveAsPrefab();
    void onReparentEntity(UInt64 uuid, UInt64 newParent);

    Scope<HierarchyViewModel> m_viewModel;
    QLineEdit* m_filter = nullptr;
    QTreeWidget* m_tree = nullptr;
    ScopedConnection m_viewModelSubscription;
    ScopedConnection m_selectionSubscription;
};

} // namespace cakery
