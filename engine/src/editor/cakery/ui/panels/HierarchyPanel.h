// do@Redlive

#pragma once

#include <QWidget>

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
    void onDeleteEntity();

    EditorWorkspaceContext& m_context;
    QTreeWidget* m_tree = nullptr;
};

} // namespace cakery
