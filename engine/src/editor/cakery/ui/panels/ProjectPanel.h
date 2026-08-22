// do@Redlive

#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;

namespace cakery {

class EditorWorkspaceContext;

class ProjectPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProjectPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);

    void refresh();

private:
    void onDocumentDoubleClicked(QTreeWidgetItem* item, int column);

    EditorWorkspaceContext& m_context;
    QTreeWidget* m_tree = nullptr;
};

} // namespace cakery
