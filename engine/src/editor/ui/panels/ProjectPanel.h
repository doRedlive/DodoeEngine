// do@Redlive
#pragma once
#include "dopch.h"

#include <QWidget>

#include "core/bridge/EditorTypes.h"
#include "core/Signal.h"

#include <filesystem>
#include <vector>

#include <QHash>
#include <QPixmap>
#include <QStringList>

class QPoint;
class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QComboBox;
class QFileSystemWatcher;
class AssetTreeWidget;
class QLabel;
class QScrollArea;
class QListWidget;
class QListWidgetItem;

namespace cakery {

class EditorWorkspaceContext;
class ProjectViewModel;
struct ProjectTreeNode;

class ProjectPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProjectPanel(EditorWorkspaceContext& context, QWidget* parent = nullptr);
    ~ProjectPanel() override;

    void refresh();

signals:
    void scriptOpenRequested(const QString& path);

private:
    void reloadAssets();
    void onViewModelChanged();
    void addTreeChildren(QTreeWidgetItem* parentItem, const ProjectTreeNode& node);
    bool filterTreeItem(QTreeWidgetItem* item, const QString& filter);
    FsPath selectedDirectory() const;
    QStringList selectedTreePaths() const;
    QStringList selectedGridPaths() const;
    void onContextMenu(const QPoint& pos);
    void onGridContextMenu(const QPoint& pos);
    void openAssetMenu(const QStringList& paths, const QPoint& globalPos);
    void onNewScene();
    void onNewFolder();
    void onCreateScript();
    QString loadScriptTemplate() const;
    void onImportAsset();
    void handleAssetDrop(const QStringList& paths, const QStringList& externalFiles,
                         const QString& targetDir);
    void moveAssetsTo(const QStringList& paths, const FsPath& targetDir);
    void importExternalFiles(const QStringList& files, const FsPath& targetDir);
    void reimportAssets(const QStringList& paths);
    void duplicateAssets(const QStringList& paths);
    void renameAsset(const QString& path);
    void deleteAssets(const QStringList& paths);
    void revealAssets(const QStringList& paths);
    void openAssets(const QStringList& paths);
    bool openScriptIfNeeded(const QString& path);
    void copyAssets(const QStringList& paths);
    void cutAssets(const QStringList& paths);
    void pasteAssets();
    void copyAssetPaths(const QStringList& paths);
    void copyAssetGuid(const QStringList& paths);
    void findAssetReferences(const QStringList& paths);
    void onDocumentDoubleClicked(QTreeWidgetItem* item, int column);
    void updatePreview(QTreeWidgetItem* item);
    void populateAssetGrid(const FsPath& directory);
    void selectTreeAsset(const QString& path);
    QPixmap loadThumbnail(const String& path, const String& type);

    Scope<ProjectViewModel> m_viewModel;
    EditorWorkspaceContext& m_context;
    QTreeWidget* m_tree = nullptr;
    QLabel* m_preview = nullptr;
    QScrollArea* m_previewScroll = nullptr;
    QListWidget* m_assetGrid = nullptr;
    QLineEdit* m_filter = nullptr;
    QComboBox* m_typeFilter = nullptr;
    QFileSystemWatcher* m_watcher = nullptr;
    bool m_refreshPending = false;
    FsPath m_gridDirectory;
    QHash<QString, QPair<qint64, QPixmap>> m_thumbnailCache;
    ScopedConnection m_viewModelSubscription;
};

} // namespace cakery
