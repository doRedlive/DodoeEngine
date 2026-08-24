// do@Redlive

#pragma once

#include <QWidget>

#include <filesystem>

class QPoint;
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
    void addDirectory(QTreeWidgetItem* parentItem, const std::filesystem::path& directory);
    std::filesystem::path selectedDirectory() const;
    void onContextMenu(const QPoint& pos);
    void onNewScene();
    void onNewFolder();
    void onDocumentDoubleClicked(QTreeWidgetItem* item, int column);

    EditorWorkspaceContext& m_context;
    QTreeWidget* m_tree = nullptr;
    std::filesystem::path m_root;
};

} // namespace cakery
